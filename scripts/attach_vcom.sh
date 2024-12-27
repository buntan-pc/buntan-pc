#!/bin/sh

attach_dev() {
  cmd.exe /C usbipd attach --busid $1 --wsl
}

if [ $# -eq 1 ]
then
  echo Attaching a specified device: BUS_ID = $1
  attach_dev $1
  exit 0
fi

vcom_line=$(cd /mnt/c; cmd.exe /C usbipd list | nkf | grep "USB Serial Converter")

if [ "$vcom_line" = "" ]
then
  echo Serial device not found.
  exit 0
else
  echo Showing serial devices:
  echo "$vcom_line"
  echo "To attach a device: '$0 BUS_ID'"
  exit 0
fi
