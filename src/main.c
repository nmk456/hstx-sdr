#include "inv_modulator.h"

#define PIN_RF 12

int main() {
    stdio_init_all();

    inv_mod_setup(PIN_RF, pio0);
    inv_mod_enable(true);

    char msg[] = "hello there";

    while (1) {
        inv_mod_transmit((uint8_t*) &msg, sizeof(msg));

        sleep_ms(100);
    }
}
