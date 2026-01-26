/**
 * Class to poll the keybed shift registers and return an array of pressed key
 * The keybed's job
 */

#ifndef KEYBED_H
#define KEYBED_H

#include <Arduino.h>
#include <organ.h>
#include <ISystem.h>

#define POLLING_INTERVAL (10)

class Keybed : ISystem {
  public:
    /// @brief create a keybed instance to handle note changes
    /// @param shift_reg_bit the first bit of the shift register's matrix output
    /// that corresponds to this key
    /// E.g. for a 6x11 matrix, this will be the 11th bit (0-index);
    Keybed(uint8_t keybed_idx)
        : keybed_idx() {}

    /// @brief trigger the SPI routine to poll the current key state, compare
    /// to the previous, and dispatch messages to the keybed's subscribers as
    /// required
    void update() {
        // if the polling interval is exceeded then re-read the keybeds
        if (millis >= POLLING_INTERVAL) {
            Organ::SPI_update_key_state();
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_key_state_change();
    }

    /// @brief Fires the callback when a key is pressed
    /// @param callback function when a key press is triggered
    void setHandleKeyPressed(void (*callback)(uint8_t key)) {
        on_keyPress = callback;
    }

    /// @brief Fires the callback when a key is pressed
    /// @param callback function when a key is released
    void setHandleKeyReleased(void (*callback)(uint8_t key)) {
        on_keyUp = callback;
    }

  private:
    static elapsedMillis millis;

    uint8_t keybed_idx;
    uint64_t keybed_state;

    void (*on_keyPress)(uint8_t key) = nullptr;
    void (*on_keyUp)(uint8_t key) = nullptr;

    void handle_key_state_change() {
        // first find the keys that have changed
        uint64_t pressedKeys = Organ::current_key_state[keybed_idx] & ~keybed_state;
        uint64_t releasedKeys = keybed_state & ~Organ::current_key_state[keybed_idx];
        int keyId;

        // variation of Brian Kernighan's algorithm to get the indices of set bits.
        // These correspond to the currently pressed keys
        // We do this twice for the pressed keys, and the released keys
        while (pressedKeys != 0) {
            // get the index of the rightmost set bit
            keyId = __builtin_ctzll(pressedKeys);
            on_keyPress(keyId);
            // Clear the least significant set bit
            pressedKeys &= (pressedKeys - 1);
        }

        while (releasedKeys != 0) {
            keyId = __builtin_ctzll(releasedKeys);
            on_keyUp(keyId);
            releasedKeys &= (releasedKeys - 1);
        }

        // update the local state now
        keybed_state = Organ::current_key_state[keybed_idx];
    }
};

#endif