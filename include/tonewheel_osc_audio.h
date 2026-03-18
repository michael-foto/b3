/* Copyright (c) 2018 Peter Teichman */

#ifndef TONEWHEEL_OSC_AUDIO_H
#define TONEWHEEL_OSC_AUDIO_H

#include <Audio.h>
#include "tonewheel_osc.h"

/**
 * TonewheelOsc is a Teensy AudioStream wrapper around the tonewheel_osc
 * oscillator block.
 * It splits the tonewheel output based on which tonewheels should pass through
 * the vibrato line (upper/lower) and which should be clean
 * Channel 0 - raw signal
 * Channel 1 - vibrato signal
 */
class TonewheelOsc : public AudioStream {
  public:
    TonewheelOsc() : AudioStream(0, NULL) {
    }

    void init() {
        osc = tonewheel_osc_new();
    }

    void update() {
        // the block containing all tonewheel audio for keybeds not passing
        // through the vibrato filter
        audio_block_t *raw_block;
        // block containing tonewheel audio for keybeds with vibrato active
        audio_block_t *vibrato_block;
        raw_block = allocate();
        vibrato_block = allocate();

        if (!raw_block || !vibrato_block) {
            return;
        }

        // fill blocks from the oscilator channels. One for all tonewheels that
        // should run through the vibrato filter (channel 1), and one to skip 
        // the vibrato (channel 0)
        tonewheel_osc_fill(osc, vibrato_block->data, raw_block->data, AUDIO_BLOCK_SAMPLES);

        transmit(raw_block, 0);
        transmit(vibrato_block, 1);
        release(raw_block);
        release(vibrato_block);
    }

    void clear() {
        memset(osc->raw_volumes, 0, sizeof osc->raw_volumes);
        memset(osc->vibrato_volumes, 0, sizeof osc->vibrato_volumes);
    }

    void setVolumes(uint32_t volumes[92]) {
        for (int i = 1; i < 92; i++) {
            tonewheel_osc_set_volume(osc->raw_volumes, i, volumes[i]);
        }
    }

    void setVibratoVolumes(uint32_t volumes[92]) {
        for (int i = 1; i < 92; i++) {
            tonewheel_osc_set_volume(osc->vibrato_volumes, i, volumes[i]);
        }
    }

  private:
    tonewheel_osc *osc;
};

#endif
