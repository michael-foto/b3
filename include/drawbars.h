#ifndef DRAWBARS_H
#define DRAWBARS_H

#include <ISystem.h>

#define POLLING_INTERVAL (100)

class Drawbars : public ISystem {
  public:
    Drawbars();
    /// @brief Read the drawbar MUX channels and set the drawbar values
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
    void setOnDrawbarChange(void (*callback)(uint8_t drawbar, uint8_t value)) {
    }

    uint8_t upper[10] = {0};
    uint8_t lower[10] = {0};

  private:
    elapsedMillis millis = POLLING_INTERVAL;
    void (*on_drawbar_change)(uint8_t drawbar, uint16_t value) = nullptr;

    void handle_drawbar_change() {
        boolean upper_changed = false;
        boolean lower_changed = false;
        for (uint8_t i = 1; i <= 9; i++) {
            if (upper[i] != Organ::current_drawbar_state[i - 1]) {
                on_drawbar_change(i - 1, upper[i]);
                upper_changed = true;
            }
            // because drawbars are 1-indexed, but hardware is 0-indexed
            if (lower[i] != Organ::current_drawbar_state[i + 9 - 1]) {
                on_drawbar_change(i + 9 - 1, lower[i]);
                lower_changed = true;
            }
        }

        // fill values into the local state
        if (upper_changed) {
            std::copy(&Organ::current_drawbar_state, &Organ::current_drawbar_state + 9, upper + 1);
        }
        if (lower_changed) {
            std::copy(&Organ::current_drawbar_state + 9, &Organ::current_drawbar_state + 18, lower + 1);
        }
    }
};

#endif