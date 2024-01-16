#include "Squaw.h"
// #include "dsp.h"

const float ONE_THIRD = 1.0f / 3.0f;
const float TWO_THIRDS = 2.0f * ONE_THIRD;

static inline float Polyblep(float phase_inc, float t);


float Squaw::Process() {
  float square, saw, tri, fade_in;

  saw = (2.0f * phase_) - 1.0f;
  square = phase_ < pw_ ? 1.0f : -1.0;
  tri = phase_ < 0.5f ? 1.0f : -1.0f;

  float half_cycle_blep = Polyblep(phase_inc_, phase_);
  saw -= half_cycle_blep;
  square += half_cycle_blep;
  tri += half_cycle_blep;
  square -= Polyblep(phase_inc_, fmodf(phase_ + (1.0f - pw_), 1.0f));
  tri -= Polyblep(phase_inc_, fmodf(phase_ + 0.5f, 1.0f));

  saw *= -1.0f;
  square *= -0.707f;
  tri  = phase_inc_ * tri + (1.0f - phase_inc_) * last_tri_;
  last_tri_ = tri;
  tri *= -4.0f;

  fade_in = fmodf(pw_, ONE_THIRD) * 3.0f;
  if (pw_ < ONE_THIRD) {
      tri *= fade_in;
      square *= fade_in;
  } else if (pw_ < TWO_THIRDS) {
      saw *= 1.0f - fade_in;
      tri *= DSY_MAX(1.0f - fade_in - fade_in, 0.0f);
  } else {
      tri = 0.0f;
      saw = 0.0f;
  }
      
        
  phase_ += phase_inc_;
  if (phase_ > 1.0f) {
      phase_ -= 1.0f;
  }

  return (saw + tri + square) * ONE_THIRD;
}

float Squaw::CalcPhaseInc(float f) { return f * sr_recip_; }

static float Polyblep(float dt, float t) {
  if(t < dt) {
    t /= dt;
    return t + t - t * t - 1.0f;
  } else if(t > 1.0f - dt) {
    t = (t - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  } else {
    return 0.0f;
  }
}


