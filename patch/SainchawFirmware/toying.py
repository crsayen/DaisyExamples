import time
TWO_PI_RECIP = 0.15915494
TWOPI_F = 2.0 * 3.1415927410125732421875
SQUARE_FUDGE = 0.83333

def calculate_tri(phase_, pw_):
    saw = 0;
    square = 0;
    t = phase_ * TWO_PI_RECIP;
    t_scaled_to_pw = (t + 1.0) / 2.0

    if pw_ < 0.5:
        if t_scaled_to_pw < pw_:
            square = pw_ * SQUARE_FUDGE
            saw =   (-1 * t_scaled_to_pw) + pw_
        else:
            square = 0
            saw = t_scaled_to_pw - pw_
    else:
        if phase_ < 0.0:
            square = pw_ * SQUARE_FUDGE
            saw =  -0.5 * t
        else:
            square = 0
            saw = 0.5 * t
        saw *= 2 - 2 * pw_
    return 2 * (square + saw) - 1, t, t_scaled_to_pw
                                
# print(calculate(0))
# print(calculate(0.25))


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

resolution = 25

def do_cycle(pw):
    for i in range(resolution):
        p = ((i / (resolution / 2)) - 1) * TWOPI_F
        val, t, t_s = calculate_tri(p, pw)
        # val = calculate_tri(p, pw)
        line = f'.{" " * (resolution)}.{" " * (resolution)}.'
        idx = int(((val * 0.5) + 0.5) * (2 * resolution))
        idx = idx if 0 < idx < (resolution * 2) else None
        stats = f'v:{str(round(val, 3))[0:4]}, t:{str(round(t, 3))[0:4]}, ts:{str(round(t_s, 3))[0:4]}'
        line_list = [*line]
        if idx is not None: line_list[idx] = 'x'
        line = ''.join(line_list)
        print(f"{line}  {stats}")

for percent in range(0, 100, 5):
    pw = percent / 100
    do_cycle(pw)
    time.sleep(0.05)