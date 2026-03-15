#ifndef PERCUSSION_H
#define PERCUSSION_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define PERCUSSION_POLLING_INTERVAL (100)

class Percussion : public ISystem {
  public:
    Percussion() : ISystem() {
        Organ::percussion_init();
    };

    /// @brief Read the percussion switches
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= PERCUSSION_POLLING_INTERVAL) {
            Organ::read_percussion();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_percussion_change();
    }

    /// @brief
    /// @param callback a function that takes the percussion number (1-indexed) and the value (0-8)
    void set_on_percussion_change(void (*callback)()) {
        on_percussion_change = callback;
    }

    Organ::Speed speed;
    Organ::PercussionHarmonic type;
    Organ::PercussionVolume volume;
    boolean on;

  private:
    elapsedMillis millis = PERCUSSION_POLLING_INTERVAL;
    void (*on_percussion_change)() = nullptr;

    void handle_percussion_change() {
        bool change = false;

        if (Organ::current_percussion_state.speed != speed) {
            DEBUG_PRINT("percussion speed change:: ");
            DEBUG_PRINTLN((int)Organ::current_percussion_state.speed);
            change = true;
            speed = Organ::current_percussion_state.speed;
        } else if (Organ::current_percussion_state.type != type) {
            DEBUG_PRINT("percussion harmonic change:: ");
            DEBUG_PRINTLN((int)Organ::current_percussion_state.type);
            change = true;
            type = Organ::current_percussion_state.type;
        } else if (Organ::current_percussion_state.volume != volume) {
            DEBUG_PRINT("percussion volume change :: ");
            DEBUG_PRINTLN((int)Organ::current_percussion_state.volume);
            change = true;
            volume = Organ::current_percussion_state.volume;
        } else if (Organ::current_percussion_state.on != on) {
            DEBUG_PRINT("percussion change :: ");
            DEBUG_PRINTLN(Organ::current_percussion_state.on ? "on" : "off");
            change = true;
            on = Organ::current_percussion_state.on;
        }

        if (on_percussion_change != nullptr && change) {
            on_percussion_change();
        }
    }
};

#endif