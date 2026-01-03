/**
 * Class to poll the keybed shift registers and return an array of pressed key
 */

#ifndef KEYBED_H
#define KEYBED_H

#define POLLING_INTERVAL (10)

class Keybed {
  public:
    Keybed() : keyState() {
    }

    void init();
    uint8_t *getKeyState() {
        if (millis >= POLLING_INTERVAL) {
            _pollKeyState();
            millis = 0;
        }
        return this->keyState;
    }

    /// @brief Fires the callback when a key is pressed
    /// @param callback function when a key press is triggered
    void setHandleKeyPressed(void (*callback)(uint8_t key));

    /// @brief Fires the callback when a key is pressed
    /// @param callback function when a key is released
    void setHandleKeyReleased(void (*callback)(uint8_t key));

  private:
    elapsedMillis millis;
    uint8_t keyState[61] = {0};
    void _pollKeyState();
};

#endif