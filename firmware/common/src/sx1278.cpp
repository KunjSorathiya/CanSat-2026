#include "cansat/sx1278.hpp"

namespace cansat {

namespace {

// SX1276/77/78/79 register map (LoRa mode) from the Semtech datasheet.
constexpr std::uint8_t REG_FIFO = 0x00;
constexpr std::uint8_t REG_OP_MODE = 0x01;
constexpr std::uint8_t REG_FRF_MSB = 0x06;
constexpr std::uint8_t REG_FRF_MID = 0x07;
constexpr std::uint8_t REG_FRF_LSB = 0x08;
constexpr std::uint8_t REG_PA_CONFIG = 0x09;
constexpr std::uint8_t REG_OCP = 0x0B;
constexpr std::uint8_t REG_LNA = 0x0C;
constexpr std::uint8_t REG_FIFO_ADDR_PTR = 0x0D;
constexpr std::uint8_t REG_FIFO_TX_BASE_ADDR = 0x0E;
constexpr std::uint8_t REG_FIFO_RX_BASE_ADDR = 0x0F;
constexpr std::uint8_t REG_FIFO_RX_CURRENT_ADDR = 0x10;
constexpr std::uint8_t REG_IRQ_FLAGS = 0x12;
constexpr std::uint8_t REG_RX_NB_BYTES = 0x13;
constexpr std::uint8_t REG_PKT_SNR_VALUE = 0x19;
constexpr std::uint8_t REG_PKT_RSSI_VALUE = 0x1A;
constexpr std::uint8_t REG_MODEM_CONFIG_1 = 0x1D;
constexpr std::uint8_t REG_MODEM_CONFIG_2 = 0x1E;
constexpr std::uint8_t REG_PREAMBLE_MSB = 0x20;
constexpr std::uint8_t REG_PREAMBLE_LSB = 0x21;
constexpr std::uint8_t REG_PAYLOAD_LENGTH = 0x22;
constexpr std::uint8_t REG_MODEM_CONFIG_3 = 0x26;
constexpr std::uint8_t REG_DETECTION_OPTIMIZE = 0x31;
constexpr std::uint8_t REG_DETECTION_THRESHOLD = 0x37;
constexpr std::uint8_t REG_SYNC_WORD = 0x39;
constexpr std::uint8_t REG_DIO_MAPPING_1 = 0x40;
constexpr std::uint8_t REG_VERSION = 0x42;
constexpr std::uint8_t REG_PA_DAC = 0x4D;

constexpr std::uint8_t MODE_LONG_RANGE = 0x80;  // LoRa (must be set in sleep)
constexpr std::uint8_t MODE_SLEEP = 0x00;
constexpr std::uint8_t MODE_STDBY = 0x01;
constexpr std::uint8_t MODE_TX = 0x03;
constexpr std::uint8_t MODE_RX_CONTINUOUS = 0x05;

constexpr std::uint8_t IRQ_TX_DONE = 0x08;
constexpr std::uint8_t IRQ_RX_DONE = 0x40;
constexpr std::uint8_t IRQ_PAYLOAD_CRC_ERROR = 0x20;

constexpr std::uint8_t PA_BOOST = 0x80;
constexpr double kFstepHz = 32000000.0 / 524288.0;  // 32 MHz / 2^19

std::uint8_t bandwidth_index(std::uint32_t hz) {
    const std::uint32_t table[] = {7800,   10400,  15600,  20800, 31250,
                                   41700,  62500,  125000, 250000, 500000};
    for (std::uint8_t i = 0; i < 10; ++i) {
        if (hz <= table[i]) return i;
    }
    return 9;
}

}  // namespace

void Sx1278::write_reg(std::uint8_t reg, std::uint8_t value) {
    const std::uint8_t tx[2] = {static_cast<std::uint8_t>(reg | 0x80), value};
    hal_.select(hal_.ctx, true);
    hal_.transfer(hal_.ctx, tx, nullptr, 2);
    hal_.select(hal_.ctx, false);
}

std::uint8_t Sx1278::read_reg(std::uint8_t reg) {
    const std::uint8_t tx[2] = {static_cast<std::uint8_t>(reg & 0x7F), 0x00};
    std::uint8_t rx[2] = {0, 0};
    hal_.select(hal_.ctx, true);
    hal_.transfer(hal_.ctx, tx, rx, 2);
    hal_.select(hal_.ctx, false);
    return rx[1];
}

void Sx1278::write_fifo(const std::uint8_t* data, std::size_t len) {
    const std::uint8_t addr = REG_FIFO | 0x80;
    hal_.select(hal_.ctx, true);
    hal_.transfer(hal_.ctx, &addr, nullptr, 1);
    hal_.transfer(hal_.ctx, data, nullptr, len);
    hal_.select(hal_.ctx, false);
}

void Sx1278::read_fifo(std::uint8_t* out, std::size_t len) {
    const std::uint8_t addr = REG_FIFO & 0x7F;
    hal_.select(hal_.ctx, true);
    hal_.transfer(hal_.ctx, &addr, nullptr, 1);
    hal_.transfer(hal_.ctx, nullptr, out, len);
    hal_.select(hal_.ctx, false);
}

void Sx1278::set_mode(std::uint8_t mode) {
    write_reg(REG_OP_MODE, MODE_LONG_RANGE | mode);
}

std::uint32_t Sx1278::now_ms() {
    return hal_.millis ? hal_.millis(hal_.ctx) : 0;
}

void Sx1278::sleep(std::uint32_t ms) {
    if (hal_.delay_ms) hal_.delay_ms(hal_.ctx, ms);
}

bool Sx1278::apply_settings(const Sx1278Settings& s) {
    settings_ = s;

    set_mode(MODE_SLEEP);
    sleep(5);
    set_mode(MODE_STDBY);
    sleep(5);

    // Carrier frequency.
    const std::uint64_t frf =
        static_cast<std::uint64_t>(static_cast<double>(s.frequency_hz) / kFstepHz + 0.5);
    write_reg(REG_FRF_MSB, static_cast<std::uint8_t>(frf >> 16));
    write_reg(REG_FRF_MID, static_cast<std::uint8_t>(frf >> 8));
    write_reg(REG_FRF_LSB, static_cast<std::uint8_t>(frf));

    // FIFO base addresses: full 256-byte FIFO for both directions.
    write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);

