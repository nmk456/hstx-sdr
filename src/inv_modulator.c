#include "inv_modulator.h"

#include <assert.h>

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "inv_modulator.pio.h"

#define FIRST_HSTX_PIN 12

static int sm;
static PIO pio;
static int dma_chan1, dma_chan2, dma_chan3;
static int dma_timer;

// Data rate is sys_clk/divider
static int dma_timer_divider = 256;

static int dummy_hstx_data = 0x69696969;
static uint8_t sync_word[] = {0x6B, 0xBE};

void inv_mod_setup(int rf_pin, PIO _pio) {
    // GPIO setup
    gpio_init(rf_pin);
    gpio_set_dir(rf_pin, GPIO_OUT);
    gpio_set_function(rf_pin, GPIO_FUNC_HSTX);
    gpio_set_drive_strength(rf_pin, GPIO_DRIVE_STRENGTH_2MA);
    gpio_put(rf_pin, 0);

    //// HSTX setup
    // Configure pin for clock output
    hstx_ctrl_hw->bit[rf_pin - FIRST_HSTX_PIN] = HSTX_CTRL_BIT0_CLK_BITS;
    hw_clear_bits(&hstx_ctrl_hw->csr, HSTX_CTRL_CSR_N_SHIFTS_BITS);

    // Leave HSTX CSR at defaults

    //// PIO setup
    pio = _pio;
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
    channel_config_set_write_increment(&dma_config1, false);
    channel_config_set_transfer_data_size(&dma_config1, DMA_SIZE_8);
    channel_config_set_dreq(&dma_config1, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        dma_chan1,
        &dma_config1,
        &pio->txf[sm],
        NULL,
        0,
        false
    );

    // PIO to HSTX
    dma_chan2 = dma_claim_unused_channel(true);
    dma_channel_config dma_config2 = dma_channel_get_default_config(dma_chan2);
    channel_config_set_read_increment(&dma_config2, false);
    channel_config_set_write_increment(&dma_config2, false);
    channel_config_set_transfer_data_size(&dma_config2, DMA_SIZE_8);

    dma_timer = dma_claim_unused_timer(true);
    channel_config_set_dreq(&dma_config2, dma_get_timer_dreq(dma_timer));

    // Pacing timer - 150/256 = 585.9 kbps default
    dma_timer_set_fraction(dma_timer, 1, dma_timer_divider);

    volatile void *pin_inv_addr = hw_xor_alias(((uint8_t*) &hstx_ctrl_hw->bit[rf_pin - FIRST_HSTX_PIN]) + 2);

    dma_channel_configure(
        dma_chan2,
        &dma_config2,
        pin_inv_addr,
        &pio->rxf[sm],
        0,
        true
    );

    // Dummy transfer to HSTX. This is required because the clock signal is not generated
    // unless there's incoming data.
    dma_chan3 = dma_claim_unused_channel(true);

    dma_channel_config dma_config3 = dma_channel_get_default_config(dma_chan3);
    channel_config_set_transfer_data_size(&dma_config3, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config3, false);
    channel_config_set_write_increment(&dma_config3, false);
    channel_config_set_dreq(&dma_config3, DREQ_HSTX);

    dma_channel_configure(
        dma_chan3,
        &dma_config3,
        &hstx_fifo_hw->fifo,
        &dummy_hstx_data,
        0,
        false
    );
}

void inv_mod_datarate(int divider) {
    assert(divider % 32 && "Divider must be a multiple of 32 for now"); // see inv_mod_transmit

    dma_timer_set_fraction(dma_timer, 1, divider);
    dma_timer_divider = divider;
}

void inv_mod_transmit(uint8_t *buf, uint len) {
    // Make sure last transmission is finished and PIO is back to a known state
    while (inv_mod_busy());
    pio_sm_restart(pio, sm);
    pio_sm_clear_fifos(pio, sm);

    // Set read address, length, then start transfer
    dma_channel_set_read_addr(dma_chan1, buf, false);
    dma_channel_set_trans_count(dma_chan1, len, false);

    // Number of transfers = (number of bits + 1) * cycles per symbol / 32.
    // We are doing one bit extra because the last bit will otherwise get cut off unless
    // the pacing timer is exactly synchronized with transfer start (unlikely).
    dma_channel_set_trans_count(dma_chan3, ((len + 4) * 8 + 1) * dma_timer_divider / 32, false);

    // The PIO FIFO is 4 words deep so we have space to insert a 16 bit
    // preamble and 16 bit sync word before starting. The first word will be
    // pulled into the OSR so we have 5 words total to work with.

    // Send just carrier
    pio_sm_put_blocking(pio, sm, 0x00);

    // 16 bit preamble, repeating 01s after differential encoding
    pio_sm_put_blocking(pio, sm, 0xFF);
    pio_sm_put_blocking(pio, sm, 0xFF);

    // Sync word
    pio_sm_put_blocking(pio, sm, sync_word[0]);
    pio_sm_put_blocking(pio, sm, sync_word[1]);

    // Start the DMA transfers simultaneously
    dma_start_channel_mask((1u << dma_chan1) | (1u << dma_chan3));
}

void inv_mod_enable(bool en) {
    if (en) {
        // Enable HSTX
        hw_set_bits(&hstx_ctrl_hw->csr, HSTX_CTRL_CSR_EN_BITS | (1u << HSTX_CTRL_CSR_CLKDIV_LSB));

        // Enable DMA in endless mode (transfer count must be non-zero)
        dma_channel_set_trans_count(dma_chan2, (DMA_CH0_TRANS_COUNT_MODE_VALUE_ENDLESS << DMA_CH0_TRANS_COUNT_MODE_LSB) | 1u, true);
    } else {
        // Disable HSTX
        hw_clear_bits(&hstx_ctrl_hw->csr, HSTX_CTRL_CSR_EN_BITS);

        // Disable DMA
        dma_channel_abort(dma_chan1);
        dma_channel_abort(dma_chan2);
        dma_channel_abort(dma_chan3);

        // Reset PIO state
        pio_sm_restart(pio, sm);
        pio_sm_clear_fifos(pio, sm);
    }
}

int inv_mod_busy() {
    if (dma_channel_is_busy(dma_chan1) || dma_channel_is_busy(dma_chan3) || !pio_sm_is_rx_fifo_empty(pio, sm)) {
        return 1;
    }

    return 0;
}
