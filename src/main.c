#include "hardware/pio.h"
#include "inv_modulator.h"
#include "pico/stdlib.h"

#define PIN_RF 15

int main() {
    stdio_init_all();

    inv_mod_setup(PIN_RF, pio0, true);
    inv_mod_enable(true);

    char *msg = "hello there";

    while (1) {
        inv_mod_transmit((uint8_t *) msg, sizeof(msg));
    }
}
