#!/bin/sh

script_dir=$(dirname $0)
dos_dir=$script_dir/../dos

dosc=$dos_dir/dos.c
dmem=$dos_dir/dos.dmem.hex
pmem=$dos_dir/dos.pmem.hex

if [ $dosc -nt $dmem ] || [ $dosc -nt $pmem ]
then
  echo "You may need to rebuild DOS. 'dos.c' is newer than hex files."
fi

cat $dos_dir/dos.dmem.hex | $script_dir/separate-dmemhex.py $script_dir/ipl.dmem
cp $dos_dir/dos.pmem.hex $script_dir/ipl.pmem.hex

echo generated following files:
ls -1 $script_dir/ipl*
