#include "daisysp.h"
#include "sainchaw.h"
#include <string>

using namespace daisy;
using namespace daisysp;

const int   kSwarmSize                       = 3;
const int   kSwarmCenterShift                = 1;
const float kBaseFrequency                   = 32.7f;
const float kMaxDetune                       = 0.25f;
const float kFmHzPerVolt                     = 100.0f;
const float osc_amplitude                    = 0.2f / kSwarmSize;
const float kNormalizationDetectionThreshold = -0.15f;
const int   kNumNormalizedChannels           = 3;
const int   kProbeSequenceDuration           = 32;
const int   kNumSwarms                       = 3;
const int   kNumStereoChannels               = 2;

Sainchaw   sainchaw;
Oscillator swarms[kNumSwarms][kSwarmSize][kNumStereoChannels];
Oscillator test_osc;

float    detune_;
bool     alt_encoder_behavior_;
int      semitones_;
int      cents_;
float    crossfade_;
float    lpfm_;
bool     ignore_enc_switch_;
int      normalization_detection_count_;
int      normalization_detection_mismatches_[kNumNormalizedChannels];
uint32_t normalization_probe_state_;
bool     voct_patched_[kNumNormalizedChannels];
int      waitcount;

void UpdateControls();
void DetectNormalization();

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size) {
    UpdateControls();
    for(size_t i = 0; i < size; i++) {
        for(size_t chn = 0; chn < kNumStereoChannels; chn++) {
            float sig             = 0.f;
            int   numActiveSwarms = 0;
            for(size_t swarmn = 0; swarmn < kNumSwarms; swarmn++) {
                if(voct_patched_[swarmn]) {
                    ++numActiveSwarms;

                    for(int j = 0; j < kSwarmSize; j++) {
                        sig += (1.0f - crossfade_)
                               * swarms[swarmn][j][0].Process();
                        sig += crossfade_ * swarms[swarmn][j][1].Process();
                    }
                }
            }
            out[chn][i] = sig / (float)numActiveSwarms;
        }
    }
}

void SetupOsc(float samplerate) {
    for(int i = 0; i < kSwarmSize; i++) {
        for(int j = 0; j < 2; j++) {
            u_int8_t waveforms[2] = {Oscillator::WAVE_POLYBLEP_SQUARE,
                                     Oscillator::WAVE_POLYBLEP_SAW};
            swarm_0[i][j].Init(samplerate);
            swarm_0[i][j].SetWaveform(waveforms[j]);
            swarm_0[i][j].SetAmp(osc_amplitude);
        }
    }
}

int main(void) {
    float samplerate;
    sainchaw.Init(); // Initialize hardware
    samplerate = sainchaw.AudioSampleRate();

    detune_                        = 0.f;
    avg                            = 0.f;
    avghicnt                       = 0;
    alt_encoder_behavior_          = false;
    semitones_                     = 0;
    cents_                         = 0;
    crossfade_                     = 0.0f;
    ignore_enc_switch_             = false;
    normalization_detection_count_ = 0;
    normalization_probe_state_     = 123456789;
    waitcount                      = 0;

    for(int i = 0; i < kNumNormalizedChannels; i++) {
        voct_patched_[i]                       = false;
        normalization_detection_mismatches_[i] = 0;
    }

    SetupOsc(samplerate);
    sainchaw.SetAltLed(false);
    sainchaw.SetNoteLed(true);

    sainchaw.StartAdc();
    sainchaw.StartAudio(AudioCallback);
}

void handleEncoder() {
    sainchaw.SetAltLed(voct_patched_[0]);
    sainchaw.SetNoteLed(voct_patched_[1]);
    if(sainchaw.encoder.TimeHeldMs() >= 1000) {
        cents_             = 0;
        semitones_         = 0;
        ignore_enc_switch_ = true;
        return;
    }
    if(sainchaw.encoder.FallingEdge()) {
        if(ignore_enc_switch_) {
            ignore_enc_switch_ = false;
            return;
        }
        alt_encoder_behavior_ = !alt_encoder_behavior_;
        sainchaw.SetAltLed(alt_encoder_behavior_);
        sainchaw.SetNoteLed(!alt_encoder_behavior_);
    }
    if(alt_encoder_behavior_) {
        cents_ += sainchaw.encoder.Increment();
    } else {
        semitones_ += sainchaw.encoder.Increment();
    }
}

