#!/bin/sh -eux

script_dir=$(dirname $0)
scripts_dir=$script_dir/../../scripts
uartpy=$scripts_dir/uart.py

# CSI 2 r  スクロール範囲
$uartpy --baudrate 9600 --nowait 1B 5B 32 72

# CSI 999 H  カーソルを最下行に
$uartpy --baudrate 9600 --nowait 1B 5B 39 39 39 48

# DEAD
$uartpy --baudrate 9600 --nowait 44 45 41 44

# ESC 7  カーソル位置を保存 DECSC
$uartpy --baudrate 9600 --nowait 1B 37

# CSI 1;1 H  カーソルを左上に
$uartpy --baudrate 9600 --nowait 1B 5B 31 3B 31 48

# @^
$uartpy --baudrate 9600 --nowait 40 5E

# ESC 7  カーソル位置を復帰 DECRC
$uartpy --baudrate 9600 --nowait 1B 38

# LF BEEF
$uartpy --baudrate 9600 --nowait 0A 42 45 45 46
