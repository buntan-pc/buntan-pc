#!/bin/sh -u
if [ $# -ne 1 ]
then
  echo "Usage: $0 <exe file>"
  exit 1
fi

exe="$1"
if [ ! -f "$exe" ]
then
  exe="$exe.exe"
fi

if [ ! -f "$exe" ]
then
  echo "No such file: $exe"
  exit 1
fi

../../scripts/uart.py --nodelim --nowait --exe "$exe"
