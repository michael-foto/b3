#ifndef DRAWBARS_H
#define DRAWBARS_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define DRAWBAR_POLLING_INTERVAL (100)

class Drawbars : public ISystem {
  public:
    Drawbars() : ISystem() {
        Organ::drawbars_init();
        millis = 0;
    };

    /// @brief Read the drawbar MUX channels and set the drawbar values
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= DRAWBAR_POLLING_INTERVAL) {
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

    std::array<uint8_t, 10> upper = {0};
    std::array<uint8_t, 10> lower = {0};

  private:
    elapsedMillis millis = DRAWBAR_POLLING_INTERVAL;
    void (*on_drawbar_change)() = nullptr;

    void handle_drawbar_change() {
        boolean upper_changed = false;
        boolean lower_changed = false;
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
            std::copy(std::begin(Organ::current_drawbar_state), std::begin(Organ::current_drawbar_state) + 9, std::begin(upper) + 1);
            DEBUG_PRINT("upper drawbars :: ");
            for (int i = 1; i < 10; i++) {
                DEBUG_PRINT(upper[i]);
                DEBUG_PRINT(", ");
            }
            DEBUG_PRINTLN();
            on_drawbar_change();
        }
        if (lower_changed) {
            std::copy(std::begin(Organ::current_drawbar_state) + 9, std::begin(Organ::current_drawbar_state) + 18, std::begin(lower) + 1);
            DEBUG_PRINT("lower drawbars :: ");
            for (int i = 1; i < 10; i++) {
                DEBUG_PRINT(lower[i]);
                DEBUG_PRINT(", ");
            }
            DEBUG_PRINTLN();
            on_drawbar_change();
        }
    }
};

#endif