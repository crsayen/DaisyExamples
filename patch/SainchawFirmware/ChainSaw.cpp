#include <string>

#include "Pitch.h"
#include "configuration.h"
#include "daisy_sainchaw.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

const int   kMaxSwarmSize          = 9;
const float kBaseFrequency         = 32.7f;
const float kDetuneScale           = 0.25f;
const float kFmHzPerVolt           = 100.0f;
const int   kNumSwarms             = 3;
const int   kProbeSequenceDuration = 32;
const int   kNumStereoChannels     = 2;
const int   kConfigSlot            = 0;

enum EncState { SEMITONES, CENTS, SWARM_SIZE, LAST };

Sainchaw                sainchaw;
VariableShapeOscillator swarms[kNumSwarms][kMaxSwarmSize];


CONFIGURATION current_config;
bool          docal;
int           encoderState_;
int           swarmSize_;
float         amplitudeReduction_;
int           swarmCenterShift_;
int           semitones_;
int           cents_;
float         lpfm_;
bool          ignore_enc_switch_;
bool          voct_patched_[kNumSwarms];
int           waitcount_;
Pitch         pitch[kNumSwarms];

void UpdateControls();
void handleCalibration();
bool cal_handleEncoder();
void cal_read_voct();
void save_config(uint32_t slot);
void load_config(uint32_t slot);

static void
AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  UpdateControls();
  int numActiveSwarms = 0;
  for(size_t i = 0; i < size; i++) {
    float sig = 0.f;

    for(size_t swarmn = 0; swarmn < kNumSwarms; swarmn++) {
      if(voct_patched_[swarmn]) {
        ++numActiveSwarms;

        for(int j = 0; j < swarmSize_; j++) { sig += swarms[swarmn][j].Process(); }
      }
    }
    sig *= .3f * amplitudeReduction_;

    out[0][i] = sig;
    out[1][i] = sig;
  }
}

void SetupOsc(float samplerate) {
  for(int i = 0; i < kMaxSwarmSize; i++) {
    for(int s = 0; s < kNumSwarms; s++) {
      swarms[s][i].Init(samplerate);
      swarms[s][i].SetSync(true);
    }
  }
}

int main(void) {
  float samplerate;
  sainchaw.Init(); // Initialize hardware
  samplerate = sainchaw.AudioSampleRate();
  load_config(kConfigSlot);

  encoderState_       = EncState::SEMITONES;
  docal               = false;
  swarmSize_          = 5;
  swarmCenterShift_   = 2;
  amplitudeReduction_ = .12f / swarmSize_;
  semitones_          = 0;
  cents_              = 0;
  ignore_enc_switch_  = false;
  waitcount_          = 0;

  for(int i = 0; i < kNumSwarms; i++) {
    voct_patched_[i] = false;
    // TODO: make a UI to turn inputs on and off
    if(i == 0) { voct_patched_[i] = true; }
    pitch[i].SetByNote(0);
  }

  SetupOsc(samplerate);
  sainchaw.SetAltLed(false);
  sainchaw.SetNoteLed(true);

  sainchaw.StartAdc();
  sainchaw.StartAudio(AudioCallback);
}

void handleEncoder() {
  if(sainchaw.encoder.TimeHeldMs() >= 5000) {
    sainchaw.SetNoteLed(true);
    sainchaw.SetAltLed(true);
    if(sainchaw.encoder.Increment()) {
      docal = true;
      return;
    }
    return;
  }
  if(sainchaw.encoder.TimeHeldMs() >= 1000) {
    cents_             = 0;
    semitones_         = 0;
    ignore_enc_switch_ = true;
    return;
  }
  if(sainchaw.encoder.FallingEdge()) {
    if(docal) {
      docal = false;
      handleCalibration();
    }
    if(ignore_enc_switch_) {
      ignore_enc_switch_ = false;
      return;
    }

    encoderState_++;
    if(encoderState_ == EncState::LAST) { encoderState_ = 0; }
  }

  switch(encoderState_) {
    case EncState::SEMITONES: semitones_ += sainchaw.encoder.Increment(); break;
    case EncState::CENTS: cents_ += sainchaw.encoder.Increment(); break;
    case EncState::SWARM_SIZE:
      swarmSize_
          = DSY_CLAMP(swarmSize_ + sainchaw.encoder.Increment() * 2, 1, kMaxSwarmSize);
      swarmCenterShift_ = floor(swarmCenterShift_ * .5f);
      break;
  }

  sainchaw.SetNoteLed(encoderState_ == EncState::SEMITONES
                      || encoderState_ == EncState::SWARM_SIZE);
  sainchaw.SetAltLed(encoderState_ == EncState::CENTS
                     || encoderState_ == EncState::SWARM_SIZE);
}

