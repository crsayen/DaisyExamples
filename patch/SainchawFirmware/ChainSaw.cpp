#include <string>

#include "Pitch.h"
#include "daisy_sainchaw.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

const int   kMaxOscsPerVoice                   = 9;
const float kDetuneScale                    = 0.05f;
const float kFmHzPerVolt                    = 100.0f;
const float kbrmalizationDetectionThreshold = -0.15f;
const int   kNumVoices                      = 3;
const int   kProbeSequenceDuration          = 32;
const int   kNumStereoChannels              = 2;

enum EncState { SEMITONES, CENTS, SWARM_SIZE, LAST };

Sainchaw                sainchaw;
VariableShapeOscillator swarms[kNumVoices][kMaxOscsPerVoice];


int           encoderState_;
int           swarmSize_;
float         amplitudeReduction_;
int           swarmCenterShift_;
int           semitones_;
int           cents_;
float         lpfm_;
bool          ignore_enc_switch_;
bool          voct_patched_[kNumVoices];
int           waitcount_;
Pitch         pitch[kNumVoices];

void UpdateControls();


static void
AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  UpdateControls();
  for(size_t i = 0; i < size; i++) {
    float sig = 0.f;

    for(size_t voice_n = 0; voice_n < kNumVoices; voice_n++) {
      if(voct_patched_[voice_n]) {
        for(int j = 0; j < swarmSize_; j++) { sig += swarms[voice_n][j].Process(); }
      }
    }
    sig *= .3f * amplitudeReduction_;

    out[0][i] = sig;
    out[1][i] = sig;
  }
}

void SetupOsc(float samplerate) {
  for(int i = 0; i < kMaxOscsPerVoice; i++) {
    for(int s = 0; s < kNumVoices; s++) {
      swarms[s][i].Init(samplerate);
      swarms[s][i].SetSync(true);
    }
  }
}

int main(void) {
  float samplerate;
  sainchaw.Init(); // Initialize hardware
  samplerate = sainchaw.AudioSampleRate();

  encoderState_       = EncState::SEMITONES;
  swarmSize_          = 5;
  swarmCenterShift_   = 2;
  amplitudeReduction_ = .12f / swarmSize_;
  semitones_          = 0;
  cents_              = 0;
  ignore_enc_switch_  = false;
  waitcount_          = 0;


  // for(int voice_n = 0; voice_n < kNumVoices; voice_n++) {
  //   bool is voi
  //   voct_patched_[voice_n] = ;
  //   pitch[voice_n].SetByNote(0);
  // }
  // TODO: handle more than one voice
  voct_patched_[0] = true;
  voct_patched_[1] = false;
  voct_patched_[2] = false;
  pitch[0].SetByNote(0);

  SetupOsc(samplerate);
  sainchaw.SetAltLed(false);
  sainchaw.SetNoteLed(true);

  sainchaw.StartAdc();
  sainchaw.StartAudio(AudioCallback);
}

void handleEncoder() {
  bool pushed = false;
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
    pushed = true;

    encoderState_++;
    if(encoderState_ == EncState::LAST) { encoderState_ = 0; }
  }

  int incr = sainchaw.encoder.Increment();

  if (incr != 0 || pushed) {

    switch(encoderState_) {
    case EncState::SEMITONES:
      semitones_ += incr;
      break;
    case EncState::CENTS:
      semitones_ += incr;
      break;
    case EncState::SWARM_SIZE:
      swarmSize_ = DSY_CLAMP(swarmSize_ + incr * 2, 1, kMaxOscsPerVoice);
      swarmCenterShift_ = floor(swarmCenterShift_ * .5f);
      break;
    }

    bool note_led_state
        = (encoderState_ == EncState::SEMITONES || encoderState_ == EncState::SWARM_SIZE);
    bool alt_led_state
        = (encoderState_ == EncState::CENTS || encoderState_ == EncState::SWARM_SIZE);

    sainchaw.SetNoteLed(note_led_state);
    sainchaw.SetAltLed(alt_led_state);
  }
}

void UpdateControls() {
  sainchaw.ProcessDigitalControls();
  sainchaw.ProcessAnalogControls();

  float shape_amt = DSY_CLAMP(
      ((sainchaw.GetKnobValue(Sainchaw::SHAPE_CTRL))), .0f, 1.f); // 0.0 to 1.0

  float detune_amt
      = DSY_CLAMP(((sainchaw.GetKnobValue(Sainchaw::DETUNE_CTRL))), .0f, 1.f)
        * kDetuneScale;

  // float fm_val = sainchaw.GetKnobValue(Sainchaw::FM_CTRL) * 8.f; //voltage
  // fonepole(lpfm_, fm_val, .001f);
  // ctrl[Sainchaw::FM_CTRL] = lpfm_;

  // float fm = ctrl[Sainchaw::FM_CTRL] * kFmHzPerVolt;

  // encoder
  handleEncoder();

  // Adjust oscillators based on inputs
  for(int voice_n = 0; voice_n < kNumVoices; voice_n++) {
    if(!voct_patched_[voice_n]) {
      continue;
    }

    float voltage
        = sainchaw.GetKnobValue((Sainchaw::Ctrl)(Sainchaw::PITCH_1_CTRL + voice_n)) * 5.f + 2;
    pitch[voice_n].SetByVoltage(voltage);
    pitch[voice_n].Transpose(semitones_, cents_);

    for(int osc = 0; osc < swarmSize_; osc++) {
     
      // pitch[voice_n].Transpose((osc - swarmCenterShift_) * detune_amt, 0);
      pitch[voice_n].Transpose((0 - floor(swarmSize_ / 2.f)) * detune_amt, 0);
      // pitch[i].Transpose(1, cents_);
      // swarms[s][i].SetFreq(shifted_pitch); // + fm);
      swarms[voice_n][osc].SetSyncFreq(pitch[voice_n].hz);
      swarms[voice_n][osc].SetFreq(pitch[voice_n].hz);
      swarms[voice_n][osc].SetPW(DSY_MIN(shape_amt, .5f));
      swarms[voice_n][osc].SetWaveshape(shape_amt);
    }
  }
}