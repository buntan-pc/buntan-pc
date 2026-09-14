# CSI 2 r  スクロール範囲
../../scripts/uart.py --baudrate 9600 --nowait 1B 5B 32 72

# CSI 999 H  カーソルを最下行に
../../scripts/uart.py --baudrate 9600 --nowait 1B 5B 39 39 39 48

# DEAD
../../scripts/uart.py --baudrate 9600 --nowait 44 45 41 44

# LF BEEF
../../scripts/uart.py --baudrate 9600 --nowait 0A 42 45 45 46

# ESC 7  カーソル位置を保存 DECSC
../../scripts/uart.py --baudrate 9600 --nowait 1B 37

# CSI 1;1 H  カーソルを左上に
../../scripts/uart.py --baudrate 9600 --nowait 1B 5B 31 3B 31 48

# @^
../../scripts/uart.py --baudrate 9600 --nowait 40 5E

# ESC 7  カーソル位置を復帰 DECRC
../../scripts/uart.py --baudrate 9600 --nowait 1B 38
