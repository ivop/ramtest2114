// ramtest2114 - 2114 RAM tester
//
// MARCH (M)SS Algorithm
//
// Copyright © 2025 by Ivo van Poorten
// BSD 2-Clause License
// See LICENSE file for details
//
// MARCH MSS = { 🡙(w0); 🡑(r0,r0,w1,w1); 🡑(r1,r1,w0,w0); 🡓(r0,r0,w1,w1);
//               🡓(r1,r1,w0,w0); 🡙(r0) }
//
// Arduino Nano V3 (ATmega328 16MHz, 32kB Flash, 1kB ROM, 2kB RAM)
// Hardware similar to:
// https://github.com/gpimblott/SRAM2114_Tester
//
// PD2 = /CS
// PD3 = /WE
//
// PD4-PD7 = A0-A3
// PB0-PB5 = A4-A9
//
// PC0-PC3 = D0-D3
//
// PC4 green LED
// PC5 red LED


#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

enum pins {
    PIN_CS    = 0b00000100,
    PIN_WE    = 0b00001000,
    PINS_DB   = 0b00001111,
    PINS_A0A3 = 0b11110000,
    PINS_A4A9 = 0b00111111,
    PIN_LEDG  = 0b00010000,
    PIN_LEDR  = 0b00100000,
    PINS_LEDS = 0b00110000
};

enum shifts {
    SHIFT_A0A3 = 4,
    SHIFT_A4A9 = 0
};

// ----------------------------------------------------------------------------

static void hw_usart_init(unsigned int ubrr) {
    UBRR0H = ubrr>>8;
    UBRR0L = ubrr;
    UCSR0B = 0;                             // disable all
    UCSR0B = _BV(RXEN0)  | _BV(TXEN0);      // enable RX and TX
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);     // 8N1
//    UCSR0A = _BV(U2X0);                     // Double Speed
}

static void hw_usart_tx_byte(uint8_t byte) {
    while(!(UCSR0A & _BV(UDRE0))) ;         // wait for port ready to write
    UDR0 = byte;
}

static uint8_t hw_usart_rx_byte(void) {
    while(!(UCSR0A & _BV(RXC0))) ;          // wait for byte received
    return UDR0;
}

static void hw_usart_tx_block(const void *const buf, uint16_t len) {
    const uint8_t *b = buf;
    while (len--)
        hw_usart_tx_byte(*b++);
}

static void hw_usart_rx_block(void *const buf, uint16_t len) {
    uint8_t *b = buf;
    while (len--)
        *b++ = hw_usart_rx_byte();
}

static void hw_usart_tx_string(char *s) {
    while(*s) hw_usart_tx_byte(*s++);
}

// ----------------------------------------------------------------------------

static void led_green_on(void)  { PORTC |=  PIN_LEDG; };
static void led_green_off(void) { PORTC &= ~PIN_LEDG; };
static void led_red_on(void)    { PORTC |=  PIN_LEDR; };
static void led_red_off(void)   { PORTC &= ~PIN_LEDR; };

static inline void enable_cs(void)  { PORTD &= ~PIN_CS; }
static inline void disable_cs(void) { PORTD |=  PIN_CS; }
static inline void enable_we(void)  { PORTD &= ~PIN_WE; }
static inline void disable_we(void) { PORTD |=  PIN_WE; }

static inline void set_databus_output(void) { DDRC |=  PINS_DB; }
static inline void set_databus_input(void)  { DDRC &= ~PINS_DB; }

static void put_address_bus(uint16_t a) {
    PORTD &= ~PINS_A0A3;
    PORTD |= (a & 0x0f) << SHIFT_A0A3;
    PORTB &= ~PINS_A4A9;
    PORTB |= (a >> 4) << SHIFT_A4A9;
}

static void put_databus(uint8_t v) {
    PORTC &= ~PINS_DB;
    PORTC |= v & 0x0f;
}

static uint8_t get_databus(void) {
    return PINC & PINS_DB;
}

static uint8_t ram_read(uint16_t a) {
    put_address_bus(a);

    enable_cs();
    _delay_us(1);
    uint8_t v = get_databus();
    disable_cs();

    return v;
}

static void ram_write(uint16_t a, uint8_t v) {
    put_address_bus(a);

    set_databus_output();
    put_databus(v);

    enable_cs();
    enable_we();
    _delay_us(1);
    disable_we();
    disable_cs();

    set_databus_input();
}

