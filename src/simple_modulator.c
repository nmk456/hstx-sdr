#include "simple_modulator.h"

#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "pico/stdlib.h"

#define FIRST_HSTX_PIN 12

static inline void hstx_put_word(uint32_t data) {
    while (hstx_fifo_hw->stat & HSTX_FIFO_STAT_FULL_BITS);
    hstx_fifo_hw->fifo = data;
}

void simple_mod_config(int pin) {
    gpio_init(pin);
    gpio_set_dir(pin, true);
    gpio_put(pin, 0);
    gpio_set_function(pin, GPIO_FUNC_HSTX);

    // Weaker drive strength to reduce power
    gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);

    // Configure pin for DDR, send bit 0 then bit 1
    hstx_ctrl_hw->bit[pin - FIRST_HSTX_PIN] =
        (0u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (1u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |                // Enable
        (2u << HSTX_CTRL_CSR_SHIFT_LSB) |      // Shift 2 bits per cycle
        (16u << HSTX_CTRL_CSR_N_SHIFTS_LSB) |  // Shift 16 times
        (1u << HSTX_CTRL_CSR_CLKDIV_LSB);      // CLKDIV = 1
}

inline void simple_mod_send(uint8_t byte) {
    uint32_t word0 = 0xAAAAAAAA;
    uint32_t word1 = 0x55555555;

    for (int i = 0; i < 8; i++) {
        hstx_put_word(((byte >> i) & 1) ? word1 : word0);
    }
}
