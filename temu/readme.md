# temu - temu-system-buntan_pc

buntanPCのエミュレーターです。Linux / WSL環境で動かすことを想定しています。
とりあえずDOSが動くくらいまでは完成しています。

## Build
#### 必要なツール
* `mtools`
* `make`
* `gcc`
* `verilator`

あとは[ucc](../ucc)と[uas](../uas)をビルドしておいてください。

#### ビルド手順
1. このディレクトリにcdする
2. `make`を実行する

`make run`, もしくは`out/`に生成される`bemu-system-buntan_pc`を直接実行することで実行が可能です。
