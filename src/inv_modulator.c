#include "inv_modulator.h"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "inv_modulator.pio.h"

#define FIRST_HSTX_PIN 12

static int sm;
static int dma_chan1, dma_chan2;
static int dma_timer;

void inv_mod_setup(int rf_pin, PIO pio, bool enable_dpsk) {
    // GPIO setup
    gpio_init(rf_pin);
    gpio_put(rf_pin, true);
    gpio_set_dir(rf_pin, true);

    //// HSTX setup
    // Configure pin for clock output
    hstx_ctrl_hw->bit[rf_pin - FIRST_HSTX_PIN] = HSTX_CTRL_BIT0_CLK_BITS;

    // Leave HSTX CSR at defaults

    // PIO setup
    sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &inv_modulator_program);
    pio_sm_config pio_config = inv_modulator_program_get_default_config(offset);
    pio_sm_init(pio, sm, offset, &pio_config);
    pio_sm_set_enabled(pio, sm, true);

    //// DMA setup
    // Transmit buffer to PIO
    dma_chan1 = dma_claim_unused_channel(true);
    dma_channel_config dma_config1 = dma_channel_get_default_config(dma_chan1);
    channel_config_set_read_increment(&dma_config1, true);
    channel_config_set_write_increment(&dma_config1, true);
    channel_config_set_transfer_data_size(&dma_config1, DMA_SIZE_8);
    channel_config_set_dreq(&dma_config1, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        dma_chan1,
        &dma_config1,
        &pio->txf[sm],
        NULL,
        0,
        false);

    // PIO to HSTX
    dma_chan2 = dma_claim_unused_channel(true);
    dma_channel_config dma_config2 = dma_channel_get_default_config(dma_chan2);
    channel_config_set_read_increment(&dma_config2, false);
    channel_config_set_write_increment(&dma_config2, false);
    channel_config_set_transfer_data_size(&dma_config2, DMA_SIZE_8);

    dma_timer = dma_claim_unused_timer(true);
    channel_config_set_dreq(&dma_config2, dma_get_timer_dreq(dma_timer));

    // Pacing timer - 150/1024 = 146.48 kbps default
    dma_timer_set_fraction(dma_timer, 1, 1024);

    volatile void *pin_inv_addr = &hstx_ctrl_hw->bit[rf_pin - FIRST_HSTX_PIN] + 2;

    // Used atomic XOR alias
    if (enable_dpsk) {
        pin_inv_addr += 0x1000;
    }

    dma_channel_configure(
        dma_chan2,
        &dma_config2,
        pin_inv_addr,
        &pio->rxf[sm],
        0,
        false);
}

void inv_mod_datarate(int divider) {
    dma_timer_set_fraction(dma_timer, 1, divider);
}

void inv_mod_transmit(uint8_t *buf, uint len) {
    // Set read address, length, then start transfer
    dma_channel_set_read_addr(dma_chan1, buf, false);
    dma_channel_set_trans_count(dma_chan1, len, true);
}

void inv_mod_enable(bool en) {
    if (en) {
        // Enable HSTX
        *(&hstx_ctrl_hw->csr + 0x2000) = HSTX_CTRL_CSR_EN_BITS;

        // Enable DMA
        dma_channel_set_trans_count(dma_chan2, 0, true);
    } else {
        // Disable HSTX
        *(&hstx_ctrl_hw->csr + 0x3000) = HSTX_CTRL_CSR_EN_BITS;

        // Disable DMA
        dma_channel_abort(dma_chan2);
    }
}

int inv_mod_busy() {
    if (dma_channel_is_busy(dma_chan1) || dma_channel_is_busy(dma_chan2)) {
        return 1;
    }

    return 0;
}
