#include <Arduino.h>

#include <Audio.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Wire.h>
#include <memory>

#include "drawbars.h"
#include "keybed.h"
#include "leslie.h"
#include "manual.h"
#include "percussion.h"
#include "vibrato_system.h"

#include "monitor_audio.h"
#include "tonewheel_osc_audio.h"
#include "vibrato_audio.h"

#define DEBUG

#pragma region Audio Connections
AudioMixer4 organOut;
// combined raw and vibrato tonewheel signals
AudioMixer4 tonewheels_mix;
TonewheelOsc tonewheels;
Monitor tonewheelsMonitor;
Vibrato vibrato;

AudioConnection patchCord0(tonewheels, tonewheels_mix);
AudioConnection patchCord1(tonewheels_mix, tonewheelsMonitor);

AudioConnection patchCord2(tonewheels, 0, organOut, 0);

AudioConnection patchCord3(tonewheels, 1, vibrato, 0);
AudioConnection patchCord4(vibrato, 0, organOut, 1);

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
VibratoSystem *vibrato_system;
Leslie *leslie;

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
    // reset percussion
    memset(Organ::percussion_drawbars, 0, sizeof Organ::percussion_drawbars);

    if (percussion_system->on) {
        if (percussion_system->type == Organ::PercussionHarmonic::Third) {
            Organ::percussion_drawbars[5] = 8;
        } else {
            Organ::percussion_drawbars[4] = 8;
        }
    }

    // reset the current tonewheel volumes
    tonewheels.clear();
    percussion.clear();

    // clear the arrays
    memset(Organ::percussion_volumes, 0, sizeof Organ::percussion_volumes);
    memset(Organ::upper_tonewheel_volumes, 0, sizeof Organ::upper_tonewheel_volumes);
    memset(Organ::lower_tonewheel_volumes, 0, sizeof Organ::lower_tonewheel_volumes);

    // Percussion only functions for the upper keybed
    manual_fill_volumes(upperKeybed->keybed_state, Organ::percussion_drawbars, Organ::percussion_volumes);
    percussion.setVolumes(Organ::percussion_volumes);

    manual_fill_volumes(upperKeybed->keybed_state, drawbars->upper.data(), Organ::upper_tonewheel_volumes);
    if (vibrato_system->upper) {
        tonewheels.setVibratoVolumes(Organ::upper_tonewheel_volumes);
    } else {
        tonewheels.setVolumes(Organ::upper_tonewheel_volumes);
    }

    manual_fill_volumes(lowerKeybed->keybed_state, drawbars->lower.data(), Organ::lower_tonewheel_volumes);
    if (vibrato_system->lower) {
        tonewheels.setVibratoVolumes(Organ::lower_tonewheel_volumes);
    } else {
        tonewheels.setVolumes(Organ::lower_tonewheel_volumes);
    }
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
        organOut.gain(2, 1.5);
    } else {
        organOut.gain(2, 3);
    }

    update_tonewheels();
}

void handle_vibrato_change() {
    vibrato.setMode(vibrato_system->mode);
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
    if (keybed_idx == 1 &&
        // If current key state is a power of 2, only one key is pressed
        (Organ::current_key_state[keybed_idx] & (Organ::current_key_state[keybed_idx] - 1)) == 0 &&
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
    if (keybed_idx == 1 &&
        // I.e., if this is 0, all keys released
        Organ::current_key_state[keybed_idx] == 0 &&
        percussion_system->on) {
        percussionEnv.noteOff();
    }
}

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

    Serial.print("vibrato=");
    Serial.print(vibrato.processorUsage());
    Serial.print(",");
    Serial.print(vibrato.processorUsageMax());
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

void setup() {
    Organ::serial_init();
    AudioMemory(5);

    upperKeybed = new Keybed(1);
    upperKeybed->set_handle_key_pressed(handle_note_on);
    upperKeybed->set_handle_key_released(handle_note_off);
    systems.push_back(upperKeybed);

    lowerKeybed = new Keybed(0);
    lowerKeybed->set_handle_key_pressed(handle_note_on);
    lowerKeybed->set_handle_key_released(handle_note_off);
    systems.push_back(lowerKeybed);

    drawbars = new Drawbars();
    drawbars->set_on_drawbar_change(update_tonewheels);
    systems.push_back(drawbars);

    percussion_system = new Percussion();
    percussion_system->set_on_percussion_change(handle_percussion_change);
    systems.push_back(percussion_system);

    vibrato_system = new VibratoSystem();
    vibrato_system->set_on_vibrato_change(handle_vibrato_change);
    systems.push_back(vibrato_system);

    leslie = new Leslie();
    leslie->set_on_leslie_change(Organ::write_leslie_output);
    systems.push_back(leslie);

    tonewheels.init();
    percussion.init();
    percussionEnv.noteOff();
    vibrato.init();

    swell.gain(1.0);

    organOut.gain(0, 3); // no vibrato
    organOut.gain(1, 3); // with vibrato
    organOut.gain(2, 3); // percussionEnv

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
    antialias.setLowpass(0, 1000, 0.707);
    // antialias.setLowpass(1, 3000, 0.707);
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