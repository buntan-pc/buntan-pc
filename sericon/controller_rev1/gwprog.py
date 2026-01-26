#!/usr/bin/python3

import argparse
import json
import re
import subprocess
import sys

GWPROG = r'C:\Gowin\Gowin_V1.9.12.01_x64\Programmer\bin\programmer_cli.exe'
GWPROG_HEAD = ['cmd.exe', '/C', GWPROG]
CABLE_FILE_PATH = '.gwprog.cable.json'

def perror(*args):
    print(*args, file=sys.stderr)

def program(cable, mode, dev_name, fs_file):
    if mode == 'sram':
        opind = 2
    elif mode == 'flash':
        opind = 6
    else:
        perror('Unknown mode:', mode);
        sys.exit(1)

    res = subprocess.run(
        GWPROG_HEAD + ['--operation_index', str(opind),
                       '--device', dev_name,
                       '--fsFile', fs_file,
                       '--cable', cable['cable'],
                       '--channel', str(cable['ch']),
                       '--location', str(cable['loc'])]
    )
    if res.returncode != 0:
        perror('failed to program')
        sys.exit(1)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('mode', choices=['sram', 'flash'])
    p.add_argument('dev', help='device name such as GW1NR-9C')
    p.add_argument('fs', help='path to a bitstream file')
    args = p.parse_args()

    try:
        with open(CABLE_FILE_PATH) as f:
            cable = json.load(f)
    except FileNotFoundError:
        perror('Please write cable info to', CABLE_FILE_PATH)
        sys.exit(1)
        '''example:
        {
          "cable": "USB Debugger A",
          "ch": 0,
          "loc": 213825
        }
        '''

    program(cable, args.mode, args.dev, args.fs)

if __name__ == '__main__':
    main()
