#ifndef PERCUSSION_H
#define PERCUSSION_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define POLLING_INTERVAL (100)

class Percussion : public ISystem {
  public:
    Percussion() : ISystem() {
        Organ::percussion_init();
    };

    /// @brief Read the percussion settings and if they've changed send an update
    /// message to the registered callback handler
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= POLLING_INTERVAL) {
            Organ::read_percussion();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_percussion_change();
    }

    /// @brief
    /// @param callback a function to fire when percussion values have changed
    void set_on_percussion_change(void (*callback)()) {
        on_percussion_change = callback;
    }


  private:
    elapsedMillis millis = POLLING_INTERVAL;
    Organ::Percussion prev_state = {};
    void (*on_percussion_change)() = nullptr;

    void handle_percussion_change() {
        uint8_t new_state;
        auto current_state = Organ::percussion;

        if (prev_state.on != current_state.on ||
            prev_state.speed != current_state.speed ||
            prev_state.type != current_state.type ||
            prev_state.volume != current_state.volume) {
                on_percussion_change();
                // write back to local state
                memcpy(&prev_state, &current_state, sizeof current_state);
            }
    }
};

#endif