#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>

#include "amfm_audio.h"
#include "manual.h"
#include "monitor_audio.h"
#include "preamp_audio.h"
#include "tonewheel_osc_audio.h"
#include "vibrato_audio.h"
#include "keybed.h"
#include "drawbars.h"

#pragma region Audio Connections
AudioMixer4 organOut;
TonewheelOsc tonewheels;
Monitor tonewheelsMonitor;
Vibrato vibrato;

AudioConnection patchCord0(tonewheels, 0, tonewheelsMonitor, 0);
AudioConnection patchCord1(tonewheelsMonitor, 0, vibrato, 0);
AudioConnection patchCord2(vibrato, 0, organOut, 0);

TonewheelOsc percussion;
AudioEffectEnvelope percussionEnv;

AudioConnection patchCord3(percussion, 0, percussionEnv, 0);
AudioConnection patchCord4(percussionEnv, 0, organOut, 1);

AudioAmplifier swell;
AudioConnection patchCord5(organOut, 0, swell, 0);

// This antialias filter is here to band limit the organ signal, in
// case key click transients are too high frequency, and also to give
// a slight reduction in key click.
AudioFilterBiquad antialias;
AudioConnection patchCord6(swell, 0, antialias, 0);

// Teensy DAC output
AudioOutputAnalog dac;
AudioConnection patchCord7(antialias, 0, dac, 0);
#pragma endregion

Keybed* upperKeybed;
Keybed* lowerKeybed;

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

// Teensy input/outputs
// #define TEENSY_LESLIE_STOP (0)
// #define TEENSY_LESLIE_SPEED (0)

// numKeysDown is used to keep the percussion effect single triggered:
// only the first key down affects the percussion setting.
// TODO: we might not need this as we can just check if the keystate was 0 on any state change?
uint8_t numKeysDown = 0;

void handleNoteOn(byte chan, byte note, byte vel);
void handleNoteOff(byte chan, byte note, byte vel);
void handleControlChange(byte chan, byte ctrl, byte val);

// init restores everything to just-booted state:
// 1) it thinks all keys are up
// 2) drawbar registration is set to 888800000
// 3) percussion is off, vibrato is set to C1
// 4) the Leslie is set to slow
void init() {
    // Release all keys and reset all control settings.
    memset(midiKeys, 0, 127);
    memset(midiControl, 0, 127);

    // Poll and set all drawbars
    for (int i = 1; i <= 9; i++) {
        midiControl[CC_DRAWBAR_0 + i] = random(0, 127);
    }

    updatePercussionEnvelope();
    updateTonewheelVolumes();
    updateVibrato();

}

void setup() {
    Serial.begin(115200);

    AudioMemory(10);

    upperKeybed = new Keybed(0);
    lowerKeybed = new Keybed(11);

    tonewheels.init();
    percussion.init();
    vibrato.init();
    drawbars.init();


    swell.gain(1.0);

    organOut.gain(0, 0.50); // tonewheels + vibrato
    organOut.gain(1, 0.50); // percussionEnv
    organOut.gain(2, 0);
    organOut.gain(3, 0);

    // The antialias filter is here for two purposes:
    //
    // 1) To band limit the output of the organ, just in case it
    // produces something above our Nyquist frequency (22050 Hz)
    //
    // 2) To cut the transients when turning on new tonewheels,
    // reducing key click.
    antialias.setLowpass(0, 2150, 0.707);

    upperKeybed->setHandleKeyPressed(handleNoteOn);
    upperKeybed->setHandleKeyReleased(handleNoteOff);
    lowerKeybed->setHandleKeyPressed(handleNoteOn);
    lowerKeybed->setHandleKeyReleased(handleNoteOff);
}

int count = 0;
void loop() {
    upperKeybed->update();
    lowerKeybed->update();

    // Dump debug messages every 500000 loop iterations
    if ((count++ % 500000) == 0) {
        DEBUG_status();
        DEBUG_statusVolume();
    }
}

int note2key(byte note) {
    return (int)note - 35;
}

void fullPolyphony() {
    for (int n = 0; n < 128; n++) {
        handleNoteOn(1, n, 127);
    }
}

void randomDrawbars() {
    for (int i = 1; i <= 9; i++) {
        midiControl[CC_DRAWBAR_0 + i] = random(0, 127);
    }
    updateTonewheelVolumes();
}

