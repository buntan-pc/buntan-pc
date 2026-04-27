#!/bin/sh

script_dir=$(dirname $0)
dos_dir=$script_dir/../dos

dosc=$dos_dir/dos.c
dmem=$dos_dir/dos.dmem.hex
pmem=$dos_dir/dos.pmem.hex

if [ ! -e $dmem ] || [ $dosc -nt $dmem ]
then
  make -C $dos_dir dos.dmem.hex
fi

if [ ! -e $pmem ] || [ $dosc -nt $pmem ]
then
  make -C $dos_dir dos.pmem.hex
fi

cat $dos_dir/dos.dmem.hex | $script_dir/separate-dmemhex.py $script_dir/ipl.dmem
cp $dos_dir/dos.pmem.hex $script_dir/ipl.pmem.hex

echo generated following files:
ls -1 $script_dir/ipl*
