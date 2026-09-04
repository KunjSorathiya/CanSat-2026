#include "flight/pico/neo6m.hpp"

#ifdef PICO_BUILD
#include "hardware/gpio.h"
#include "hardware/uart.h"
#endif

namespace flight::pico {

#ifdef PICO_BUILD

bool Neo6mGps::begin(uart_bus_t uart, std::uint32_t tx_gpio, std::uint32_t rx_gpio,
                     std::uint32_t baud) {
    uart_ = uart;
    uart_init(uart_, baud);
    gpio_set_function(tx_gpio, GPIO_FUNC_UART);
    gpio_set_function(rx_gpio, GPIO_FUNC_UART);
    uart_set_hw_flow(uart_, false, false);
    uart_set_format(uart_, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart_, true);
    ok_ = true;
    return true;
}

void Neo6mGps::poll(std::uint64_t now_ms) {
    if (!ok_) return;
    int budget = kMaxBytesPerPoll;
    while (budget-- > 0 && uart_is_readable(uart_)) {
        const char ch = static_cast<char>(uart_getc(uart_));
        // A sentence that completes and carries a fix stamps the fix clock. The parser
        // holds its last good fix indefinitely, so this stamp is the only evidence that
        // the position being reported is current rather than a memory of one.
        if (parser_.consume(ch) && parser_.has_fix()) {
            last_fix_ms_ = now_ms;
        }
        last_byte_ms_ = now_ms;
    }
}

#else  // host stub

bool Neo6mGps::begin(uart_bus_t, std::uint32_t, std::uint32_t, std::uint32_t) {
    ok_ = false;
    return false;
}
void Neo6mGps::poll(std::uint64_t) {}

#endif

bool Neo6mGps::latest(cansat::GpsData& data) const {
    data = parser_.latest();
    return data.valid;
}

}  // namespace flight::pico
