#include "Squaw.h"
#include "dsp.h"

static inline float Polyblep(float phase_inc, float t);

constexpr float TWO_PI_RECIP = 1.0f / TWOPI_F;

float Squaw::Process() {
  float square, saw, out, t;

  t = phase_ * TWO_PI_RECIP;

  if(phase_ >= 0.0f) {
    saw    = (2.0f * t) - 1.0f;
    square = phase_ < PI_F ? 1.0f : -1.0f;

    saw -= Polyblep(phase_inc_, t);
    square += Polyblep(phase_inc_, t);
    square -= Polyblep(phase_inc_, fmodf(t + 0.5f, 1.0f));
  } else {
    saw = (2.0f * t) + 1.0f;
    saw += Polyblep(-phase_inc_, -t);
    square = phase_ < -PI_F ? 1.0f : -1.0f;
    square -= Polyblep(-phase_inc_, -t);
    square += Polyblep(-phase_inc_, -fmodf(t - 0.5f, 1.0f));
  }

  saw *= -1.0f;
  square *= 0.8f;

  phase_ += phase_inc_;
  if(phase_ > TWOPI_F) {
    phase_ -= TWOPI_F;
    eoc_ = true;
  } else if(phase_ < -TWOPI_F) {
    phase_ += TWOPI_F;
    eoc_ = true;
  } else {
    eoc_ = false;
  }
  eor_ = (phase_ - phase_inc_ < PI_F && phase_ >= PI_F);

  return (square + saw) * 0.5;
}

float Squaw::CalcPhaseInc(float f) { return (TWOPI_F * f) * sr_recip_; }

static float Polyblep(float phase_inc, float t) {
  float dt = phase_inc * TWO_PI_RECIP;
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
