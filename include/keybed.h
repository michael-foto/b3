/**
 * Class to poll the keybed shift registers and return an array of pressed key
 * The keybed's job
 */

#ifndef KEYBED_H
#define KEYBED_H

#include <Arduino.h>
#include <ISystem.h>
#include <organ.h>

#define KEYBED_POLLING_INTERVAL (8)

class Keybed : public ISystem {
  public:
    /// @brief create a keybed instance to handle note changes
    /// @param id the array index of the keybed matrix as scanned
    Keybed(uint8_t id) : keybed_idx(id) {
        Organ::keybed_init();
        millis = 0;
    }

    /// @brief the current state of the keybed. 0 is up and 1 is pressed.
    /// each bit index corresponds to the given key from 0-61
    uint64_t keybed_state = 0;

    /// @brief trigger the SPI routine to poll the current key state, compare
    /// to the previous, and dispatch messages to the keybed's subscribers as
    /// required
    void update() {
        // // DEBUG_PRINTLN("Inside :: Keybed::update");
        // if the polling interval is exceeded then re-read the keybeds
        if (millis >= KEYBED_POLLING_INTERVAL) {
            Organ::SPI_update_key_state();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_key_state_change();
        // DEBUG_PRINTLN("Exiting :: Keybed::update");
    }

    /// @brief Fires the callback when a key is pressed
    /// @param callback function when a key press is triggered
    void set_handle_key_pressed(void (*callback)(uint8_t keybed_idx, uint8_t key)) {
        on_keyPress = callback;
    }

    /// @brief Fires the callback when a key is pressed
    /// @param callback function when a key is released
    void set_handle_key_released(void (*callback)(uint8_t keybed_idx, uint8_t key)) {
        on_keyUp = callback;
    }

  private:
    elapsedMillis millis = 0;

    const uint8_t keybed_idx;
    void (*on_keyPress)(uint8_t keybed_idx, uint8_t key) = nullptr;
    void (*on_keyUp)(uint8_t keybed_idx, uint8_t key) = nullptr;

    void handle_key_state_change() {
        // DEBUG_PRINTLN("Inside :: Keybed::handle_key_state_change");
        // first find the keys that have changed
        uint64_t pressedKeys = Organ::current_key_state[keybed_idx] & ~keybed_state;
        uint64_t releasedKeys = keybed_state & ~Organ::current_key_state[keybed_idx];
        int keyId;

        // update the local state before calling the hooks
        keybed_state = Organ::current_key_state[keybed_idx];

        // variation of Brian Kernighan's algorithm to get the indices of set bits.
        // These correspond to the currently pressed keys
        // We do this twice for the pressed keys, and the released keys
        while (pressedKeys != 0) {
            // get the index of the rightmost set bit
            keyId = __builtin_ctzll(pressedKeys);
            if (on_keyPress != nullptr) {
                on_keyPress(keybed_idx, keyId);
            }
            // Clear the least significant set bit
            pressedKeys &= (pressedKeys - 1);
        }

        while (releasedKeys != 0) {
            keyId = __builtin_ctzll(releasedKeys);
            if (on_keyUp != nullptr) {
                on_keyUp(keybed_idx, keyId);
            }
            releasedKeys &= (releasedKeys - 1);
        }
        // DEBUG_PRINTLN("Inside :: Keybed::handle_key_state_change");
    }
};

#endif