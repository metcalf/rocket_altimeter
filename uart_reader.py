import serial
import time

def test_measuring_and_printing(ser):
    while True:
        s = ser.read(1)
        if s == b'+':
            break
        print(s)

    ser.read(3)

    last = time.time()
    while True:
        bts = ser.read(4)

        if len(bts) < 4:
           continue

        now = time.time()
        # print("%s\t%s%d" % (":".join(hex(b) for b in bts[1:]), chr(bts[0]), int.from_bytes(bts[1:], 'little', signed=True)))
        print("%dms\t%s%d" % (round((now - last)*1000), chr(bts[0]), int.from_bytes(bts[1:], 'little', signed=True)))
        last = now

def test_measuring_and_recording(ser):
    while True:
        print(int.from_bytes(ser.read(1), 'big', signed=True))

with serial.Serial('/dev/cu.usbserial-10', 9600) as ser:
    #test_measuring_and_printing(ser)
    test_measuring_and_recording(ser)
