#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstddef>
#include <cstdint>

struct spi_inst_t;
extern spi_inst_t* spi0;
extern spi_inst_t* spi1;

enum spi_cpol_t { SPI_CPOL_0, SPI_CPOL_1 };
enum spi_cpha_t { SPI_CPHA_0, SPI_CPHA_1 };
enum spi_order_t { SPI_LSB_FIRST, SPI_MSB_FIRST };

extern "C" {
std::uint32_t spi_init(spi_inst_t* spi, std::uint32_t baudrate);
std::uint32_t spi_set_baudrate(spi_inst_t* spi, std::uint32_t baudrate);
void spi_set_format(spi_inst_t* spi, std::uint32_t data_bits, spi_cpol_t cpol,
                    spi_cpha_t cpha, spi_order_t order);
int spi_write_read_blocking(spi_inst_t* spi, const std::uint8_t* src, std::uint8_t* dst,
                            std::size_t len);
int spi_write_blocking(spi_inst_t* spi, const std::uint8_t* src, std::size_t len);
int spi_read_blocking(spi_inst_t* spi, std::uint8_t repeated_tx, std::uint8_t* dst,
                      std::size_t len);
}