void handleNoteOn(uint8_t key) {
    Serial.print("Key pressed: ");
    Serial.print(key);
    Serial.print("\n");

    // MIDI notes always have the high bit unset, but just in case.
    if (note & 0x80) {
        return;
    }

    midiKeys[note] = velocity;
    if (note <= MANUAL_KEY_0 || note > MANUAL_KEY_61) {
        return;
    }

    updateTonewheelVolumes();

    if (++numKeysDown == 1 && midiControl[CC_PERCUSSION]) {
        percussionEnv.noteOn();
    }
}

void handleNoteOff(uint8_t key) {
    Serial.print("Note off: ");
    Serial.print(key);
    Serial.print("\n");

    if (key & 0x80) {
        return;
    }

    midiKeys[note] = 0;
    if (note <= MANUAL_KEY_0 || note > MANUAL_KEY_61) {
        return;
    }

    if (--numKeysDown == 0 && midiControl[CC_PERCUSSION]) {
        percussionEnv.noteOff();
    }

    updateTonewheelVolumes();
}

void updateReset() {
    if (midiControl[CC_RESET]) {
        midiControl[CC_RESET] = 0;
        init();
    }
}

void updateVibrato() {
    uint8_t mode = midiControl[CC_VIBRATO_MODE];
    if (mode == 0) {
        vibrato.setMode(V1);
    } else if (mode <= 26) {
        vibrato.setMode(C1);
    } else if (mode <= 51) {
        vibrato.setMode(V2);
    } else if (mode <= 84) {
        vibrato.setMode(C2);
    } else if (mode <= 102) {
        vibrato.setMode(V3);
    } else if (mode <= 127) {
        vibrato.setMode(C3);
    }

    if (!midiControl[CC_VIBRATO]) {
        vibrato.setMode(Off);
    }
}

void updatePercussionEnvelope() {
    percussionEnv.delay(0.0);
    percussionEnv.attack(0.1);
    percussionEnv.sustain(0.0);
    percussionEnv.release(0.0);

    if (midiControl[CC_PERCUSSION_FAST]) {
        percussionEnv.decay(300.0);
    } else {
        percussionEnv.decay(630.0);
    }

    if (midiControl[CC_PERCUSSION_SOFT]) {
        organOut.gain(1, 0.25);
    } else {
        organOut.gain(1, 0.50);
    }
}

uint8_t bars[10] = {0};
uint16_t volumes[92] = {0};
uint8_t percBars[10] = {0};
uint16_t percVolumes[92] = {0};

void updateTonewheelVolumes() {
    for (int i = 1; i < 10; i++) {
        bars[i] = manual_quantize_drawbar(midiControl[CC_DRAWBAR_0 + i]);
    }

    if (midiControl[CC_PERCUSSION]) {
        bars[9] = 0;
        if (midiControl[CC_PERCUSSION_THIRD]) {
            percBars[5] = manual_quantize_drawbar(127);
        } else {
            percBars[4] = manual_quantize_drawbar(127);
        }
    }

    manual_fill_volumes(upperKeybed.getKeyState(), percBars, percVolumes);
    percussion.setVolumes(percVolumes);

    manual_fill_volumes(&midiKeys[MANUAL_KEY_0], bars, volumes);
    tonewheels.setVolumes(volumes);
}

float remap(float v, float oldmin, float oldmax, float newmin, float newmax) {
    return newmin + (v - oldmin) * (newmax - newmin) / (oldmax - oldmin);
}

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

    if (ctrl & 0x80) {
        return;
    }

    midiControl[ctrl] = val;

    if (ctrl == CC_SWELL) {
        swell.gain(remap((float)val, 0, 127, 0, 2.5));
    } else if (ctrl == CC_RESET) {
        updateReset();
    } else if (ctrl == CC_PERCUSSION) {
        updatePercussionEnvelope();
        updateTonewheelVolumes();
    } else if (ctrl == CC_PERCUSSION_FAST) {
        updatePercussionEnvelope();
    } else if (ctrl == CC_PERCUSSION_SOFT) {
        updatePercussionEnvelope();
    } else if (ctrl > CC_DRAWBAR_0 && ctrl <= CC_DRAWBAR_9) {
        updateTonewheelVolumes();
    } else if (ctrl == CC_ROTARY_STOP || ctrl == CC_ROTARY_SPEED) {
        updateLeslieRotation();
    } else if (ctrl == CC_VIBRATO || ctrl == CC_VIBRATO_MODE) {
        updateVibrato();
    }
}

void DEBUG_showKeys() {
    for (int i = 0; i < 62; i++) {
        Serial.print("keys[");
        Serial.print(i);
        Serial.print("] = ");
        Serial.print(midiKeys[MANUAL_KEY_0 + i]);
        Serial.print("\n");
    }
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
