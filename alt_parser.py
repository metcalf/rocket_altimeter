import matplotlib.pyplot as plt
import sys
import csv
import os.path
import click

MAX_POSITIVE_VALUE = 10
MIN_NEGATIVE_VALUE = MAX_POSITIVE_VALUE - 15

mode = sys.argv[1]

input_fn = sys.argv[2]
if input_fn == '-':
    bytes = sys.stdin.buffer.read()
    csv_fn = None
    txt_fn = None
    png_fn = None
else:
    bytes = open(input_fn, 'rb').read()
    csv_fn = input_fn + ".csv"
    txt_fn = input_fn + ".txt"
    png_fn = input_fn + ".png"

    if any(os.path.exists(p) for p in (csv_fn, txt_fn, png_fn)):
        if not click.confirm(f"Output path {input_fn}.* exists, overwrite?"):
            sys.exit(1)

if len(sys.argv) > 3:
    xlim, ylim = (int(v) for v in sys.argv[3].split(","))
else:
    xlim = None
    ylim = None

if mode == 'rocket':
    PA_INTERVAL = 18
    FAST_INTERVAL_INVERSE_SECS = 8
    SLOW_INTERVAL_SECS = 1
    FAST_INTERVAL_RECORDS = 80
elif mode == 'throw':
    PA_INTERVAL = 5 # ~1.5'
    FAST_INTERVAL_INVERSE_SECS = 10
    SLOW_INTERVAL_SECS = 1
    FAST_INTERVAL_RECORDS = 200
elif mode == 'electric':
    PA_INTERVAL = 17 # ~5'
    FAST_INTERVAL_INVERSE_SECS = 4
    SLOW_INTERVAL_SECS = 1
    FAST_INTERVAL_RECORDS = 40
elif mode == 'kite':
    PA_INTERVAL = 11 # ~3'
    FAST_INTERVAL_INVERSE_SECS = 2
    SLOW_INTERVAL_SECS = 1
    FAST_INTERVAL_RECORDS = 256
else:
    print("Unknown mode: %s" % mode)
    sys.exit(1)

FAST_INTERVAL_SECS = 1.0 / FAST_INTERVAL_INVERSE_SECS
FEET_PER_INTERVAL = (PA_INTERVAL/3.6)

def plot_data(data, xlim, ylim):
    plt_x = []
    plt_y = []
    last_a = -1234
    for (t, a) in data:
        # Skip data points with no altitude difference from the prior point to avoid
        # jagged plotting due to sampling coarseness
        if a != last_a:
            plt_x.append(t)
            plt_y.append(a)
        last_a = a

    plt.plot(plt_x, plt_y)
    if xlim is not None:
        plt.xlim(0, xlim)
    if ylim is not None:
        plt.ylim(0, ylim)

    if png_fn is not None:
        plt.savefig(png_fn)

    plt.show()

def write_data(data, csv_fn, txt_fn):
    if csv_fn is not None:
        with open(csv_fn, 'w') as csv_f:
            writer = csv.writer(csv_f)
            writer.writerow(('time (secs)', 'altitude (ft)', 'speed (ft/s)'))
            last_t = -1
            last_a = 0
            for (t, a) in data:
                delta_a = a - last_a
                s = delta_a / (t - last_t)
                last_t = t
                last_a = a
                writer.writerow((t, round(a), round(s)))

    if txt_fn is not None:
        with open(txt_fn, 'w') as txt_f:
            txt_f.write(f"""
PA_INTERVAL={PA_INTERVAL}
FAST_INTERVAL_INVERSE_SECS={FAST_INTERVAL_INVERSE_SECS}
SLOW_INTERVAL_SECS={SLOW_INTERVAL_SECS}
FAST_INTERVAL_RECORDS={FAST_INTERVAL_RECORDS}
FAST_INTERVAL_SECS={FAST_INTERVAL_SECS}
FEET_PER_INTERVAL={FEET_PER_INTERVAL}
    """)

def parse_data(bytes):
    raw_deltas = []
    for b in bytes:
        raw_deltas.append((b & 0x0f) + MIN_NEGATIVE_VALUE)
        raw_deltas.append(((b >> 4) & 0x0f) + MIN_NEGATIVE_VALUE)

    carry = 0
    deltas = []
    for rd in raw_deltas:
        if(rd == MAX_POSITIVE_VALUE or rd == MIN_NEGATIVE_VALUE):
            carry = carry + rd
        else:
            deltas.append(carry + rd)
            carry = 0

    data = [[0, 0]]
    for d in deltas:
        last_t, last_a = data[-1]
        if(len(data) > FAST_INTERVAL_RECORDS):
            t_incr = SLOW_INTERVAL_SECS
        else:
            t_incr = FAST_INTERVAL_SECS
        data.append([last_t + t_incr, last_a + d * FEET_PER_INTERVAL])

    return data

data = parse_data(bytes)
write_data(data, csv_fn, txt_fn)
plot_data(data, xlim, ylim)
