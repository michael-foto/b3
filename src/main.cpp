#include <Arduino.h>

#include <Audio.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Wire.h>
#include <memory>

#include "drawbars.h"
#include "keybed.h"
#include "manual.h"
#include "percussion.h"
#include "vibrato_system.h"

#include "monitor_audio.h"
#include "tonewheel_osc_audio.h"
#include "vibrato_audio.h"

#define DEBUG

#pragma region Audio Connections
AudioMixer4 organOut;
TonewheelOsc tonewheels;
Monitor tonewheelsMonitor;
Vibrato upperVibrato;
Vibrato lowerVibrato;

AudioConnection patchCord0(tonewheels, 0, tonewheelsMonitor, 0);
AudioConnection patchCord1(tonewheelsMonitor, 0, upperVibrato, 0);
AudioConnection patchCord2(tonewheelsMonitor, 0, lowerVibrato, 0);
AudioConnection patchCord3(upperVibrato, 0, organOut, 0);
AudioConnection patchCord4(lowerVibrato, 0, organOut, 1);

TonewheelOsc percussion;
AudioEffectEnvelope percussionEnv;

AudioConnection patchCord5(percussion, 0, percussionEnv, 0);
AudioConnection patchCord6(percussionEnv, 0, organOut, 2);

AudioAmplifier swell;
AudioConnection patchCord7(organOut, swell);

// This antialias filter is here to band limit the organ signal, in
// case key click transients are too high frequency, and also to give
// a slight reduction in key click.
AudioFilterBiquad antialias;
AudioConnection patchCord8(swell, antialias);

// Teensy DAC output
AudioOutputAnalog dac;
AudioConnection patchCord9(antialias, dac);
#pragma endregion

std::vector<ISystem *> systems;

Keybed *upperKeybed;
Keybed *lowerKeybed;
Drawbars *drawbars;
Percussion *percussion_system;
VibratoSystem *vibrato;

// TODO later: add MIDI out via USB
// MIDI key values
#define MANUAL_KEY_0 (35)
#define MANUAL_KEY_61 (MANUAL_KEY_0 + 61)

// MIDI controller names
#define CC_SWELL (11)
#define CC_RESET (46)
#define CC_DRAWBAR_0 (69)
#define CC_DRAWBAR_9 (CC_DRAWBAR_0 + 9)
#define CC_ROTARY_SPEED (82)
#define CC_ROTARY_STOP (79)
#define CC_VIBRATO_MODE (84)
#define CC_PERCUSSION (87)
#define CC_PERCUSSION_FAST (88)
#define CC_PERCUSSION_SOFT (89)
#define CC_PERCUSSION_THIRD (95) // is this correct?
#define CC_VIBRATO (107)
#define CC_SPEAKER_DRIVE (111)


void update_tonewheels() {
    if (percussion_system->on) {
        if (percussion_system->type == Organ::PercussionHarmonic::Third) {
            Organ::percussion_drawbars[5] = 8;
        } else {
            Organ::percussion_drawbars[4] = 8;
        }
    }

    // clear the arrays
    memset(Organ::percussion_volumes, 0, sizeof Organ::percussion_volumes);
    memset(Organ::tonewheel_volumes, 0, sizeof Organ::tonewheel_volumes);

    // Percussion only functions for the upper keybed
    manual_fill_volumes(upperKeybed->keybed_state, Organ::percussion_drawbars, Organ::percussion_volumes);
    percussion.setVolumes(Organ::percussion_volumes);

    manual_fill_volumes(upperKeybed->keybed_state, drawbars->upper.data(), Organ::tonewheel_volumes);
    tonewheels.setVolumes(Organ::tonewheel_volumes);

    manual_fill_volumes(lowerKeybed->keybed_state, drawbars->lower.data(), Organ::tonewheel_volumes);
    tonewheels.setVolumes(Organ::tonewheel_volumes);
}

void handle_percussion_change() {
    percussionEnv.delay(0.0);
    percussionEnv.attack(0.1);
    percussionEnv.sustain(0.0);
    percussionEnv.release(0.0);

    if (percussion_system->speed == Organ::Speed::Fast) {
        percussionEnv.decay(300.0);
    } else {
        percussionEnv.decay(630.0);
    }

    if (percussion_system->volume == Organ::PercussionVolume::Soft) {
        organOut.gain(2, 0.25);
    } else {
        organOut.gain(2, 0.50);
    }

    update_tonewheels();
}