void UpdateControls() {
  sainchaw.ProcessDigitalControls();
  sainchaw.ProcessAnalogControls();


  float shape_amt = DSY_CLAMP(
      ((sainchaw.GetKnobValue(Sainchaw::SHAPE_CTRL) + 1) * .5), .0f, 1.f); // 0.0 to 1.0

  float detune_amt
      = DSY_CLAMP(
            ((sainchaw.GetKnobValue(Sainchaw::DETUNE_CTRL) + 1.f) * .5) - .05f, .0f, 1.f)
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

    pitch[s].SetByVoltage(voltage);

    for(int i = 0; i < swarmSize_; i++) {
      pitch[i].Transpose(((i - swarmCenterShift_) * detune_amt) + semitones_, cents_);
      // swarms[s][i].SetFreq(shifted_pitch); // + fm);
      swarms[s][i].SetSyncFreq(pitch[i].hz);
      swarms[s][i].SetFreq(pitch[i].hz);
      swarms[s][i].SetPW(DSY_MIN(shape_amt, .5f));
      swarms[s][i].SetWaveshape(shape_amt);
    }
  }
}

/*
 *
 *
 *
 **     2222222222      222222        2222
 **   22222222222     2222  2222      2222
 **  2222            2222     2222    2222
 **  2222           222222222222222   2222
 **  2222           2222       2222   2222
 **   22222222222   2222       2222   222222222222222
 *      2222222222  2222       2222   22222222222222
 *
 *
 */


bool cal_handleEncoder() {
  sainchaw.ProcessDigitalControls();
  return sainchaw.encoder.FallingEdge();
}

void cal_read_voct(int input, bool onevolt) {
  Sainchaw::Ctrl i = (Sainchaw::Ctrl)(Sainchaw::PITCH_1_CTRL + input);
  sainchaw.controls[i].Process();
  float read_value = sainchaw.GetKnobValue(i);
  switch(input) {
    case 0:
      if(onevolt) {
        current_config.voct_1_1v = read_value;
      } else {
        current_config.voct_1_3v = read_value;
      }
      break;
    case 1:
      if(onevolt) {
        current_config.voct_2_1v = read_value;
      } else {
        current_config.voct_2_3v = read_value;
      }
      break;
    case 2:
      if(onevolt) {
        current_config.voct_3_1v = read_value;
      } else {
        current_config.voct_3_3v = read_value;
      }
      break;
    default: return; break;
  }
}

void handleCalibration() {
  sainchaw.StopAudio();
  bool calibrating  = true;
  int  complete     = 6;
  bool waiting      = true;
  int  current_step = 0;

  while(calibrating) {
    int voct = current_step % 3;
    if(voct == 0) {
      sainchaw.SetNoteLed(false);
      sainchaw.SetAltLed(false);
    } else if(voct == 1) {
      sainchaw.SetNoteLed(true);
      sainchaw.SetAltLed(false);
    } else {
      sainchaw.SetNoteLed(true);
      sainchaw.SetAltLed(true);
    }

    if(waiting) {
      waiting = !cal_handleEncoder();
      continue;
    }

    cal_read_voct(voct, current_step < 3);
    waiting = true;
    current_step++;
    if(current_step == complete) {
      save_config(kConfigSlot);
      bool ledstate = false;
      for(int i = 0; i < 1099999; i++) {
        if(i % 100000 == 0) {
          sainchaw.SetAltLed(ledstate);
          sainchaw.SetNoteLed(!ledstate);
          ledstate = !ledstate;
        }
      }
      sainchaw.StartAudio(AudioCallback);
    }
  }
}


void save_config(uint32_t slot) {
  uint32_t base = 0x90000000;
  base += slot * 4096;
  sainchaw.seed.qspi.Erase(base, base + sizeof(CONFIGURATION));
  sainchaw.seed.qspi.Write(base, sizeof(CONFIGURATION), (uint8_t *)&current_config);
}

void load_config(uint32_t slot) {
  memcpy(&current_config,
         reinterpret_cast<void *>(0x90000000 + (slot * 4096)),
         sizeof(CONFIGURATION));
}