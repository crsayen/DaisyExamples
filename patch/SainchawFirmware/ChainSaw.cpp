#include <string>

#include "Pitch.h"
#include "daisy_sainchaw.h"
#include "daisysp.h"
// #include "configuration.h"
#include "Squaw.h"

using namespace daisy;
using namespace daisysp;

const int   kMaxOscsPerVoice                = 13;
const int   kMinOscsPerVoice                = 3;
const float kDetuneScale                    = 0.15f;
const float kFmHzPerVolt                    = 100.0f;
const float kbrmalizationDetectionThreshold = -0.15f;
const int   kNumVoices                      = 5;
const int   kNumStereoChannels              = 2;


#define LOAD_METER
#ifdef LOAD_METER
CpuLoadMeter cpuLoadMeter;
#endif

enum EncState { SEMITONES, CENTS, SWARM_SIZE, LAST };

Sainchaw                sainchaw;
Squaw                   swarms[kNumVoices][kMaxOscsPerVoice];



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
bool          cpu_meter_one;
bool          cpu_meter_two;
Pitch         pitch[kNumVoices];

void UpdateControls();

void blockStart() {
#ifdef LOAD_METER
  cpuLoadMeter.OnBlockStart();
#endif
}

void blockEnd() {
#ifdef LOAD_METER
  cpuLoadMeter.OnBlockEnd();
  // float load = cpuLoadMeter.GetAvgCpuLoad();
  float load = cpuLoadMeter.GetMaxCpuLoad();
  if(load > .75f) {
    cpu_meter_one = true;
    cpu_meter_two = true;
  } else if(load > .5f) {
    cpu_meter_one = true;
    cpu_meter_two = false;
  } else if(load > .25f) {
    cpu_meter_one = false;
    cpu_meter_two = true;
  } else {
    cpu_meter_one = false;
    cpu_meter_two = false;
  }
#endif
}

#ifdef LOAD_METER
void displayLoadMeter() {
  sainchaw.SetAltLed(cpu_meter_two);
  sainchaw.SetNoteLed(cpu_meter_one);
}
#endif

static void
AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  blockStart();
  UpdateControls();
  for(size_t i = 0; i < size; i++) {
    float sigr = 0.f;
    float sigl = 0.f;
    // float sigm = 0.f;

    for(size_t voice_n = 0; voice_n < kNumVoices; voice_n++) {
      if(voct_patched_[voice_n]) {
        for(int j = 0; j < swarmSize_; j++) {
          float sig = swarms[voice_n][j].Process();
          switch (j)
          {
          case 0:
            
            sigl += sig;
            sigr += sig;
            break;
          case 1:
            sigl += sig;
            // sigr += sig;
            break;
          case 2:
            // sigl += sig;
            sigr += sig;
            break;
          case 5:
            // sigl += sig;
            sigr += sig;
            break;
          case 6:
            sigl += sig;
            // sigr += sig;
            break;
          case 9:
            sigl += sig;
            // sigr += sig;
            break;
          case 10:
            // sigl += sig;
            sigr += sig;
            break;
          
          default:
            sigl += sig;
            sigr += sig;
            break;
          }
        }
      }
    }
    // sig *= .3f * amplitudeReduction_;
    // sig *= .3f * amplitudeReduction_;
    // sig *= .3f * amplitudeReduction_;
    // sigl += sigm;
    // sigr += sigm;
    sigl *= 0.5f * amplitudeReduction_;
    sigr *= 0.5f * amplitudeReduction_;

    out[0][i] = sigl;
    out[1][i] = sigr;
  }
  blockEnd();
}

void SetupOsc(float samplerate) {
  for(int i = 0; i < kMaxOscsPerVoice; i++) {
    for(int s = 0; s < kNumVoices; s++) {
      swarms[s][i].Init(samplerate);
    }
  }
}

int main(void) {
  float samplerate;
  sainchaw.Init(); // Initialize hardware
  // sainchaw.SetAudioSampleRate()
  samplerate = sainchaw.AudioSampleRate();

  encoderState_       = EncState::SEMITONES;
  swarmSize_          = 3;
  swarmCenterShift_   = 2;
  amplitudeReduction_ = .12f / swarmSize_;
  semitones_          = -12;
  cents_              = 0;
  ignore_enc_switch_  = false;
  waitcount_          = 0;

#ifdef LOAD_METER
  cpuLoadMeter.Init(samplerate, sainchaw.AudioBlockSize());
#endif

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
      swarmSize_ = DSY_CLAMP(swarmSize_ + incr * 2, kMinOscsPerVoice, kMaxOscsPerVoice);
      swarmCenterShift_ = floor(swarmCenterShift_ * .5f);
      break;
    }

    bool note_led_state
        = (encoderState_ == EncState::SEMITONES || encoderState_ == EncState::SWARM_SIZE);
    bool alt_led_state
        = (encoderState_ == EncState::CENTS || encoderState_ == EncState::SWARM_SIZE);
#ifndef LOAD_METER
    sainchaw.SetNoteLed(note_led_state);
    sainchaw.SetAltLed(alt_led_state);
#endif
  }
}

void UpdateControls() {
#ifdef LOAD_METER
  displayLoadMeter();
#endif
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

    for(int osc = 0; osc < swarmSize_; osc++) {
      pitch[voice_n].SetByVoltage(voltage);
      pitch[voice_n].Transpose(semitones_, cents_);
      pitch[voice_n].Transpose((osc - swarmCenterShift_) * detune_amt, 0);
      swarms[voice_n][osc].SetFreq(pitch[voice_n].hz);
      swarms[voice_n][osc].SetShape(shape_amt);
    }
  }
}