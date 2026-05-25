#!/bin/sh

script_dir=$(dirname $0)
dos_dir=$script_dir/../dos

src=$dos_dir/testram.s
dmem=testram.dmem.hex
pmem=testram.pmem.hex

if [ ! -e $dos_dir/$dmem ] || [ $src -nt $dos_dir/$dmem ]
then
  make -C $dos_dir $dmem
fi

if [ ! -e $dos_dir/$pmem ] || [ $src -nt $dos_dir/$pmem ]
then
  make -C $dos_dir $pmem
fi

cat $dos_dir/$dmem | $script_dir/separate-dmemhex.py $script_dir/ipl.dmem
cp $dos_dir/$pmem $script_dir/ipl.pmem.hex

echo generated following files:
ls -1 $script_dir/ipl*