    // LNA: max gain + boost.
    write_reg(REG_LNA, static_cast<std::uint8_t>(read_reg(REG_LNA) | 0x03));

    // Bandwidth + coding rate + explicit header.
    std::uint8_t cr = s.coding_rate;
    if (cr < 5) cr = 5;
    if (cr > 8) cr = 8;
    const std::uint8_t bw = bandwidth_index(s.bandwidth_hz);
    write_reg(REG_MODEM_CONFIG_1,
              static_cast<std::uint8_t>((bw << 4) | ((cr - 4) << 1) | 0x00));

    // Spreading factor + CRC.
    std::uint8_t sf = s.spreading_factor;
    if (sf < 6) sf = 6;
    if (sf > 12) sf = 12;
    write_reg(REG_MODEM_CONFIG_2,
              static_cast<std::uint8_t>((sf << 4) | (s.enable_crc ? 0x04 : 0x00)));

    // Low-data-rate optimise on when symbol time > 16 ms; AGC auto-on.
    const double symbol_ms = (1u << sf) * 1000.0 / static_cast<double>(s.bandwidth_hz);
    write_reg(REG_MODEM_CONFIG_3, static_cast<std::uint8_t>((symbol_ms > 16.0 ? 0x08 : 0x00) | 0x04));

    // SF6 needs special detection settings; otherwise use the LoRa defaults.
    if (sf == 6) {
        write_reg(REG_DETECTION_OPTIMIZE, 0xC5);
        write_reg(REG_DETECTION_THRESHOLD, 0x0C);
    } else {
        write_reg(REG_DETECTION_OPTIMIZE, 0xC3);
        write_reg(REG_DETECTION_THRESHOLD, 0x0A);
    }

    write_reg(REG_PREAMBLE_MSB, static_cast<std::uint8_t>(s.preamble_length >> 8));
    write_reg(REG_PREAMBLE_LSB, static_cast<std::uint8_t>(s.preamble_length));

    write_reg(REG_SYNC_WORD, s.sync_word);

    // PA + OCP. PA_BOOST path; enable PA_DAC (+3 dB, up to 20 dBm) only above 17 dBm.
    std::int8_t power = s.tx_power_dbm;
    if (power < 2) power = 2;
    if (power > 20) power = 20;
    if (power > 17) {
        write_reg(REG_PA_DAC, 0x87);
        write_reg(REG_PA_CONFIG, static_cast<std::uint8_t>(PA_BOOST | (17 - 2)));
        // Remaining headroom handled by PA_DAC; register value capped at 17 dBm nominal.
        write_reg(REG_PA_CONFIG, static_cast<std::uint8_t>(PA_BOOST | (power - 5)));
    } else {
        write_reg(REG_PA_DAC, 0x84);
        write_reg(REG_PA_CONFIG, static_cast<std::uint8_t>(PA_BOOST | (power - 2)));
    }
    write_reg(REG_OCP, 0x2B);  // OCP on, ~100 mA trim

    // DIO0 -> RxDone by default (00). transmit() re-maps to TxDone (01) as needed.
    write_reg(REG_DIO_MAPPING_1, 0x00);

    set_mode(MODE_STDBY);
    return true;
}

