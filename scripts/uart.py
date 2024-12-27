#!/usr/bin/python3

import argparse
import os.path
import serial
from serial.tools.list_ports import comports
import sys
import time


def hex_to_bytes_packed(hex_list):
    '''
    4 命令を 9 バイトで送る方式。
    18bits x 4 = 72bits = 9bytes
    '''
    return b''


def hex_to_bytes(hex_list, mode=None):
    if mode == 'program_packed':
        return hex_to_bytes_packed(hex_list)

    conv = lambda i: i.to_bytes(1, 'big')
    if mode == 'program_simple':
        conv = lambda i: i.to_bytes(3, 'big')
    if mode == 'little_2bytes':
        conv = lambda i: i.to_bytes(2, 'big')
    return b''.join(conv(i) for i in hex_list)


def receive_stdout(filename, ser):
    num = 0
    with open(filename, 'wb') as f:
        b = ser.read(1)
        while b != b'\x04':
            num += 1
            f.write(b)
            f.flush()
            b = ser.read(1)
    return (num, b)


def send_program_and_data(ser, pmem_list, dmem_list, mode):
    ser.write(b'\x55')
    ser.flush()
    time.sleep(0.005)
    ser.write(b'\xAA')
    ser.flush()

    pmem_size = len(pmem_list)
    dmem_size = len(dmem_list) * 2

    mode_flag = 0x4000 if mode == 'program_packed' else 0x0000
    ser.write((pmem_size | mode_flag).to_bytes(length=2, byteorder='big'))
    ser.write(dmem_size.to_bytes(length=2, byteorder='big'))

    ser.write(hex_to_bytes(pmem_list, mode))
    ser.write(hex_to_bytes(dmem_list, 'little_2bytes'))
    ser.flush()


def main():
    devs = comports()
    if devs:
        default_dev = devs[0].device
    else:
        default_dev = '/dev/ttyUSB0'

    p = argparse.ArgumentParser()
    p.add_argument('--dev', default=default_dev,
                   help='path to a serial device')
    p.add_argument('--timeout', type=int, default=3,
                   help='time to wait for result')
    p.add_argument('--nowait', action='store_true',
                   help='do not wait for result/stdout')
    p.add_argument('--stdout',
                   help='path to a file to hold bytes received')
    p.add_argument('--pmem',
                   help='program transfer mode with given program memory hex file')
    p.add_argument('--dmem',
                   help='data memory hex file (used with --pmem)')
    p.add_argument('hex', nargs='*',
                   help='list of hex values to send')

    args = p.parse_args()

    if args.dev[0] == '/' and not os.path.exists(args.dev):
        print('No such device: ' + args.dev, file=sys.stderr)
        print("You may need 'sudo modprobe ftdi_sio'")
        sys.exit(1)

    ser = serial.Serial(args.dev, 115200, timeout=args.timeout,
                        bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
                        stopbits=serial.STOPBITS_ONE)

    if args.pmem:
        with open(args.pmem) as f:
            file_data = f.read()
        pmem_list = [int(c, 16) for c in file_data.split()]

        dmem_list = []
        if args.dmem:
            with open(args.dmem) as f:
                file_data = f.read()
            dmem_list = [int(c, 16) for c in file_data.split()]

        send_program_and_data(ser, pmem_list, dmem_list, 'program_simple')

    if args.hex:
        hex_list = [int(c, 16) for c in args.hex]
        ser.write(hex_to_bytes(hex_list))
        ser.flush()

    if not args.nowait:
        if args.stdout:
            recv_len, last_byte = receive_stdout(args.stdout, ser)
            info = 'with EOT' if last_byte == b'\x04' else 'no EOT'
            print(f"{recv_len} bytes ({info}) received and saved to '{args.stdout}'")
        else:
            read_bytes = ser.read(1)
            if len(read_bytes) == 0: # timeout
                print('timeout')
            else:
                print(' '.join('{:02x}'.format(b) for b in read_bytes))

    ser.close()


if __name__ == '__main__':
    main()
