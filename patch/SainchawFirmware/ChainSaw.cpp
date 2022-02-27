#include "daisysp.h"
#include "sainchaw.h"
#include <string>

#define SWARM_SIZE 7
#define SWRM_CTR_SHFT 3
#define CENT 0.02f
#define SEMITONE 0.166666667f
#define BASE_FREQ 32.7f
#define MAX_DETUNE 0.15f
#define FM_HZ_PER_VOLT 200.0f

using namespace daisy;
using namespace daisysp;

Sainchaw   sainchaw;
Oscillator swarm_0[SWARM_SIZE][2];
Oscillator test_osc;

float detune;
bool  alt_encoder_behavior;
int   semitones;
int   cents;
float osc_amplitude;
float crossfade;

void UpdateControls();

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size) {
    UpdateControls();
    for(size_t i = 0; i < size; i++) {
        for(size_t chn = 0; chn < 2; chn++) {
            float sig = 0.f;
            for(int j = 0; j < SWARM_SIZE; j++) {
                sig += (1.0f - crossfade) * swarm_0[j][0].Process();
                sig += crossfade * swarm_0[j][1].Process();
            }
            out[chn][i] = sig;
        }
    }
}

void SetupOsc(float samplerate) {
    for(int i = 0; i < SWARM_SIZE; i++) {
        for(int j = 0; j < 2; j++) {
            u_int8_t waveforms[2] = {Oscillator::WAVE_POLYBLEP_SAW,
                                     Oscillator::WAVE_POLYBLEP_SQUARE};
            swarm_0[i][j].Init(samplerate);
            swarm_0[i][j].SetWaveform(waveforms[j]);
            swarm_0[i][j].SetAmp(osc_amplitude);
        }
    }
}

int main(void) {
    float samplerate;
    sainchaw.Init(); // Initialize hardware (daisy seed, and patch)
    samplerate = sainchaw.AudioSampleRate();

    detune               = 0.f;
    alt_encoder_behavior = false;
    semitones            = 0;
    cents                = 0;
    osc_amplitude        = .7f / SWARM_SIZE;
    crossfade            = 0.0f;

    SetupOsc(samplerate);
    sainchaw.SetAltLed(false);
    sainchaw.SetNoteLed(true);

    sainchaw.StartAdc();
    sainchaw.StartAudio(AudioCallback);
}

void handleEncoder() {
    if(sainchaw.encoder.FallingEdge()) {
        alt_encoder_behavior = !alt_encoder_behavior;
        sainchaw.SetAltLed(alt_encoder_behavior);
        sainchaw.SetNoteLed(!alt_encoder_behavior);
    }
    if(alt_encoder_behavior) {
        cents += sainchaw.encoder.Increment();
    } else {
        semitones += sainchaw.encoder.Increment();
    }
}


void UpdateControls() {
    sainchaw.ProcessDigitalControls();
    sainchaw.ProcessAnalogControls();

    // Detune and Shape cv
    float ctrl[Sainchaw::CTRL_LAST];
    for(int i = Sainchaw::DETUNE_CTRL; i < Sainchaw::SHAPE_CTRL + 1; i++) {
        ctrl[i]
            = sainchaw.GetKnobValue((Sainchaw::Ctrl)i) + 1.f * .5f; //normalized
    }

    // FM cv
    float fm_val            = sainchaw.GetKnobValue(Sainchaw::FM_CTRL);
    ctrl[Sainchaw::FM_CTRL] = fm_val * 8.f; //voltage

    // Pitch cv
    // for(int i = Sainchaw::PITCH_1_CTRL; i < Sainchaw::CTRL_LAST; i++) {
    // float pitch_val = sainchaw.GetKnobValue((Sainchaw::Ctrl)i);
    float pitch_val = sainchaw.GetKnobValue(Sainchaw::PITCH_1_CTRL);
    ctrl[Sainchaw::PITCH_1_CTRL] = pitch_val * 5.f + 2; //voltage
    // }

    crossfade = DSY_CLAMP(ctrl[Sainchaw::SHAPE_CTRL], 0.f, 1.f);

    detune = DSY_CLAMP(ctrl[Sainchaw::DETUNE_CTRL], 0.f, 1.f);

    float fm = powf(2.f, ctrl[Sainchaw::FM_CTRL] * 6.875) * FM_HZ_PER_VOLT;

    float pitch = powf(2.f, ctrl[Sainchaw::PITCH_1_CTRL]) * 32.7f; //Hz


    //encoder
    handleEncoder();

    //Adjust oscillators based on inputs
    for(int i = 0; i < SWARM_SIZE; i++) {
        // TODO: don't need to do all this stuff 7 times
        float shift = (i - SWRM_CTR_SHFT) * detune * MAX_DETUNE;
        shift += semitones + (cents * 0.12f);
        float shifted_pitch = powf(2.0f, shift * kOneTwelfth) * pitch;
        shifted_pitch       = DSY_CLAMP(shifted_pitch, 8.175f, 16742.4f);

        swarm_0[i][0].SetFreq(shifted_pitch); // + fm);
        swarm_0[i][1].SetFreq(shifted_pitch); // + fm);
    }
}