void handle_note_on(uint8_t keybed_idx, uint8_t key) {
    DEBUG_PRINT("keybed: ");
    DEBUG_PRINT(keybed_idx);
    DEBUG_PRINT(", Key pressed: ");
    DEBUG_PRINT(key);
    DEBUG_PRINTLN();

    if (key < 0 || key >= 61) {
        return;
    }

    update_tonewheels();

    // Top keyboard only
    if (keybed_idx == 0 &&
        // current key state has not been updated to the new state yet.
        // I.e., if this is 0, then this is the first keypress
        Organ::current_key_state[keybed_idx] == 0 &&
        percussion_system->on) {
        percussionEnv.noteOn();
    }
}

void handle_note_off(uint8_t keybed_idx, uint8_t key) {
    DEBUG_PRINT("keybed: ");
    DEBUG_PRINT(keybed_idx);
    DEBUG_PRINT(", Key released: ");
    DEBUG_PRINT(key);
    DEBUG_PRINTLN();

    if (key < 0 || key >= 61) {
        return;
    }

    update_tonewheels();

    // Top keyboard only
    if (keybed_idx == 0 &&
        // current key state has not been updated to the new state yet.
        // I.e., if this is a power of two then only one key was pressed - it must have been released
        (Organ::current_key_state[keybed_idx] & (Organ::current_key_state[keybed_idx] - 1)) &&
        percussion_system->on) {
        percussionEnv.noteOff();
    }
}

/*
// handleControlChange is compatible (where possible) with the Nord
// Electro 3 MIDI implementation:
// http://www.nordkeyboards.com/sites/default/files/files/downloads/manuals/nord-electro-3/Nord%20Electro%203%20English%20User%20Manual%20v3.x%20Edition%203.1.pdf
void handleControlChange(byte chan, byte ctrl, byte val) {
    if (ctrl == 1) {
        // Skip logging aftertouch messages, so the serial log isn't
        // spammed with them.
        return;
    }

    Serial.print("Control Change, ch=");
    Serial.print(chan, DEC);
    Serial.print(", control=");
    Serial.print(ctrl, DEC);
    Serial.print(", value=");
    Serial.print(val, DEC);
    Serial.println();

    if (ctrl == CC_SWELL) {
        swell.gain(remap((float)val, 0, 127, 0, 2.5));
    } else if (ctrl == CC_PERCUSSION) {
        handle_percussion_change();
        update_tonewheels();
    } else if (ctrl == CC_PERCUSSION_FAST) {
        handle_percussion_change();
    } else if (ctrl == CC_PERCUSSION_SOFT) {
        handle_percussion_change();
    } else if (ctrl > CC_DRAWBAR_0 && ctrl <= CC_DRAWBAR_9) {
        update_tonewheels();
    } else if (ctrl == CC_ROTARY_STOP || ctrl == CC_ROTARY_SPEED) {
        updateLeslieRotation();
    } else if (ctrl == CC_VIBRATO || ctrl == CC_VIBRATO_MODE) {
        updateVibrato();
    }
}
*/

#pragma region DEBUG
void DEBUG_showKeys() {
    Serial.print("upper keys :: ");
    Serial.print(Organ::current_key_state[1]);
    Serial.print("\n");
    Serial.print("lower keys :: ");
    Serial.print(Organ::current_key_state[0]);
    Serial.print("\n");
}

void DEBUG_showVolumes(uint16_t volumes[92]) {
    for (int i = 0; i < 92; i++) {
        Serial.print("volumes[");
        Serial.print(i);
        Serial.print("] = ");
        Serial.print(volumes[i]);
        Serial.print("\n");
    }
}

