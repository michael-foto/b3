#ifndef LESLIE_H
#define LESLIE_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define LESLIE_POLLING_INTERVAL (200)

class Leslie : public ISystem {
  public:
    Leslie() : ISystem() {
        Organ::leslie_init();
        millis = 0;
    };

    /// @brief Read the leslie MUX channels and set the drawbar values
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= LESLIE_POLLING_INTERVAL) {
            Organ::read_leslie();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_leslie_change();
    }

    /// @brief
    /// @param callback a function to run on change of the leslie state
    void set_on_leslie_change(void (*callback)()) {
        on_leslie_change = callback;
    }

    Organ::Speed leslieSpeed = Organ::Speed::Slow;
    boolean isStopped = true;

  private:
    elapsedMillis millis = LESLIE_POLLING_INTERVAL;
    void (*on_leslie_change)() = nullptr;

    void handle_leslie_change() {
        bool change = false;

        if (Organ::current_leslie_state.isStopped != isStopped) {
            change = true;
            isStopped = Organ::current_leslie_state.isStopped;
            if (isStopped == true) {
                DEBUG_PRINTLN("leslie stopped");
            }
        } if (Organ::current_leslie_state.leslieSpeed != leslieSpeed) {
            change = true;
            leslieSpeed = Organ::current_leslie_state.leslieSpeed;
            if (isStopped == false) {
                DEBUG_PRINT("leslie speed :: ");
                DEBUG_PRINTLN((int)Organ::current_leslie_state.leslieSpeed ? "slow" : "fast");
            }
        }

        if (on_leslie_change != nullptr && change) {
            on_leslie_change();
        }
    }
};

#endif