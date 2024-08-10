#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "pico/stdlib.h"

#define PIN_RF 15
#define FIRST_HSTX_PIN 12

static inline void hstx_put_word(uint32_t data) {
    while (hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS);
    hstx_fifo_hw->fifo = data;
}

void hstx_config() {
    gpio_init(PIN_RF);
    gpio_set_dir(PIN_RF, true);
    gpio_put(PIN_RF, 0);
    gpio_set_function(PIN_RF, GPIO_FUNC_HSTX);

    // Weaker drive strength to reduce power
    gpio_set_drive_strength(PIN_RF, GPIO_DRIVE_STRENGTH_2MA);

    // Configure pin for DDR, send bit 0 then bit 1
    hstx_ctrl_hw->bit[PIN_RF - FIRST_HSTX_PIN] =
        (0u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (1u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |                // Enable
        (2u << HSTX_CTRL_CSR_SHIFT_LSB) |      // Shift 2 bits per cycle
        (16u << HSTX_CTRL_CSR_N_SHIFTS_LSB) |  // Shift 16 times
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);      // CLKDIV = 1
}

static inline void send_bpsk(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        uint32_t word = ((byte >> i) & 1) ? 0x55555555 : 0xAAAAAAAA;
        hstx_put_word(word);
    }
}

int main() {
    stdio_init_all();
    hstx_config();

    uint8_t x = 0;

    while (1) {
        // Send carrier
        send_bpsk(0);

        // Send incrementing value
        // send_bpsk(x++);
    }
}