// ----------------------------------------------------------------------------

static void fail(uint16_t a, uint8_t v, uint8_t e, int w) {
    hw_usart_tx_string("FAIL!\r\n");
    char s[128];
    snprintf(s, 128, "a=%03x, v=%1x, expected=%1x, line=%d\r\n", a, v, e, w);
    hw_usart_tx_string(s);
    led_green_off();
    led_red_on();
    while(1) ;
}

// ----------------------------------------------------------------------------

int main(void) {
    uint8_t updown = 0;         // bit 0 🡙(w0), bit 1 🡙(r0)
    uint8_t zero = 0b0000;
    uint8_t one = 0b1111;
    int a;
    uint8_t v;
    uint8_t bg_zero[3] = { 0b0000, 0b0011, 0b0101 };
    uint8_t bg_one[3]  = { 0b1111, 0b1100, 0b1010 };
    uint8_t bgidx = 0;

    hw_usart_init(103);         // 9600 baud, debug console

    DDRD |= 0b11111100;         // PD2-PD7 output   (/CS, /WE, A0-A3)
    DDRB |= 0b00111111;         // PB0-PB5 output   (A4-A9)
    DDRC |= PINS_LEDS;          // PC4-PC5 LEDs

    hw_usart_tx_string("2114 SRAM MARCH MSS TESTER\r\n");

    while (1) {

        if (!(updown &1)) {
            led_green_on();
            led_red_on();
        } else {
            led_green_off();
            led_red_off();
        }

        zero = bg_zero[bgidx];
        one  = bg_one[bgidx];

        // 🡙(w0)

        if (!(updown & 1)) {
            hw_usart_tx_string("^(w0)\r\n");
            for (a=0; a<1024; a++) {
                ram_write(a, zero);
            }
        } else {
            hw_usart_tx_string("v(w0)\r\n");
            for (a=1023; a>=0; a--) {
                ram_write(a, zero);
            }
        }

        // 🡑(r0,r0,w1,w1)

        hw_usart_tx_string("^(r0,r0,w1,w1)\r\n");
        for (a=0; a<1024; a++) {
            v = ram_read(a);
            if (v != zero) fail(a,v,zero,__LINE__);
            v = ram_read(a);
            if (v != zero) fail(a,v,zero,__LINE__);
            ram_write(a, one);
            ram_write(a, one);
        }

        // 🡑(r1,r1,w0,w0)

        hw_usart_tx_string("^(r1,r1,w0,w0)\r\n");
        for (int a=0; a<1024; a++) {
            v = ram_read(a);
            if (v != one) fail(a,v,one,__LINE__);
            v = ram_read(a);
            if (v != one) fail(a,v,one,__LINE__);
            ram_write(a, zero);
            ram_write(a, zero);
        }

        // 🡓(r0,r0,w1,w1)

        hw_usart_tx_string("v(r0,r0,w1,w1)\r\n");
        for (a=1023; a>=0; a--) {
            v = ram_read(a);
            if (v != zero) fail(a,v,zero,__LINE__);
            v = ram_read(a);
            if (v != zero) fail(a,v,zero,__LINE__);
            ram_write(a, one);
            ram_write(a, one);
        }

        // 🡓(r1,r1,w0,w0)

        hw_usart_tx_string("v(r1,r1,w0,w0)\r\n");
        for (int a=1023; a>=0; a--) {
            v = ram_read(a);
            if (v != one) fail(a,v,one,__LINE__);
            v = ram_read(a);
            if (v != one) fail(a,v,one,__LINE__);
            ram_write(a, zero);
            ram_write(a, zero);
        }

        // 🡙(r0)

        if (!(updown & 2)) {
            hw_usart_tx_string("^(r0)\r\n");
            for (a=0; a<1024; a++) {
                v = ram_read(a);
                if (v != zero) fail(a,v,zero,__LINE__);
            }
        } else {
            hw_usart_tx_string("v(r0)\r\n");
            for (a=1023; a>=0; a--) {
                v = ram_read(a);
                if (v != zero) fail(a,v,zero,__LINE__);
            }
        }

        updown++;
        if (updown == 4) {
            updown = 0;
            bgidx++;
            if (bgidx == 3) {
                hw_usart_tx_string("RAM TESTED OK!\r\n");
                led_green_on();
                led_red_off();
                while(1);
            }
        }
    }
}
