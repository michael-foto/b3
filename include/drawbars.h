#ifndef DRAWBARS_H
#define DRAWBARS_H

#include <ISystem.h>

#define POLLING_INTERVAL (100)

class Drawbars : public ISystem {
  public:
    Drawbars();
    /// @brief Read the drawbar MUX channels and set the drawbar values
    void update() {
        // if the polling interval is exceeded then repoll
        if (millis >= POLLING_INTERVAL) {
            millis = 0;
        }
        // always handle state change comparing global to local
        handle_drawbar_change();
    }

    std::array<uint8_t, 10> upper();
    std::array<uint8_t, 10> upper();

  private:
    elapsedMillis millis = POLLING_INTERVAL;
};

#endif