void DetectNormalization() {
    // state of the highest-order bit
    if(waitcount++ < 5) {
        return;
    }
    waitcount = 0;
    bool hilo = normalization_probe_state_ >> 31;

    // float expected_adc_value
    // = hilo ? expected_norm_probe_high : expected_norm_probe_low;
    for(int i = 0; i < kNumNormalizedChannels; ++i) {
        float actual_adc_value = sainchaw.GetKnobValue(
            (Sainchaw::Ctrl)(i + Sainchaw::PITCH_1_CTRL)); // -.2 -.06

        bool read_state = actual_adc_value > kNormalizationDetectionThreshold;
        if(read_state != hilo) {
            ++normalization_detection_mismatches_[i];
        }
    }

    ++normalization_detection_count_;
    if(normalization_detection_count_ >= kProbeSequenceDuration) {
        normalization_detection_count_ = 0;
        for(int i = 0; i < kNumNormalizedChannels; ++i) {
            voct_patched_[i] = normalization_detection_mismatches_[i] >= 2;
            normalization_detection_mismatches_[i] = 0;
        }
    }

    normalization_probe_state_
        = 1103515245 * normalization_probe_state_ + 12345;
    sainchaw.SetNormalizationProbe(normalization_probe_state_ >> 31);
}


void UpdateControls() {
    sainchaw.ProcessDigitalControls();
    sainchaw.ProcessAnalogControls();
    DetectNormalization();
    // Detune and Shape cv
    float ctrl[Sainchaw::CTRL_LAST];
    for(int i = Sainchaw::DETUNE_CTRL; i < Sainchaw::SHAPE_CTRL + 1; i++) {
        ctrl[i]
            = sainchaw.GetKnobValue((Sainchaw::Ctrl)i) + 1.f * .5f; //normalized
    }

    float fm_val = sainchaw.GetKnobValue(Sainchaw::FM_CTRL) * 8.f; //voltage
    fonepole(lpfm_, fm_val, .001f);
    ctrl[Sainchaw::FM_CTRL] = lpfm_;

    for(int i = 0; i < 3; i++) {
        ctrl[Sainchaw::PITCH_1_CTRL + i]
            = sainchaw.GetKnobValue(Sainchaw::PITCH_1_CTRL + i) * 5.f
              + 2; //voltage
    }


    crossfade_ = DSY_CLAMP(ctrl[Sainchaw::SHAPE_CTRL], 0.f, 1.f);

    detune_ = DSY_CLAMP(ctrl[Sainchaw::DETUNE_CTRL], 0.f, 1.f);

    float fm = ctrl[Sainchaw::FM_CTRL] * kFmHzPerVolt;

    float pitch = powf(2.f, ctrl[Sainchaw::PITCH_1_CTRL]) * 32.7f; //Hz


    //encoder
    handleEncoder();

    //Adjust oscillators based on inputs
    for(int i = 0; i < kSwarmSize; i++) {
        for(int swarmn = 0; swarmn < kNumSwarms; s++) {
            if(voct_patched_[swarmn]) {
                float shift = (i - kSwarmCenterShift) * detune_ * kMaxDetune;
                shift += semitones_ + (cents_ * 0.12f);
                float shifted_pitch = powf(2.0f, shift * kOneTwelfth) * pitch;
                shifted_pitch = DSY_CLAMP(shifted_pitch, 8.175f, 16742.4f);
                swarms[swarmn][i][0].SetFreq(shifted_pitch + fm);
                swarms[swarmn][i][1].SetFreq(shifted_pitch + fm);
            }
        }
    }
}