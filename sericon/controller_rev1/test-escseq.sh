#!/bin/sh -eux

script_dir=$(dirname $0)
scripts_dir=$script_dir/../../scripts
uartpy=$scripts_dir/uart.py
sendhex="$uartpy --baudrate 9600 --nowait"

# CSI 2 r  スクロール範囲
$sendhex 1B 5B 32 72

# CSI 999 H  カーソルを最下行に
$sendhex 1B 5B 39 39 39 48

# DEAD
$sendhex 44 45 41 44

# ESC 7  カーソル位置を保存 DECSC
$sendhex 1B 37

# CSI 1;1 H  カーソルを左上に
$sendhex 1B 5B 31 3B 31 48

# @^
$sendhex 40 5E

# ESC 7  カーソル位置を復帰 DECRC
$sendhex 1B 38

# LF BEEF
$sendhex 0A 42 45 45 46

# CSI ? 1049 h  DECSET XT_EXTSCRN 代替バッファに切り替え
$sendhex 1B 5B 3F 31 30 34 39 68

# CSI 1;1 H
$sendhex 1B 5B 31 3B 31 48

# CAFE LF BABE
$sendhex 43 41 46 45 0A 42 41 42 45

sleep 2

# CSI ? 1049 l  DECRST XT_EXTSCRN 標準バッファに切り替え
$sendhex 1B 5B 3F 31 30 34 39 6C

# LF std buffer
$sendhex 0A 73 74 64 20 62 75 66 66 65 72

sleep 1

# OSC 10 ; rgb:FF/00/00 BEL
$sendhex 1B 5D 31 30 3B 72 67 62 3A 46 46 2F 30 30 2F 30 30 07

sleep 1

# OSC 10 ; rgb:00/00/00 BEL
$sendhex 1B 5D 31 30 3B 72 67 62 3A 30 30 2F 30 30 2F 30 30 07

# OSC 11 ; rgb:FF/FF/FF BEL
$sendhex 1B 5D 31 31 3B 72 67 62 3A 46 46 2F 46 46 2F 46 46 07

sleep 1

# OSC 10 ; reset BEL
$sendhex 1B 5D 31 30 3B 72 65 73 65 74 07

sleep 1

# OSC 11 ; reset BEL
$sendhex 1B 5D 31 31 3B 72 65 73 65 74 07
