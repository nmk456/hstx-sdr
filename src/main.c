#include "hardware/clocks.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include <stdio.h>

#include "inv_modulator.h"

#define PIN_RF 15
#define PIN_DEBUG 16

int main() {
    stdio_init_all();

    gpio_init(PIN_DEBUG);
    gpio_set_dir(PIN_DEBUG, GPIO_OUT);
    gpio_set_function(PIN_DEBUG, GPIO_FUNC_SIO);
    gpio_put(PIN_DEBUG, 0);

    inv_mod_setup(PIN_RF, pio0);
    inv_mod_enable(true);

    char msg[] = "hello there";

    while (1) {
        gpio_put(PIN_DEBUG, 1);
        inv_mod_transmit((uint8_t*) &msg, sizeof(msg));
        while(inv_mod_busy());
        gpio_put(PIN_DEBUG, 0);

        sleep_ms(5);
    }
}
