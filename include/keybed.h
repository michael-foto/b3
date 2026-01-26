/**
 * Class to poll the keybed shift registers and return an array of pressed key
 * The keybed's job
 */

#ifndef KEYBED_H
#define KEYBED_H

#include <Arduino.h>
#include <bitset>
#include <vector>

#define POLLING_INTERVAL (10)
#define NUM_ROWS (6)
#define NUM_COLS (11)
#define NUM_KEYBEDS (2)
#define LATCH_PIN (18)

class Keybed {
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
        // if the polling interval is exceeded then repoll
        if (millis >= POLLING_INTERVAL) {
            // set global
            current_key_state = SPI_decode();
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
    static std::array<uint64_t, NUM_KEYBEDS> current_key_state;

    uint8_t keybed_idx;
    uint64_t keybed_state;

    void (*on_keyPress)(uint8_t key) = nullptr;
    void (*on_keyUp)(uint8_t key) = nullptr;

    void handle_key_state_change() {
        // first find the keys that have changed
        uint64_t pressedKeys = current_key_state[keybed_idx] & ~keybed_state;
        uint64_t releasedKeys = keybed_state & ~current_key_state[keybed_idx];
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
        keybed_state = current_key_state[keybed_idx];
    }

    static std::array<uint64_t, NUM_KEYBEDS> SPI_decode() {
        // two keybeds
        std::array<uint64_t, NUM_KEYBEDS> keybed = {0x0};

        for (int row = 0; row < NUM_ROWS; row++) {
            // write to the HC595
            digitalWriteFast(LATCH_PIN, LOW);
            SPI.transfer(1 << row);
            digitalWriteFast(LATCH_PIN, HIGH);
            // allow time for the data to be written to the 165s register
            delayMicroseconds(1);
            // latch data
            digitalWriteFast(LATCH_PIN, LOW);

            // number of bytes needed to store the data (add 7 so that we round up)
            int num_bytes = ((NUM_COLS * NUM_KEYBEDS) + 7) / 8;

            /*
             * Read the SPI bus one byte at a time and write 1s for the pressed
             * keys to the corresponding key value in the array for the keybed
             *
             * We have to multiply by NUM_ROWS to get the corresponding key.
             * I.e. key = COL * NUM_ROWS + ROW.
             */
            for (char i = 0; i < num_bytes; i++) {
                uint8_t data = SPI.transfer(0x0);
                while (data != 0) {
                    // first index of set bit
                    char idx = __builtin_ctzll(data);
                    char col = (idx + (i * 8));
                    // Set the keybed for the corresponding byte
                    keybed[col / NUM_COLS] |= 0x1 << ((col * NUM_ROWS) + row);
                    // unset the idx bit
                    data &= (data - 1);
                }
            }

            // just debug the top keybed
            std::string binary_string = std::bitset<64>(keybed[0]).to_string();
            Serial.print(binary_string.c_str());
        }
        Serial.println();
        return keybed;
    }
};

#endif