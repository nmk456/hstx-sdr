#pragma once

#include "hardware/pio.h"
#include "pico/stdlib.h"

void inv_mod_setup(int rf_pin, PIO pio);

// Sets datarate to clk_hstx / divider
void inv_mod_datarate(int divider);

// Transmit data
void inv_mod_transmit(uint8_t *buf, uint len);

// Enable transmitter - do this immediately after transmitting
void inv_mod_enable(bool en);

// Returns 1 if transmit is in progress
int inv_mod_busy();
