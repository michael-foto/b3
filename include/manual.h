/* Copyright (c) 2018 Peter Teichman */

#ifndef MANUAL_H
#define MANUAL_H

#include <Arduino.h>

// manual is here to maintain the mapping between physical keys on the
// manuals of an organ and the tonewheel oscillator. It takes key
// events and translates those to tonewheel volumes.
//
// It is aware of several physical properties of the Hammond B-3: the
// resistances of each wiring between keys and tonewheels, the
// resistances of each drawbar setting, and the factory recommended
// voltage from each tonewheel.

#define MAX_TONEWHEEL_VOLUME (1U << 15)

// resistance & friends return the resistance of the wire (in ohms)
// connected to the tonewheel for key + drawbar.
float resistance(int key, int drawbar);
float resistance1(int key);
float resistance2(int key);
float resistance3(int key);
float resistance4(int key);
float resistance5(int key);
float resistance6(int key);
float resistance7(int key);
float resistance8(int key);
float resistance9(int key);

// voltages taken from the "Post 1956 TG" table at HammondWiki. Using
// the Vpp levels, which feels right given that we're adding peak to
// peak volumes, but my EE memory is hazy here.
// http://www.dairiki.org/HammondWiki/ToneWheelGeneratorOutputLevels
float voltages[92] = {
    // Add an empty first element so voltages can be 1-indexed.
    0,
    70.0,
    69.2,
    68.3,
    67.3,
    66.4,
    65.5,
    64.5,
    63.6,
    62.6,
    61.7,
    60.8,
    60.0,
    15.0,
    14.6,
    14.3,
    14.0,
    13.6,
    13.3,
    13.0,
    12.6,
    12.3,
    12.0,
    11.6,
    11.3,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.0,
    11.1,
    11.2,
    11.3,
    11.4,
    11.5,
    11.7,
    11.8,
    12.0,
    12.2,
    12.5,
    12.8,
    13.0,
    13.2,
    13.4,
    13.6,
    14.0,
    14.2,
    14.5,
    14.7,
    15.1,
    15.2,
    15.6,
    15.8,
    16.0,
    16.3,
    16.6,
    17.0,
    17.3,
    17.7,
    18.0,
    18.5,
    18.8,
    19.2,
    19.4,
    19.6,
    19.8,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    20.0,
    19.7,
    19.3,
    19.0,
    18.7,
    18.3,
    18.0,
};

// foldback wraps calculated tonewheel values outside of the keyboard
// range (13..92) back into that range. The 1..13 tonewheels are
// controlled by the Hammond foot pedals, which is why they're not
// included here.
int foldback(uint8_t tonewheel) {
    while (tonewheel < 13) {
        tonewheel += 12;
    }
    while (tonewheel > 91) {
        tonewheel -= 12;
    }
    return tonewheel;
}

// tonewheel returns the number of the tonewheel connected to _key_ at
// _drawbar_.
int tonewheel(int key, int drawbar) {
    switch (drawbar) {
    case 1: // Sub-octave; 16'
        return foldback(key);
    case 2: // 5th; 5 1/3'
        return foldback(key + 19);
    case 3: // Unison; 8'
        return foldback(key + 12);
    case 4: // 8th (Octave); 4'
        return foldback(key + 24);
    case 5: // 12th; 2 2/3'
        return foldback(key + 31);
    case 6: // 15th; 2'
        return foldback(key + 36);
    case 7: // 17th; 1 3/5'
        return foldback(key + 40);
    case 8: // 19th; 1 1/3'
        return foldback(key + 43);
    case 9: // 22nd; 1'
        return foldback(key + 48);
    }
    return 0;
}

// Q19-scaled gain mapping for each drawbar stop
static const uint32_t draw_gain_q19[9] = {
    0,    // level 0
    298,  // 1.414
    421,  // 2.0
    596,  // 2.828
    1057, // 5.0
    1195, // 5.657
    1690, // 8.0
    2390, // 11.31
    3392  // 16.0
};

// manual_fill_volumes returns the current set of tonewheel volumes,
// with values in the Q14 range. keys is an array of 61 keys on a
// manual, one-indexed and nonzero if pressed. drawbars contains the
// resistance at each of the 9 drawbars, also one-indexed.
//
// drawbars[1]: 16' (sub-octave)
// drawbars[2]: 5 1/3' (5th)
// drawbars[3]: 8' (unison)
// drawbars[4]: 4' (8th)
// drawbars[5]: 2 2/3' (12th)
// drawbars[6]: 2' (15th)
// drawbars[7]: 1 3/5' (15th)
// drawbars[8]: 1 1/3' (19th)
// drawbars[9]: 1' (22nd)
void manual_fill_volumes(uint64_t keys, uint8_t drawbars[10], uint32_t ret[92]) {
    for (int k = 0; k < 61; k++) {
        if (!(keys & (1ULL << k))) {
            continue;
        }

        for (int d = 1; d < 10; d++) {
            uint8_t level = drawbars[d];
            if (!level) {
                continue;
            }

            int t = tonewheel(k + 1, d); // map key+drawbar to tonewheel index

            // prevent overflow
            ret[t] += ret[t] + draw_gain_q19[level];
            if (ret[t] > (MAX_TONEWHEEL_VOLUME)) {
                ret[t] = MAX_TONEWHEEL_VOLUME;
            }
        }
    }
    return;
}

#endif