#include <string>

#include "daisysp.h"
#include "sainchaw.h"

using namespace daisy;
using namespace daisysp;

const int   kSwarmSize                       = 7;
const int   kSwarmCenterShift                = 3;
const float kBaseFrequency                   = 32.7f;
const float kDetuneScale                     = 0.175f;
const float kFmHzPerVolt                     = 100.0f;
const float osc_amplitude                    = 0.2f / kSwarmSize;
const float kNormalizationDetectionThreshold = -0.15f;
const int   kNumNormalizedChannels           = 3;
const int   kProbeSequenceDuration           = 32;
const int   kNumSwarms                       = 3;
const int   kNumStereoChannels               = 2;

Sainchaw                sainchaw;
VariableShapeOscillator swarms[kNumSwarms][kSwarmSize];

// #define LOAD_METER

#ifdef LOAD_METER
CpuLoadMeter cpuLoadMeter;
#endif


bool     alt_encoder_behavior_;
int      semitones_;
int      cents_;
float    lpfm_;
bool     ignore_enc_switch_;
int      normalization_detection_count_;
int      normalization_detection_mismatches_[kNumNormalizedChannels];
uint32_t normalization_probe_state_;
bool     voct_patched_[kNumNormalizedChannels];
int      waitcount_;
uid_t    cpuLoad_2_bits;

void UpdateControls();
void DetectNormalization();

void blockStart() {
#ifdef LOAD_METER
  cpuLoadMeter.OnBlockStart();
#endif
}

void blockEnd() {
#ifdef LOAD_METER
  cpuLoadMeter.OnBlockEnd();
  float load = cpuLoadMeter.GetAvgCpuLoad();
  if(load > .75f) {
    cpuLoad_2_bits = 0b11;
  } else if(load > .5f) {
    cpuLoad_2_bits = 0b10;
  } else if(load > .25f) {
    cpuLoad_2_bits = 0b01;
  } else {
    cpuLoad_2_bits = 0b00;
  }
#endif
}

void displayLoadMeter() {
#ifdef LOAD_METER
  sainchaw.SetAltLed(cpuLoad_2_bits & 0x01);
  sainchaw.SetNoteLed(cpuLoad_2_bits & 0x02);
#endif LOAD_METER
}

void displayEncoderState() {
#ifndef LOAD_METER
  sainchaw.SetAltLed(alt_encoder_behavior_);
  sainchaw.SetNoteLed(!alt_encoder_behavior_);
#endif
}

static void
AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  UpdateControls();
  int numActiveSwarms = 0;
  for(size_t i = 0; i < size; i++) {
    float sig = 0.f;

    for(size_t swarmn = 0; swarmn < kNumSwarms; swarmn++) {
      if(voct_patched_[swarmn]) {
        ++numActiveSwarms;

        for(int j = 0; j < kSwarmSize; j++) { sig += swarms[swarmn][j].Process(); }
      }
    }
    sig *= 0.005;

    out[0][i] = sig;
    out[1][i] = sig;
  }
}

void SetupOsc(float samplerate) {
  for(int i = 0; i < kSwarmSize; i++) {
    for(int s = 0; s < kNumSwarms; s++) {
      swarms[s][i].Init(samplerate);
      swarms[s][i].SetSync(true);
    }
  }
}

int main(void) {
  float samplerate;
  sainchaw.Init(); // Initialize hardware
  samplerate                     = sainchaw.AudioSampleRate();
  alt_encoder_behavior_          = false;
  semitones_                     = 0;
  cents_                         = 0;
  ignore_enc_switch_             = false;
  normalization_detection_count_ = 0;
  normalization_probe_state_     = 123456789;
  waitcount_                     = 0;

#ifdef LOAD_METER
  cpuLoadMeter.Init(samplerate, sainchaw.AudioBlockSize());
#endif

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

    displayEncoderState();
  }
  if(alt_encoder_behavior_) {
    cents_ += sainchaw.encoder.Increment();
  } else {
    semitones_ += sainchaw.encoder.Increment();
  }
}

void DetectNormalization() {
  // apparently either my ADCs or my GPIO are too slow to
  // reliably perform normalization detection at 1khz+.
  // so we wait for 5 update cycles (approx 5ms) to check
  // for the normalization noise
  if(waitcount_++ < 5) { return; }
  waitcount_ = 0;

  bool expected_state = normalization_probe_state_ >> 31;

  for(int i = 0; i < kNumNormalizedChannels; i++) {
    float actual_adc_value = sainchaw.GetKnobValue(
        (Sainchaw::Ctrl)(i + Sainchaw::PITCH_1_CTRL)); // approx ( -.2 | -.06 )

    bool read_state = actual_adc_value > kNormalizationDetectionThreshold;
    if(read_state != expected_state) { normalization_detection_mismatches_[i]++; }
  }

  ++normalization_detection_count_;
  if(normalization_detection_count_ >= kProbeSequenceDuration) {
    normalization_detection_count_ = 0;
    for(int i = 0; i < kNumNormalizedChannels; ++i) {
      voct_patched_[i] = normalization_detection_mismatches_[i] >= 4;
      normalization_detection_mismatches_[i] = 0;
    }
  }

  normalization_probe_state_ = 1103515245 * normalization_probe_state_ + 12345;
  sainchaw.SetNormalizationProbe(normalization_probe_state_ >> 31);
}

void UpdateControls() {
  sainchaw.ProcessDigitalControls();
  sainchaw.ProcessAnalogControls();
  DetectNormalization();
  displayLoadMeter();

  float shape_amt = DSY_CLAMP(
      ((sainchaw.GetKnobValue(Sainchaw::SHAPE_CTRL) + 1) * .5), .0f, 1.f); // 0.0 to 1.0

  float detune_amt
      = DSY_CLAMP(((sainchaw.GetKnobValue(Sainchaw::DETUNE_CTRL) + 1.f) * .5), .0f, 1.f)
        * kDetuneScale;

  // float fm_val = sainchaw.GetKnobValue(Sainchaw::FM_CTRL) * 8.f; //voltage
  // fonepole(lpfm_, fm_val, .001f);
  // ctrl[Sainchaw::FM_CTRL] = lpfm_;

  // float fm = ctrl[Sainchaw::FM_CTRL] * kFmHzPerVolt;

  // encoder
  handleEncoder();

  // Adjust oscillators based on inputs
  for(int s = 0; s < kNumSwarms; s++) {
    if(!voct_patched_[s]) { continue; }

    float voltage
        = sainchaw.GetKnobValue((Sainchaw::Ctrl)(Sainchaw::PITCH_1_CTRL + s)) * 5.f + 2;

    float pitch = powf(2.f, voltage) * 32.7f; // Hz

    for(int i = 0; i < kSwarmSize; i++) {
      float shift = (i - kSwarmCenterShift) * detune_amt + semitones_ + cents_ * 0.12f;
      float shifted_pitch = powf(2.0f, shift * kOneTwelfth) * pitch;
      // swarms[s][i].SetFreq(shifted_pitch); // + fm);
      swarms[s][i].SetSyncFreq(shifted_pitch);
      swarms[s][i].SetFreq(shifted_pitch);
      swarms[s][i].SetPW((shape_amt)*0.5f);
      swarms[s][i].SetWaveshape(shape_amt);
    }
  }
}