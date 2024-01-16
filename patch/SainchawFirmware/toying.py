import math
import matplotlib.pyplot as plt
one_third = 1 / 3
two_thirds = 2 * one_third
last_tri = 0;

def polyblep(phase_inc, t):
  dt = phase_inc
  if(t < dt):
    t /= dt
    return t + t - t * t - 1.0
  elif t > 1.0 - dt:
    t = (t - 1.0) / dt
    return t * t + t + t + 1.0
  else:
    return 0.0

def calc_phase_inc(f): return f * sr_recip_

sr_        = 48000
sr_recip_  = 1.0 / sr_
freq_      = 100.0
phase_inc = calc_phase_inc(freq_)
phase_ = 0


def calculate_tri(pw_):
    global phase_, last_tri

    saw = 0;
    square = 0;

    saw = (2.0 * phase_) - 1.0
    square = 1 if phase_ < pw_ else -1.0
    tri = 1 if phase_ < 0.5 else -1.0

    half_cycle_blep = polyblep(phase_inc, phase_)
    saw -= half_cycle_blep
    square += half_cycle_blep
    tri += half_cycle_blep
    square -= polyblep(phase_inc, math.fmod(phase_ + (1.0 - pw_), 1.0))
    tri -= polyblep(phase_inc, math.fmod(phase_ + 0.5, 1.0))

    saw *= -1.0;
    square *= -0.707
    tri  = phase_inc * tri + (1.0 - phase_inc) * last_tri
    last_tri = tri;
    tri *= -4.0

    fade_in = math.fmod(pw_, one_third) * 3
    fade_out = 1 - fade_in
    fast_fade = max(fade_out - fade_in, 0)
    if 0 <= pw_ < one_third:
       tri *= fade_in
       square *= fade_in
    if one_third <= pw_ < two_thirds:
       saw *= fade_out
       tri *= fast_fade
    if two_thirds <= pw_ < 1:
       tri = 0
       saw = 0
       
        
    phase_ += phase_inc;
    if phase_ > 1.0:
        phase_ -= 1.0

    return saw, square, tri




def print_plot():
    pw = 0.2
    n_cycles = 30
    sweep_pw = False
    plot_spread = 2
    sweep_pw = True

    _n_samples = 48000 // (100 // n_cycles)
    _saw_samples = []
    _square_samples = []
    _tri_samples = []
    _osc_samples = []
    for i in range(_n_samples):
        if sweep_pw:
            pw = i / _n_samples
        saw, square, tri = calculate_tri(pw)
        _saw_samples.append(saw - plot_spread)
        _square_samples.append(square + plot_spread)
        _tri_samples.append(tri + 2 * plot_spread)
        _osc_samples.append((saw + square + tri) / 2)
    plt.plot(_saw_samples)
    plt.plot(_square_samples)
    plt.plot(_tri_samples)
    plt.plot(_osc_samples)
    plt.show()
    
print_plot()

