#ifndef DRAWBARS_H
#define DRAWBARS_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define POLLING_INTERVAL (100)

class Percussion : public ISystem {
  public:
    Percussion() : ISystem() {
      Organ::percussion_init();
    };

    /// @brief Read the percussion switches 
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= POLLING_INTERVAL) {
            Organ::read_drawbars();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_drawbar_change();
    }

    /// @brief
    /// @param callback a function that takes the drawbar number (1-indexed) and the value (0-8)
    void setOnDrawbarChange(void (*callback)()) {
        on_drawbar_change = callback;
    }

    uint8_t upper[10] = {0};
    uint8_t lower[10] = {0};

  private:
    elapsedMillis millis = POLLING_INTERVAL;
    void (*on_drawbar_change)() = nullptr;

    void handle_drawbar_change() {
        boolean upper_changed = false;
        boolean lower_changed = false;
        uint8_t new_state;
        for (uint8_t i = 1; i <= 9; i++) {
            if (upper[i] != Organ::current_drawbar_state[i - 1]) {
                upper_changed = true;
            }
            // because drawbars are 1-indexed, but hardware is 0-indexed
            if (lower[i] != Organ::current_drawbar_state[i + 9 - 1]) {
                lower_changed = true;
            }
        }

        // fill values into the local state
        if (upper_changed) {
            std::copy(&Organ::current_drawbar_state, &Organ::current_drawbar_state + 9, upper + 1);
            on_drawbar_change();
        }
        if (lower_changed) {
            std::copy(&Organ::current_drawbar_state + 9, &Organ::current_drawbar_state + 18, lower + 1);
            on_drawbar_change();
        }
    }
};

#endif