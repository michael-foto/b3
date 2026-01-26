/*
 * Teensy hardware methods
 * Contains all the methods to directly read and write to hardware controlling
 * the organ
 */

#ifndef ORGAN_H
#define ORGAN_H

#include <Arduino.h>
#include <SPI.h>
#include <bitset>

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#define NUM_ROWS (6)
#define NUM_COLS (11)
#define NUM_KEYBEDS (2)
#define NUM_DRAWBARS (18)

#define DRAWBAR_LOWER_PIN (A0)
#define DRAWBAR_UPPER_PIN (A1)
#define DRAWBAR_MUX_PIN0 (0)
#define LATCH_PIN (18)

namespace Organ {

enum class PercussionSpeed {
    Fast,
    Slow
};

enum class PercussionVolume {
    Soft,
    Loud
};

enum class PercussionHarmonic {
    Second,
    Third
};

typedef struct {
    PercussionSpeed speed;
    PercussionHarmonic type;
    PercussionVolume volume;
    boolean on;
} Percussion;

enum class VibratoMode {
    V1 = 1,
    V2,
    V3,
    C1,
    C2,
    C3,
};

typedef struct {
    VibratoMode mode;
    boolean upper;
    boolean lower;
} Vibrato;

std::array<uint64_t, NUM_KEYBEDS> current_key_state = {0x0};
std::array<uint16_t, NUM_DRAWBARS> current_drawbar_state = {0x0};
Vibrato upper_vibrato = {VibratoMode::V1, false, 1};

void serial_init() {
    Serial.begin(9600);
}

void keybed_init() {
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(MISO, INPUT_PULLDOWN);

    SPI.begin();
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
}

void drawbars_init() {
    // analog drawbars
    pinMode(DRAWBAR_LOWER_PIN, INPUT);
    pinMode(DRAWBAR_UPPER_PIN, INPUT);
    pinMode(DRAWBAR_MUX_PIN0, OUTPUT);
    pinMode(DRAWBAR_MUX_PIN0 + 1, OUTPUT);
    pinMode(DRAWBAR_MUX_PIN0 + 2, OUTPUT);
}

void percussion_init() {
}

void SPI_update_key_state() {
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
        DEBUG_PRINT(std::bitset<64>(keybed[0]).to_string().c_str());
    }
    DEBUG_PRINTLN();
    current_key_state = keybed;
}

void read_drawbars() {
    for (uint8_t i = 0; i < 9; i++) {
        // encode the select pin
        digitalWrite(DRAWBAR_MUX_PIN0 + 0, (i >> 3) & 1);
        digitalWrite(DRAWBAR_MUX_PIN0 + 1, (i >> 2) & 1);
        digitalWrite(DRAWBAR_MUX_PIN0 + 2, (i >> 1) & 1);
        digitalWrite(DRAWBAR_MUX_PIN0 + 3, (i >> 0) & 1);
        delayMicroseconds(100);

        // write the raw value into the array
        current_drawbar_state[i] = analogRead(DRAWBAR_LOWER_PIN);
        current_drawbar_state[i + 9] = analogRead(DRAWBAR_UPPER_PIN);

        DEBUG_PRINT("drawbar raw values: ");
        DEBUG_PRINT(value >> 7);
        DEBUG_PRINT(", ");
    }
    DEBUG_PRINTLN();
}

}; // namespace Organ

#endif