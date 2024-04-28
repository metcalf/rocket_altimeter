#pragma once

#include "avr/io.h"

#ifdef USART_DEBUG
#define USART_DEBUG_INIT() usart_debug_init()
#define USART_DEBUG_SEND(c) usart_debug_send(c)
#else
#define USART_DEBUG_INIT()
#define USART_DEBUG_SEND(c)
#endif

void usart_debug_init() {
    PORTMUX.USARTROUTEA |= PORTMUX_USART1_ALT1_gc;
    VPORTC.DIR |= PIN2_bm; // TxD on PC2
    USART1.BAUD = (uint16_t)((float)(F_CLK_PER * 64 / (16 * (float)9600)) + 0.5);
    USART1.CTRLB = USART_TXEN_bm; // TX only
}

void usart_debug_send(char byte) {
    while (!(USART1.STATUS & USART_DREIF_bm)) {
        ;
    }
    USART1_TXDATAL = byte;
}
