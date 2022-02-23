#include "daisysp.h"
#include "sainchaw.h"
#include <string>

#define MAX_DETUNE 100
#define SWARM_SIZE 3

using namespace daisy;
using namespace daisysp;

Sainchaw sainchaw;
Oscillator swarm_0[SWARM_SIZE];
// Oscillator swarm_1[SWARM_SIZE];
// Oscillator swarm_2[SWARM_SIZE];

int detune_amt;

int parameter;
int final_parameter;

int waveforms[3];
int osc_octaves[3];
int final_wave;

float testval;

void UpdateControls();

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    UpdateControls();
    for(size_t i = 0; i < size; i++)
    {
        //Process and output the three oscillators
        for(size_t chn = 0; chn < 2; chn++)
        {
            float sig = 0.f;
            for(int j = 0; j < SWARM_SIZE; j++) {
                sig += swarm_0[j].Process();
            }
            out[chn][i] = sig / SWARM_SIZE;
        }
    }
}

void SetupOsc(float samplerate)
{
    for(int i = 0; i < SWARM_SIZE; i++)
    {
        swarm_0[i].Init(samplerate);
        swarm_0[i].SetAmp(.3);
    }
}

int main(void)
{
    float samplerate;
    sainchaw.Init(); // Initialize hardware (daisy seed, and patch)
    samplerate = sainchaw.AudioSampleRate();

    detune_amt = 0;

    parameter = 0;
    final_parameter = 6;

    SetupOsc(samplerate);

    testval = 0.f;

    sainchaw.StartAdc();
    sainchaw.StartAudio(AudioCallback);
}

void handleEncoder() {
    if (sainchaw.encoder.FallingEdge()) {
        parameter += 1;
        parameter %= (final_parameter + 1);
    }
}

void UpdateControls()
{
    sainchaw.ProcessDigitalControls();
    sainchaw.ProcessAnalogControls();

    // cv
    float ctrl[Sainchaw::CTRL_LAST];
    for(int i = 0; i < Sainchaw::CTRL_LAST; i++)
    {
        ctrl[i] = sainchaw.GetKnobValue((Sainchaw::Ctrl)i);
    }

    for(int i = 0; i < 4; i++)
    {
        ctrl[i] = ctrl[i] * 5.f;           //voltage
        ctrl[i] = powf(2.f, ctrl[i]) * 55; //Hz
    }

    testval = sainchaw.GetKnobValue((Sainchaw::Ctrl)2) * 5.f;

    //encoder
    handleEncoder();

    //Adjust oscillators based on inputs
    for(int i = 0; i < SWARM_SIZE; i++)
    {
        // float base_freq = ctrl[i];
        float base_freq = 1000;
        float detune_fac = detune_amt * 0.01f * .05f;
        float octs[3];
        for (int o = 0; o < 3; o++) {
            if (osc_octaves[o] == 0) {
                octs[o] = 1;
            }
            else if (osc_octaves[o] > 0) {
                octs[o] = osc_octaves[o] + 1;
            } else {
                octs[o] = -1.f / (2.f * osc_octaves[o]);
            }
        }
        
        swarm_0[i].SetFreq((base_freq * octs[0]) - (base_freq * detune_fac));
        // osc[base_osc_idx + 0].SetFreq((base_freq * octs[0]) - (base_freq * detune_fac));
    }
}