bool Sx1278::begin(const Sx1278Hal& hal, const Sx1278Settings& settings) {
    hal_ = hal;
    healthy_ = false;
    receiving_ = false;
    if (!hal_.select || !hal_.transfer || !hal_.set_reset || !hal_.delay_ms) {
        return false;
    }

    // Hardware reset: RA-02 RESET is active low.
    hal_.set_reset(hal_.ctx, false);
    sleep(2);
    hal_.set_reset(hal_.ctx, true);
    sleep(10);

    version_ = read_reg(REG_VERSION);
    if (version_ != 0x12) {
        return false;  // 0x12 = SX1276/78 silicon revision
    }

    // Enter LoRa mode (only settable while in sleep).
    set_mode(MODE_SLEEP);
    sleep(5);

    healthy_ = apply_settings(settings);
    return healthy_;
}

bool Sx1278::reconfigure(const Sx1278Settings& settings) {
    if (!healthy_) return false;
    receiving_ = false;
    return apply_settings(settings);
}

bool Sx1278::set_sync_word(std::uint8_t sync_word) {
    if (!healthy_) return false;
    settings_.sync_word = sync_word;
    write_reg(REG_SYNC_WORD, sync_word);
    return read_reg(REG_SYNC_WORD) == sync_word;
}

bool Sx1278::transmit(const std::uint8_t* data, std::size_t len, std::uint32_t timeout_ms) {
    if (!healthy_ || data == nullptr || len == 0) {
        return false;
    }
    if (len > 255) len = 255;

    receiving_ = false;
    set_mode(MODE_STDBY);
    write_reg(REG_DIO_MAPPING_1, 0x40);  // DIO0 = TxDone
    write_reg(REG_FIFO_ADDR_PTR, 0x00);
    write_fifo(data, len);
    write_reg(REG_PAYLOAD_LENGTH, static_cast<std::uint8_t>(len));
    write_reg(REG_IRQ_FLAGS, 0xFF);  // clear
    set_mode(MODE_TX);

    // Wait for TxDone via DIO0 if wired, otherwise poll the IRQ register. Bounded either
    // way so a stuck radio can never stall the caller.
    const std::uint32_t start = now_ms();
    std::uint32_t waited = 0;
    while (true) {
        const bool done = hal_.read_dio0 ? hal_.read_dio0(hal_.ctx)
                                         : (read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE) != 0;
        if (done) {
            break;
        }
        if (hal_.millis) {
            if (now_ms() - start >= timeout_ms) {
                set_mode(MODE_STDBY);
                write_reg(REG_IRQ_FLAGS, 0xFF);
                return false;
            }
        } else {
            sleep(2);
            waited += 2;
            if (waited >= timeout_ms) {
                set_mode(MODE_STDBY);
                write_reg(REG_IRQ_FLAGS, 0xFF);
                return false;
            }
        }
    }

    write_reg(REG_IRQ_FLAGS, 0xFF);
    set_mode(MODE_STDBY);
    return true;
}

void Sx1278::start_receive() {
    if (!healthy_) return;
    set_mode(MODE_STDBY);
    write_reg(REG_DIO_MAPPING_1, 0x00);  // DIO0 = RxDone
    write_reg(REG_FIFO_ADDR_PTR, 0x00);
    write_reg(REG_IRQ_FLAGS, 0xFF);
    set_mode(MODE_RX_CONTINUOUS);
    receiving_ = true;
}

std::size_t Sx1278::poll_receive(std::uint8_t* out, std::size_t cap) {
    if (!healthy_ || !receiving_ || out == nullptr || cap == 0) {
        return 0;
    }
    const std::uint8_t irq = read_reg(REG_IRQ_FLAGS);
    if ((irq & IRQ_RX_DONE) == 0) {
        return 0;
    }
    write_reg(REG_IRQ_FLAGS, 0xFF);
    if (irq & IRQ_PAYLOAD_CRC_ERROR) {
        return 0;  // corrupt frame: drop, keep listening
    }

    std::size_t len = read_reg(REG_RX_NB_BYTES);
    if (len > cap) len = cap;
    write_reg(REG_FIFO_ADDR_PTR, read_reg(REG_FIFO_RX_CURRENT_ADDR));
    read_fifo(out, len);

    const std::uint8_t snr_raw = read_reg(REG_PKT_SNR_VALUE);
    last_snr_db_ = static_cast<float>(static_cast<std::int8_t>(snr_raw)) / 4.0f;
    const std::uint8_t rssi_raw = read_reg(REG_PKT_RSSI_VALUE);
    // 433 MHz band uses the -157 dBm offset (Semtech datasheet 5.5.5).
    last_rssi_dbm_ = -157 + static_cast<int>(rssi_raw) +
                     (last_snr_db_ < 0.0f ? static_cast<int>(last_snr_db_) : 0);
    return len;
}

}  // namespace cansat
