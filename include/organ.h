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

#define DEBUG

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
#define LATCH_PIN_I (21)

#define PERCUSSION_ON_PIN (7)
#define PERCUSSION_VOL_PIN (6)
#define PERCUSSION_SPEED_PIN (5)
#define PERCUSSION_HARMONIC_PIN (4)

#define VIBRATO_SELECT_PIN (A2)
#define VIBRATO_UPPER_PIN (9)
#define VIBRATO_LOWER_PIN (8)

#define ROTO_FAST_PIN (10)
#define ROTO_SLOW_PIN (24)

#define LESLIE_SPEED_OUT_PIN (19)
#define LESLIE_STOP_OUT_PIN (20)

namespace Organ {

enum class Speed {
    Fast,
    Slow
};

enum class PercussionVolume {
    Soft,
    Normal
};

enum class PercussionHarmonic {
    Second,
    Third
};

typedef struct {
    Speed speed;
    PercussionHarmonic type;
    PercussionVolume volume;
    boolean on;
} Percussion;

enum class VibratoMode {
    V1 = 1,
    C1,
    V2,
    C2,
    V3,
    C3,
};

typedef struct {
    VibratoMode mode;
    boolean upper;
    boolean lower;
} Vibrato;

typedef struct {
    Speed leslieSpeed;
    boolean isStopped;
} Leslie;

/// @brief current key state for top and bottom keybeds. 64-bit integer
/// MSB-first, where the first bit responds to the first key, up to 61st bit
/// for the last key. 1 is pressed, 0 is released
std::array<uint64_t, NUM_KEYBEDS> current_key_state = {0x0, 0x0};

/// @brief current drawbar levels from 0-7. 0-indexed. first 9 are for the top
/// manual, 2nd 9 for the bottom
std::array<uint8_t, NUM_DRAWBARS> current_drawbar_state = {0x0, 0x0};

boolean keybed_initialised = false;

Vibrato current_vibrato_state = {VibratoMode::C1, false, false};
Percussion current_percussion_state = {};
Leslie current_leslie_state = {Speed::Slow, true};

uint32_t upper_tonewheel_volumes[92] = {0};
uint32_t lower_tonewheel_volumes[92] = {0};
uint8_t percussion_drawbars[10] = {0};
uint32_t percussion_volumes[92] = {0};

void serial_init() {
    Serial.begin(1000);
}

void keybed_init() {
    // DEBUG_PRINTLN("inside :: keybed_init");
    if (keybed_initialised) {
        return;
    }

    pinMode(LATCH_PIN, OUTPUT);
    pinMode(LATCH_PIN_I, OUTPUT);
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(MISO, INPUT_PULLDOWN);

    SPI.begin();
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

    keybed_initialised = true;
}

void drawbars_init() {
    // analog drawbars
    pinMode(DRAWBAR_LOWER_PIN, INPUT);
    pinMode(DRAWBAR_UPPER_PIN, INPUT);
    pinMode(DRAWBAR_MUX_PIN0, OUTPUT);
    pinMode(DRAWBAR_MUX_PIN0 + 1, OUTPUT);
    pinMode(DRAWBAR_MUX_PIN0 + 2, OUTPUT);
    pinMode(DRAWBAR_MUX_PIN0 + 3, OUTPUT);
}

void percussion_init() {
    // percussion switches
    pinMode(PERCUSSION_ON_PIN, INPUT_PULLUP);
    pinMode(PERCUSSION_VOL_PIN, INPUT_PULLUP);
    pinMode(PERCUSSION_SPEED_PIN, INPUT_PULLUP);
    pinMode(PERCUSSION_HARMONIC_PIN, INPUT_PULLUP);
}

void vibrato_init() {
    // vibrator selectors
    pinMode(VIBRATO_SELECT_PIN, INPUT);
    pinMode(VIBRATO_UPPER_PIN, INPUT_PULLUP);
    pinMode(VIBRATO_LOWER_PIN, INPUT_PULLUP);
}

void leslie_init() {
    pinMode(ROTO_FAST_PIN, INPUT_PULLUP);
    pinMode(ROTO_SLOW_PIN, INPUT_PULLUP);
    pinMode(LESLIE_SPEED_OUT_PIN, OUTPUT);
    pinMode(LESLIE_STOP_OUT_PIN, OUTPUT);
}

/**
 * The top keybed has a bad hardware implementation meaning custom
 * mapping is needed. This implementation is hardware specific
 * and will not apply to other keybeds
 */
uint8_t map_keybed_keys(uint8_t col, uint8_t row) {
    // TODO: fix in hardware by re-soldering.
    // The top keybed's row matrix is out of order.
    // It goes 3 4 5 0 1 2. So this handles that case
    bool is_top_row = col / NUM_COLS;
    char parsed_row = is_top_row ? (row + 3) % 6 : row;

    char new_key = ((col * NUM_ROWS) + parsed_row);
    // keybed skips 101 - 106
    if (new_key > 100) {
        new_key = new_key - 7;
    }
    return (is_top_row ? new_key - 65 : new_key);
}

void SPI_update_key_state() {
    // DEBUG_PRINTLN("Inside :: Keybed::SPI_update_key_state");
    // two keybeds
    std::array<uint64_t, NUM_KEYBEDS> keybed = {0x0, 0x0};

    // number of bytes needed to store the data (add 7 so that we round up)
    const int num_bytes = ((NUM_COLS * NUM_KEYBEDS) + 7) / 8;

    for (char row = 0; row < NUM_ROWS; row++) {
        // write to the HC595
        digitalWriteFast(LATCH_PIN, LOW);
        digitalWriteFast(LATCH_PIN_I, HIGH);
        SPI.transfer(1 << row);
        digitalWriteFast(LATCH_PIN, HIGH);
        digitalWriteFast(LATCH_PIN_I, LOW);
        // allow time for the data to be written to the 165s register
        delayMicroseconds(10);
        // latch data
        digitalWriteFast(LATCH_PIN, LOW);
        digitalWriteFast(LATCH_PIN_I, HIGH);

        /*
         * Read the SPI bus one byte at a time and write 1s for the pressed
         * keys to the corresponding key value in the array for the keybed
         *
         * We have to multiply by NUM_ROWS to get the corresponding key.
         * I.e. key = COL * NUM_ROWS + ROW.
         */
        for (char i = num_bytes; i > 0; i--) {
            uint8_t data = SPI.transfer(0x0);
            // DEBUG_PRINT(std::bitset<8>(data).to_string().c_str());

            while (data != 0) {
                // first index of set bit
                char idx = __builtin_ctzll(data);
                char col = (idx + ((i - 1) * 8));

                char key_number = map_keybed_keys(col, row);

                // Set the keybed for the corresponding byte
                keybed[col / NUM_COLS] |= 1ULL << key_number;
                // unset the idx bit
                data &= (data - 1);
            }
        }
    }
    current_key_state = keybed;
    // DEBUG_PRINTLN("Exiting :: Keybed::SPI_update_key_state");
}

/// @brief mapping to take analog value of the drawbar sliders to a value in
/// the range 0-7.
/// @param raw_value the raw analog value
/// @return the drawbar volume level from 0-7
uint8_t quantize_drawbars(uint16_t raw_value) {
    // drawbars are reversed
    return min((0x400 - raw_value) / 113, 8);
}

void read_drawbars() {
    for (uint8_t i = 0; i < 9; i++) {
        // encode the select pin
        digitalWrite(DRAWBAR_MUX_PIN0 + 0, (i >> 0) & 1);
        digitalWrite(DRAWBAR_MUX_PIN0 + 1, (i >> 1) & 1);
        digitalWrite(DRAWBAR_MUX_PIN0 + 2, (i >> 2) & 1);
        digitalWrite(DRAWBAR_MUX_PIN0 + 3, (i >> 3) & 1);
        delayMicroseconds(100);

        // write the raw value into the array
        current_drawbar_state[17 - (i * 2)] = quantize_drawbars(analogRead(DRAWBAR_UPPER_PIN));
        current_drawbar_state[16 - (i * 2)] = quantize_drawbars(analogRead(DRAWBAR_LOWER_PIN));

        // disable the 9th drawbar if percussion is on
        if (current_percussion_state.on) {
            current_drawbar_state[8] = 0;
        }
    }
}

void read_percussion() {
    current_percussion_state.on = digitalRead(PERCUSSION_ON_PIN) == 1;
    current_percussion_state.volume = digitalRead(PERCUSSION_VOL_PIN) ? PercussionVolume::Soft : PercussionVolume::Normal;
    current_percussion_state.speed = digitalRead(PERCUSSION_SPEED_PIN) ? Speed::Fast : Speed::Slow;
    current_percussion_state.type = digitalRead(PERCUSSION_HARMONIC_PIN) ? PercussionHarmonic::Third : PercussionHarmonic::Second;
}

void read_vibrato() {
    current_vibrato_state.upper = digitalRead(VIBRATO_UPPER_PIN);
    current_vibrato_state.lower = digitalRead(VIBRATO_LOWER_PIN);

    uint16_t raw = analogRead(VIBRATO_SELECT_PIN);
    Organ::VibratoMode mode;

    if (raw > 1000) {
        mode = Organ::VibratoMode::C1;
    } else if (raw > 800) {
        mode = Organ::VibratoMode::C3;
    } else if (raw > 600) {
        mode = Organ::VibratoMode::V3;
    } else if (raw > 400) {
        mode = Organ::VibratoMode::C2;
    } else if (raw > 200) {
        mode = Organ::VibratoMode::V2;
    } else {
        mode = Organ::VibratoMode::V1;
    }
    current_vibrato_state.mode = mode;
}

void read_leslie() {
}

void write_leslie_output() {
    digitalWrite(LESLIE_STOP_OUT_PIN, current_leslie_state.isStopped);
    digitalWrite(LESLIE_SPEED_OUT_PIN, current_leslie_state.leslieSpeed == Speed::Fast ? 1 : 0);
}

}; // namespace Organ

#endif