#ifndef VIBRATO_H
#define VIBRATO_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define VIBRATO_POLLING_INTERVAL (100)

class VibratoSystem : public ISystem {
  public:
    VibratoSystem() : ISystem() {
        Organ::vibrato_init();
        millis = 0;
    };

    /// @brief Read the vibrato MUX channels and set the drawbar values
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= VIBRATO_POLLING_INTERVAL) {
            Organ::read_vibrato();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_vibrato_change();
    }

    /// @brief
    /// @param callback a function to run on change of the vibrato state
    void setOnvibratoChange(void (*callback)()) {
        on_vibrato_change = callback;
    }

    Organ::VibratoMode mode = Organ::VibratoMode::C1;
    boolean upper = false;
    boolean lower = false;

  private:
    elapsedMillis millis = VIBRATO_POLLING_INTERVAL;
    void (*on_vibrato_change)() = nullptr;

    void handle_vibrato_change() {
        bool change = false;

        if (Organ::current_vibrato_state.upper != upper) {
            DEBUG_PRINT("upper vibrato  :: ");
            DEBUG_PRINTLN(Organ::current_vibrato_state.upper ? "on" : "off");
            change = true;
            upper = Organ::current_vibrato_state.upper;
        } else if (Organ::current_vibrato_state.lower != lower) {
            DEBUG_PRINT("lower vibrato :: ");
            DEBUG_PRINTLN(Organ::current_vibrato_state.lower ? "on" : "off");
            change = true;
            lower = Organ::current_vibrato_state.lower;
        } else if (Organ::current_vibrato_state.mode != mode) {
            DEBUG_PRINT("Vibrato mode change :: ");
            DEBUG_PRINTLN((int)Organ::current_vibrato_state.mode);
            change = true;
            mode = Organ::current_vibrato_state.mode;
        }

        if (on_vibrato_change != nullptr && change) {
            on_vibrato_change();
        }
    }
};

#endif