#include <string>

#include "Pitch.h"
#include "daisy_sainchaw.h"
#include "daisysp.h"
#include "configuration.h"
#include "Squaw.h"
// #define CAL
#define STORE_SETTINGS
// #define LOAD_METER

using namespace daisy;
using namespace daisysp;

const uint32_t kConfigTag                      = 3141592653;
const int      kMaxOscsPerVoice                = 7;
const int      kMinOscsPerVoice                = 1;
const float    kDetuneScale                    = 0.15f;
const float    kFmHzPerVolt                    = 100.0f;
const float    kbrmalizationDetectionThreshold = -0.15f;
const int      kNumVoices                      = 3;
const int      kNumStereoChannels              = 2;
const int      kNumControlSteps                = 7;
const uint16_t kSettingsSaveInterval           = 10000;


void UpdateControls();
void save_settings();
void load_settings();




#ifdef LOAD_METER
CpuLoadMeter cpuLoadMeter;
#endif

enum EncState { SEMITONES, CENTS, SWARM_SIZE, ACTIVE_VOICES, LAST };

Sainchaw                sainchaw;
Squaw                   swarms[kNumVoices][kMaxOscsPerVoice];

SETTINGS         settings;
float            amplitudeReduction_;
int              swarmCenterShift_;
int              encoderState_;
uint16_t         save_settings_counter;
float            lpfm_;
bool             ignore_enc_switch_;
bool             voct_patched_[kNumVoices];
int              waitcount_;
size_t           control_step_;
bool             cpu_meter_one;
bool             cpu_meter_two;
volatile bool             settings_changed;
Pitch            pitch[kNumVoices];
volatile float   prevent_optimization;

void blockStart() {
#ifdef LOAD_METER
  cpuLoadMeter.OnBlockStart();
#endif
}

#ifdef LOAD_METER
void blockEnd() {
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
}
#endif

#ifdef LOAD_METER
void displayLoadMeter() {
  sainchaw.SetAltLed(cpu_meter_two);
  sainchaw.SetNoteLed(cpu_meter_one);
}
#endif

void setSwarmSize(int val) {
  settings.swarmSize = val;
  settings_changed = true;
}

void setActiveVoices(int val) {
  settings.activeVoices = val;
  settings_changed = true;
}

void setSwarmCenterShift(int val) {
  settings.swarmCenterShift = val;
  settings_changed = true;
}

void setSemitones(int val) {
  settings.semitones = val;
  settings_changed = true;
}

void setCents(int val) {
  settings.cents = val;
  settings_changed = true;
}


