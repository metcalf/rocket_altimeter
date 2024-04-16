import matplotlib.pyplot as plt
import sys

MAX_POSITIVE_VALUE = 10
MIN_NEGATIVE_VALUE = MAX_POSITIVE_VALUE - 15

bytes = sys.stdin.buffer.read()

raw_deltas = []
for b in bytes:
    raw_deltas.append(b & 0x0f + MIN_NEGATIVE_VALUE)
    raw_deltas.append((b >> 4) & 0x0f + MIN_NEGATIVE_VALUE)

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
    if(len(data) > 20):
        t_incr = 3
    else:
        t_incr = 0.5
    data.append([last_t + t_incr, last_a + d])

for datum in data:
    print("%f,%d" % datum)

plt.scatter(*zip(*data))
plt.show()