/// @brief Print status to the debug console
void DEBUG_status() {
    Serial.print("CPU: ");
    Serial.print("tonewheels=");
    Serial.print(tonewheels.processorUsage());
    Serial.print(",");
    Serial.print(tonewheels.processorUsageMax());
    Serial.print("  ");

    Serial.print("upper vibrato=");
    Serial.print(upperVibrato.processorUsage());
    Serial.print(",");
    Serial.print(upperVibrato.processorUsageMax());
    Serial.print("  ");

    Serial.print("lower vibrato=");
    Serial.print(lowerVibrato.processorUsage());
    Serial.print(",");
    Serial.print(lowerVibrato.processorUsageMax());
    Serial.print("  ");

    Serial.print("antialias=");
    Serial.print(antialias.processorUsage());
    Serial.print(",");
    Serial.print(antialias.processorUsageMax());
    Serial.print("  ");

    Serial.print("all=");
    Serial.print(AudioProcessorUsage());
    Serial.print(",");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("    ");

    Serial.print("Memory: ");
    Serial.print(AudioMemoryUsage());
    Serial.print(",");
    Serial.print(AudioMemoryUsageMax());
    Serial.print("    ");
    Serial.println();
}

/// @brief Print volume and tonewheel status to the debug console
void DEBUG_statusVolume() {
    Serial.print("Volume: ");
    Serial.print("tonewheels=");
    Serial.print(tonewheelsMonitor.volumeUsageMin());
    Serial.print(",");
    Serial.print(tonewheelsMonitor.volumeUsageMax());
    Serial.print("    ");
    Serial.print(tonewheelsMonitor.volumeUsageMinEver());
    Serial.print(",");
    Serial.print(tonewheelsMonitor.volumeUsageMaxEver());
    Serial.println();

    tonewheelsMonitor.reset();
}

/*
/// @brief Print volume and tonewheel status to the debug console
void DEBUG_statusPerc() {
    Serial.print("percOn=");
    Serial.print(midiControl[CC_PERCUSSION]);
    Serial.print("    ");
    Serial.print("percFast=");
    Serial.print(midiControl[CC_PERCUSSION_FAST]);
    Serial.print("    ");
    Serial.print("percSoft=");
    Serial.print(midiControl[CC_PERCUSSION_SOFT]);
    Serial.print("    ");
    Serial.print("percThird=");
    Serial.print(midiControl[CC_PERCUSSION_THIRD]);
    Serial.print("    ");
    Serial.println();
}
*/

void setup() {
    Organ::serial_init();
    AudioMemory(5);

    upperKeybed = new Keybed(1);
    upperKeybed->setHandleKeyPressed(handle_note_on);
    upperKeybed->setHandleKeyReleased(handle_note_off);
    systems.push_back(upperKeybed);

    lowerKeybed = new Keybed(0);
    lowerKeybed->setHandleKeyPressed(handle_note_on);
    lowerKeybed->setHandleKeyReleased(handle_note_off);
    systems.push_back(lowerKeybed);

    drawbars = new Drawbars();
    drawbars->setOnDrawbarChange(update_tonewheels);
    systems.push_back(drawbars);

    percussion_system = new Percussion();
    percussion_system->setOnpercussionChange(handle_percussion_change);
    systems.push_back(percussion_system);

    vibrato = new VibratoSystem();
    vibrato->setOnvibratoChange(update_tonewheels);
    systems.push_back(vibrato);

    tonewheels.init();
    percussion.init();
    upperVibrato.init();
    lowerVibrato.init();

    swell.gain(1.0);

    organOut.gain(0, 0.5);  // tonewheels + upper vibrato
    organOut.gain(1, 0.5);  // tonewheels + lower vibrato
    organOut.gain(2, 0.50); // percussionEnv

    // The antialias filter is here for two purposes:
    //
    // 1) To band limit the output of the organ, just in case it
    // produces something above our Nyquist frequency (22050 Hz)
    //
    // 2) To cut the transients when turning on new tonewheels,
    // reducing key click.
    //
    // 3) to normalise the loudness of the higher tonewheels for
    // a more balanced output
    antialias.setLowpass(0, 2150, 0.707);
    // antialias.setLowpass(1, 1025, 0.5);
}

int count = 0;
void loop() {
    // DEBUG_PRINTLN("BEGIN LOOP");
    // Update each system in the organ
    for (const auto &system : systems) {
        system->update();
    }

    // Dump debug messages every 500000 loop iterations
    if ((count++ % 500000) == 0) {
        DEBUG_status();
        DEBUG_statusVolume();
    }
}