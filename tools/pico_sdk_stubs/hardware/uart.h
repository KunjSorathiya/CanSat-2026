#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstdint>

struct uart_inst_t;
extern uart_inst_t* uart0;
extern uart_inst_t* uart1;

enum uart_parity_t { UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD };

extern "C" {
std::uint32_t uart_init(uart_inst_t* uart, std::uint32_t baudrate);
void uart_set_hw_flow(uart_inst_t* uart, bool cts, bool rts);
void uart_set_format(uart_inst_t* uart, std::uint32_t data_bits, std::uint32_t stop_bits,
                     uart_parity_t parity);
void uart_set_fifo_enabled(uart_inst_t* uart, bool enabled);
bool uart_is_readable(uart_inst_t* uart);
std::uint8_t uart_getc(uart_inst_t* uart);
void uart_putc_raw(uart_inst_t* uart, char c);
bool uart_is_writable(uart_inst_t* uart);
}
