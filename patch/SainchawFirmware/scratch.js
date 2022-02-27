const PI_F = 3.1416
const TWOPI_F = PI_F * 2
const TWO_PI_RECIP = 1.0 / TWOPI_F
let phase_ = 0
let phase_n = 0

function phase_inc(f) {
  return TWOPI_F * f * (1 / 48000)
}

function polyblep(phase_inc, t) {
  const dt = phase_inc * TWO_PI_RECIP
  if (t < dt) {
    t /= dt
    return t + t - t * t - 1.0
  } else if (t > 1.0 - dt) {
    t = (t - 1.0) / dt
    return t * t + t + t + 1.0
  } else {
    return 0.0
  }
}

function process(f) {
  let out
  let t
  let phaseinc = phase_inc(f)

  // t = phase_ * TWO_PI_RECIP
  // // console.log(t)
  // if (phaseinc >= 0) {
  //   out = 2.0 * t - 1.0
  //   out -= polyblep(Math.abs(phaseinc), Math.abs(t))
  // } else {
  //   out = 2.0 * t + 1.0
  //   out += polyblep(Math.abs(phaseinc), Math.abs(t))
  // }

  // out *= -1.0

  // case WAVE_POLYBLEP_SQUARE:
  t = phase_ * TWO_PI_RECIP
  if (phaseinc > 0) {
    out = phase_ < PI_F ? 1.0 : -1.0
    out += polyblep(phaseinc, t)
    out -= polyblep(phaseinc, (t + 0.5) % 1.0)
  } else {
    out = phase_ < -PI_F ? 1.0 : -1.0
    out -= polyblep(-phaseinc, -t)
    out += polyblep(-phaseinc, -(t - 0.5) % 1.0)
  }
  out *= 0.707 // ?

  phase_ += phaseinc
  if (phase_ > TWOPI_F) {
    phase_ -= TWOPI_F
  } else if (phase_ < -TWOPI_F) {
    phase_ += TWOPI_F
  }
  return out
}

function r(n) {
  return Math.round(n * 100) / 100
}

for (let i = 0; i < 40; i++) {
  let out = process(-3000)
  console.log(out)
}
