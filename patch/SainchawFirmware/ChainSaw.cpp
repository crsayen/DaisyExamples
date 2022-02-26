#include "daisysp.h"
#include "sainchaw.h"
#include <string>

#define MAX_DETUNE 100
#define SWARM_SIZE 1
#define CENT 0.02f
#define SEMITONE 0.166666667f
#define BASE_FREQ 32.7f

using namespace daisy;
using namespace daisysp;

Sainchaw   sainchaw;
Oscillator swarm_0[SWARM_SIZE];

int  detune_amt;
bool alt_encoder_behavior;
int  semitones;
int  cents;

void UpdateControls();

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size) {
    UpdateControls();
    for(size_t i = 0; i < size; i++) {
        //Process and output the three oscillators
        for(size_t chn = 0; chn < 2; chn++) {
            float sig = 0.f;
            for(int j = 0; j < SWARM_SIZE; j++) {
                sig += swarm_0[j].Process();
            }
            out[chn][i] = sig;
        }
    }
}

void SetupOsc(float samplerate) {
    for(int i = 0; i < SWARM_SIZE; i++) {
        swarm_0[i].Init(samplerate);
        //swarm_0[i].SetAmp(0.14285);
        swarm_0[i].SetAmp(1);
    }
}

int main(void) {
    float samplerate;
    sainchaw.Init(); // Initialize hardware (daisy seed, and patch)
    samplerate = sainchaw.AudioSampleRate();

    detune_amt           = 0;
    alt_encoder_behavior = false;
    semitones            = 0;
    cents                = 0;

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

    // cv
    float ctrl[Sainchaw::CTRL_LAST];
    for(int i = 0; i < Sainchaw::CTRL_LAST; i++) {
        ctrl[i] = sainchaw.GetKnobValue((Sainchaw::Ctrl)i);
    }

    float pitch = ctrl[Sainchaw::PITCH_1_CTRL] * 5.f;           //voltage
    pitch       = powf(2.f, ctrl[Sainchaw::PITCH_1_CTRL]) * 55; //Hz


    //encoder
    handleEncoder();

    //Adjust oscillators based on inputs
    for(int i = 0; i < SWARM_SIZE; i++) {
        swarm_0[i].SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);

        if(semitones != 0) {
            pitch = powf(2.0, (semitones / 12)) * pitch;
        }

        if(cents != 0) {
            pitch = powf(2.0, (cents / 100)) * pitch;
        }

        if(pitch < 8.175f) {
            pitch = 8.175f;
        }

        swarm_0[i].SetFreq(pitch);
    }
}