import math
import matplotlib.pyplot as plt
TWO_PI_RECIP = 0.15915494
PI_F = 3.1415927410125732421875
TWOPI_F = 2.0 * PI_F
SQUARE_FUDGE = 0.83333

def polyblep(phase_inc, t):
  dt = phase_inc * TWO_PI_RECIP
  if(t < dt):
    t /= dt
    return t + t - t * t - 1.0
  elif t > 1.0 - dt:
    t = (t - 1.0) / dt
    return t * t + t + t + 1.0
  else:
    return 0.0

def calc_phase_inc(f): return (TWOPI_F * f) * sr_recip_

sr_        = 48000
sr_recip_  = 1.0 / sr_
freq_      = 100.0
phase_inc = calc_phase_inc(freq_)
phase_ = 0


def calculate_tri(pw_):
    global phase_
    saw = 0;
    square = 0;
    pw_scaled_0_2pi = pw_ * TWOPI_F
    pw_scaled_m1_1 = pw_ * 2 - 1
    phase_scaled_m1_1 = phase_ * TWO_PI_RECIP * 2 - 1
    t = phase_scaled_m1_1 * 0.5 + 0.5
    
    pw_lt_half = pw_ < 0.5
    phase_lt_half = phase_ < PI_F

    if pw_lt_half:
        if phase_ < pw_scaled_0_2pi:
            square = (pw_scaled_m1_1 * 2 + 1)
            saw = phase_scaled_m1_1
        else:
            square = -1
            saw = -phase_scaled_m1_1
        square += polyblep(phase_inc, t);
        square -= polyblep(phase_inc, math.fmod(t + (1 - pw_), 1.0));
        saw += polyblep(phase_inc, t);
        saw -= polyblep(phase_inc, math.fmod(t + (1 - pw_), 1.0));
    else:
        if phase_lt_half:
            square = 1
            saw = phase_scaled_m1_1
        else:
            square = -1
            saw = -phase_scaled_m1_1
        square += polyblep(phase_inc, t);
        square -= polyblep(phase_inc, math.fmod(t + 0.5, 1.0));
        saw += polyblep(phase_inc, t);
        saw -= polyblep(phase_inc, math.fmod(t + 0.5, 1.0));
        saw *= 2 - 2 * pw_
        
    phase_ += phase_inc;
    if phase_ > TWOPI_F:
        phase_ -= TWOPI_F

    return saw, square, (saw + square) * 0.45




def print_plot():
    pw = 0.25
    n_cycles = 5
    sweep_pw = False
    plot_spread = 0
    # sweep_pw = True

    _n_samples = 48000 // (100 // n_cycles)
    _saw_samples = []
    _square_samples = []
    _osc_samples = []
    for i in range(_n_samples):
        if sweep_pw:
            pw = i / _n_samples
        saw, square, osc = calculate_tri(pw)
        _saw_samples.append(saw - plot_spread)
        _square_samples.append(square + plot_spread)
        _osc_samples.append(osc)
    plt.plot(_saw_samples)
    plt.plot(_square_samples)
    plt.plot(_osc_samples)
    plt.show()
    
print_plot()

