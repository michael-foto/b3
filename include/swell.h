#ifndef SWELL_H
#define SWELL_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define SWELL_POLLING_INTERVAL (100)

class Swell : public ISystem {
  public:
    Swell() : ISystem() {
        Organ::swell_init();
        millis = 0;
    };

    /// @brief Read the swell MUX channels and set the drawbar values
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= SWELL_POLLING_INTERVAL) {
            Organ::read_swell();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_swell_change();
    }

    /// @brief
    /// @param callback a function to run on change of the swell state
    void set_on_swell_change(void (*callback)()) {
        on_swell_change = callback;
    }

    float volume = 0;

  private:
    elapsedMillis millis = SWELL_POLLING_INTERVAL;
    void (*on_swell_change)() = nullptr;

    void handle_swell_change() {
        if (Organ::current_swell_state != volume) {
            volume = Organ::current_swell_state;
            DEBUG_PRINT("swell volume :: ");
            DEBUG_PRINTLN(Organ::current_swell_state);
        }

        if (on_swell_change != nullptr) {
            on_swell_change();
        }
    }
};

#endif