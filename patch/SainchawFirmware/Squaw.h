#pragma once
#include "Utility/dsp.h"
#include <stdint.h>

class Squaw {
public:
  Squaw() {}
  ~Squaw() {}


  /** Initializes the Oscillator

      \param sample_rate - sample rate of the audio engine being run, and the frequency
     that the Process function will be called.

      Defaults:
      - freq_ = 100 Hz
      - amp_ = 0.5
      - waveform_ = sine wave.
  */
  void Init(float sample_rate) {
    sr_        = sample_rate;
    sr_recip_  = 1.0f / sample_rate;
    freq_      = 50.0f;
    phase_     = 0.0f;
    phase_inc_ = CalcPhaseInc(freq_);
    last_tri_ = 0.0f;
  }


  /** Changes the frequency of the Oscillator, and recalculates phase increment.
   */
  inline void SetFreq(const float f) {
    freq_      = f;
    phase_inc_ = CalcPhaseInc(f);
  }

  /** Processes the waveform to be generated, returning one sample. This should be called
   * once per sample period.
   */
  float Process();

  void SetShape(float _pw = 0.5f) { pw_ = _pw; }

private:
  float   CalcPhaseInc(float f);
  uint8_t waveform_;
  float   amp_, freq_;
  float   sr_, sr_recip_, phase_, phase_inc_, pw_, last_tri_;
};