static void
AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  #ifdef LOAD_METER
  blockStart();
  #endif

  for(size_t i = 0; i < size; i++) {
    float sigr = 0.f;
    float sigl = 0.f;

    for(size_t voice_n = 0; voice_n < kNumVoices; voice_n++) {
      if(voct_patched_[voice_n]) {
        for(int j = 0; j < settings.swarmSize; j++) {
          float sig = swarms[voice_n][j].Process();
          switch (j)
          {
          case 1:
            sigl += sig;
            break;
          case 2:
            sigr += sig;
            break;
          case 5:
            sigr += sig;
            break;
          case 6:
            sigl += sig;
            break;
          case 9:
            sigl += sig;
            break;
          case 10:
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

    sigl *= 0.5f * amplitudeReduction_;
    sigr *= 0.5f * amplitudeReduction_;

    out[0][i] = sigl;
    out[1][i] = sigr;
  }


  #ifdef LOAD_METER
  blockEnd();
  #endif
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
  sainchaw.Init();
  samplerate = sainchaw.AudioSampleRate();
  load_settings();
  #ifdef CAL
  setSemitones(0);
  setCents(0);
  setSwarmSize(1);
  #endif
  
  #ifdef LOAD_METER
  cpuLoadMeter.Init(samplerate, sainchaw.AudioBlockSize());
  #endif

  encoderState_         = EncState::SEMITONES;
  amplitudeReduction_   = .12f / settings.swarmSize;
  ignore_enc_switch_    = false;
  waitcount_            = 0;
  waitcount_            = 0;
  save_settings_counter = 0;
  settings_changed      = false;

  #ifndef CAL
  for (int i = 0; i < kNumVoices; i++) {
    voct_patched_[i] = i < settings.activeVoices;
  }
  #endif
  #ifdef CAL
  voct_patched_[0] = false;
  voct_patched_[2] = false;
  voct_patched_[1] = true;
  #endif

  pitch[0].SetByNote(0);

  SetupOsc(samplerate);
  sainchaw.SetAltLed(false);
  sainchaw.SetNoteLed(true);

  sainchaw.StartAdc();
  sainchaw.StartAudio(AudioCallback);
  while (1) {
      UpdateControls();
  }
}

void handleEncoder() {
  bool pushed = false;
  if(sainchaw.encoder.TimeHeldMs() >= 1000) {
    setCents(0);
    setSemitones(0);
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
      setSemitones(settings.semitones +incr);
      break;
    case EncState::CENTS:
      setCents(settings.cents + incr);
      break;
    case EncState::SWARM_SIZE:
      setSwarmSize(DSY_CLAMP(settings.swarmSize + incr * 2, kMinOscsPerVoice, kMaxOscsPerVoice));
      swarmCenterShift_ = ceil(settings.swarmSize * .5f);
      break;
    case EncState::ACTIVE_VOICES:
      setActiveVoices(DSY_CLAMP(settings.activeVoices + incr, 0, 3));
      break;
    }

    #ifndef CAL
    for(int i = 0; i < kNumVoices; i++) { voct_patched_[i] = i < settings.activeVoices; }
    #endif

    #ifndef LOAD_METER
    if (pushed) {
      sainchaw.SetNoteLed(encoderState_ == EncState::SEMITONES || encoderState_ == EncState::SWARM_SIZE);
      sainchaw.SetAltLed(encoderState_ == EncState::CENTS || encoderState_ == EncState::SWARM_SIZE);
    }
    #endif
  }
}

float getCalibratedVoltage(int voice_n) {
  float raw_value = sainchaw.GetKnobValue((Sainchaw::Ctrl)(Sainchaw::PITCH_1_CTRL + voice_n));
  switch (voice_n)
  {
  case 0:
    return raw_value * 5.02f - 0.93f;
  case 1:
    return raw_value * 5.013f - 0.91f;
  case 2:
    return raw_value * 5.f + 2.f;
  default:
    return raw_value * 5.f + 2.f;
    break;
  }
}

void doControlStep() {
  sainchaw.ProcessDigitalControls();
  switch (control_step_)
  {
  case 0:
    sainchaw.ProcessAnalogControl(0);
    break;
  case 1:
    sainchaw.ProcessAnalogControl(1);
    break;
  case 2:
    sainchaw.ProcessAnalogControl(2);
    break;
  case 3:
    sainchaw.ProcessAnalogControl(3);
    break;
  case 4:
    sainchaw.ProcessAnalogControl(4);
    break;
  case 5:
    sainchaw.ProcessAnalogControl(5);
    break;
  default:
    break;
  }

  control_step_++;
  if (control_step_ >= kNumControlSteps) { control_step_ = 0; }
}

void UpdateControls() {
#ifdef LOAD_METER
  displayLoadMeter();
#endif
  
  // doControlStep();
  sainchaw.ProcessAnalogControls();
  sainchaw.ProcessDigitalControls();


  float shape_amt = DSY_CLAMP(((sainchaw.GetKnobValue(Sainchaw::SHAPE_CTRL))), .0f, 1.f);
  float detune_amt = DSY_CLAMP(((sainchaw.GetKnobValue(Sainchaw::DETUNE_CTRL))), .0f, 1.f) * kDetuneScale;

  // float fm_val = sainchaw.GetKnobValue(Sainchaw::FM_CTRL) * 8.f; //voltage
  // fonepole(lpfm_, fm_val, .001f);
  // ctrl[Sainchaw::FM_CTRL] = lpfm_;
  // float fm = ctrl[Sainchaw::FM_CTRL] * kFmHzPerVolt;

  handleEncoder();

  for(int voice_n = 0; voice_n < kNumVoices; voice_n++) {
    if(!voct_patched_[voice_n]) { continue; }

    float voltage = getCalibratedVoltage(voice_n);

    for(int osc = 0; osc < settings.swarmSize; osc++) {
      pitch[voice_n].SetByVoltage(voltage);
      pitch[voice_n].Transpose(settings.semitones, settings.cents);
      pitch[voice_n].Transpose((osc - swarmCenterShift_) * detune_amt, 0);
      swarms[voice_n][osc].SetFreq(pitch[voice_n].hz);
      swarms[voice_n][osc].SetShape(shape_amt);
    }
  }

  #ifndef CAL
  #ifdef STORE_SETTINGS
  if (save_settings_counter >= kSettingsSaveInterval) {
    save_settings();
    save_settings_counter = 0;
  }
  save_settings_counter++;
  #endif
  #endif
}

void save_settings() {
  if (settings_changed) {
    settings.tag = kConfigTag;
    uint32_t base = 0x90000000;
    base += 4096;
    sainchaw.seed.qspi.Erase(base, base + sizeof(SETTINGS));
    sainchaw.seed.qspi.Write(base, sizeof(SETTINGS), (uint8_t*)&settings);
  }
  settings_changed = false;
}

  
void load_settings() {
  memcpy(&settings, reinterpret_cast<void*>(0x90000000 + 4096), sizeof(SETTINGS));
  if (settings.tag != kConfigTag) {
    setSwarmSize(3);
    setActiveVoices(1);
    setSwarmCenterShift(1);
    setSemitones(0);
    setCents(0);
  }
}