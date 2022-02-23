#include "daisysp.h"
#include "daisy_patch.h"
#include <string>

#define MAX_DETUNE 100

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;
Oscillator osc[12];
std::string waveNames[5];
std::string parameterNames[7];

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
        for(size_t chn = 0; chn < 4; chn++)
        {
            float sig = 0.f;
            for(int j = 0; j < 3; j++) {
                int osc_idx = chn * 3 + j;
                sig += osc[osc_idx].Process();
            }
            out[chn][i] = sig;
        }
    }
}

void SetupOsc(float samplerate)
{
    for(int i = 0; i < 12; i++)
    {
        osc[i].Init(samplerate);
        osc[i].SetAmp(.3);
    }
}

void SetupWaveNames()
{
    waveNames[0] = "sine";
    waveNames[1] = "triangle";
    waveNames[2] = "saw";
    waveNames[3] = "ramp";
    waveNames[4] = "square";
}

void SetupParameterNames() {
    parameterNames[0] = "osc 1 waveform";
    parameterNames[1] = "osc 2 waveform";
    parameterNames[2] = "osc 3 waveform";
    parameterNames[3] = "osc 1 octave";
    parameterNames[4] = "osc 2 octave";
    parameterNames[5] = "osc 3 octave";
    parameterNames[6] = "detune";
}

void UpdateOled();

int main(void)
{
    float samplerate;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    samplerate = patch.AudioSampleRate();

    detune_amt = 0;

    parameter = 0;
    final_parameter = 6;

    osc_octaves[0] = 0;
    osc_octaves[1] = 0;
    osc_octaves[2] = 0;

    waveforms[0] = 0;
    waveforms[1] = 0;
    waveforms[2] = 0;
    final_wave = Oscillator::WAVE_POLYBLEP_TRI;

    SetupOsc(samplerate);
    SetupWaveNames();
    SetupParameterNames();

    testval = 0.f;

    patch.StartAdc();
    patch.StartAudio(AudioCallback);
    while(1)
    {
        UpdateOled();
    }
}

void setDisplay(std::string parameterName, std::string value)
{
    patch.display.Fill(false);

    patch.display.SetCursor(0, 0);
    std::string str  = "ShartPisser 9000";
    char*       cstr = &str[0];
    patch.display.WriteString(cstr, Font_7x10, true);

    str = parameterName + ":";
    cstr = &str[0];
    patch.display.SetCursor(0, 20);
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.SetCursor(0, 40);
    cstr = &value[0];
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.Update();
}

void UpdateOled()
{
    std::string parameterName = parameterNames[parameter];
    if (parameter == 0 || parameter == 1 || parameter == 2) {
        setDisplay(parameterName, waveNames[waveforms[parameter]]);
    } else if (parameter == 3 || parameter == 4 || parameter == 5) {
        setDisplay(parameterName, std::to_string(osc_octaves[parameter - 3]));
    } else if (parameter == 6) {
        setDisplay(parameterName, std::to_string(detune_amt));
    }
}

void handleEncoder() {
    if (patch.encoder.FallingEdge()) {
        parameter += 1;
        parameter %= (final_parameter + 1);
    }
    if (parameter == 0 || parameter == 1 || parameter == 2) {
        waveforms[parameter] += patch.encoder.Increment();
        waveforms[parameter] = (waveforms[parameter] % final_wave + final_wave) % final_wave;
    } else if (parameter == 3 || parameter == 4 || parameter == 5) {
        osc_octaves[parameter - 3] += patch.encoder.Increment();
        if (osc_octaves[parameter - 3] > 2) {
            osc_octaves[parameter - 3] = 2;
        } else if (osc_octaves[parameter - 3] < -2) {
            osc_octaves[parameter - 3] = -2;
        }
    } else if (parameter == 6) {
        detune_amt += patch.encoder.Increment();
        detune_amt = detune_amt < 0 ? 0 : detune_amt;
        detune_amt = detune_amt > MAX_DETUNE ? MAX_DETUNE : detune_amt;
    }
}

void UpdateControls()
{
    patch.ProcessDigitalControls();
    patch.ProcessAnalogControls();

    //knobs
    float ctrl[4];
    for(int i = 0; i < 4; i++)
    {
        ctrl[i] = patch.GetKnobValue((DaisyPatch::Ctrl)i);
    }

    for(int i = 0; i < 4; i++)
    {
        ctrl[i] = ctrl[i] * 5.f;           //voltage
        ctrl[i] = powf(2.f, ctrl[i]) * 55; //Hz
    }

    testval = patch.GetKnobValue((DaisyPatch::Ctrl)2) * 5.f;

    //encoder
    handleEncoder();

    //Adjust oscillators based on inputs
    for(int i = 0; i < 4; i++)
    {
        int base_osc_idx = i * 3;
        float base_freq = ctrl[i];
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
        

        osc[base_osc_idx + 0].SetFreq((base_freq * octs[0]) - (base_freq * detune_fac));
        osc[base_osc_idx + 0].SetWaveform((uint8_t)waveforms[0]);

        osc[base_osc_idx + 1].SetFreq(base_freq * octs[1]);
        osc[base_osc_idx + 1].SetWaveform((uint8_t)waveforms[1]);

        osc[base_osc_idx + 2].SetFreq((base_freq * octs[2]) + (base_freq * detune_fac));
        osc[base_osc_idx + 2].SetWaveform((uint8_t)waveforms[2]);
    }
}