#include "Squaw.h"
// #include "dsp.h"

static inline float Polyblep(float phase_inc, float t);

constexpr float TWO_PI_RECIP = 1.0f / TWOPI_F;
const float SQUARE_FUDGE = 0.83333f;


// def calculate_tri(phase_, pw_):
//     saw = 0;
//     square = 0;
//     t = phase_ * TWO_PI_RECIP;
//     t_scaled_to_pw = (t + 1.0) / 2.0

//     if pw_ < 0.5:
//         if t_scaled_to_pw < pw:
//             square = pw_ * SQUARE_FUDGE
//             saw =   (-1 * t_scaled_to_pw) + pw_
//         else:
//             square = 0
//             saw = t_scaled_to_pw - pw_
//     else:
//         if phase_ < 0.0:
//             square = pw_ * SQUARE_FUDGE
//             saw =  -0.5 * t
//         else:
//             square = 0
//             saw = 0.5 * t
//         saw *= 2 - 2 * pw_
//     return 2 * (square + saw) - 1, t, t_scaled_to_pw

float Squaw::Process() {
  float square, saw, out, t;

  // t goes from -1.0 to 1.0
  t = phase_ * TWO_PI_RECIP;
  float t_scaled_to_pw = (t + 1.f) / 2.f;

  if (pw_ < .5f) {
    if (t_scaled_to_pw < pw_) {
      square = pw_ * SQUARE_FUDGE;
      saw = (-1.f * t_scaled_to_pw) + pw_;
      saw += Polyblep(-phase_inc_, -t);
    } else {
      square = 0.f;
      saw = t_scaled_to_pw - pw_;
      saw += Polyblep(-phase_inc_, -t);
    }
  } else {
    if (phase_ < 0.0) {
      square = pw_ * SQUARE_FUDGE;
      square += Polyblep(phase_inc_, t);
      square -= Polyblep(phase_inc_, fmodf(t + 0.5f, 1.0f));
      saw = -.5f * t;
      saw -= Polyblep(phase_inc_, t);
    } else {
      square = 0.f;
      saw = 0.5f * t;
    }
    saw *= 2.f - 2.f * pw_
  }

  if(phase_ >= 0.0f) {
    if (pw_ < 0.5f) {
      saw = (2.0f * psuedo_t) + 1.0f;
      saw += Polyblep(-phase_inc_, -t);
    } else {
      saw = (2.0f * t) + 1.0f;
      saw += Polyblep(-phase_inc_, -t);
    }
    // saw    = (2.0f * t) - 1.0f;
    // square = phase_ < PI_F ? 1.0f : -1.0f;

    // saw -= Polyblep(phase_inc_, t);
    // square += Polyblep(phase_inc_, t);
    // square -= Polyblep(phase_inc_, fmodf(t + 0.5f, 1.0f));
    
  } else {
    if (pw_ < 0.5f) {
      if (t_scaled_to_pw >= pw_) {
        saw = (2.0f * psuedo_t) + 1.0f;
        saw += Polyblep(-phase_inc_, -t);
      } else {
        saw = 1.f - ((2.0f * psuedo_t) + 1.0f);
        saw += Polyblep(-phase_inc_, -t);
      }
    } else {
      saw = (2.0f * t) + 1.0f;
      saw += Polyblep(-phase_inc_, -t);
    }
    // saw = (2.0f * t) + 1.0f;
    // saw += Polyblep(-phase_inc_, -t);
    square = phase_ < -inflection ? 1.0f : -1.0f;
    square -= Polyblep(-phase_inc_, -t);
    square += Polyblep(-phase_inc_, -fmodf(t - 0.5f, 1.0f));
  }

  saw *= -1.0f;
  square *= 0.8f  * SQUARE_FUDGE * pw_;

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


