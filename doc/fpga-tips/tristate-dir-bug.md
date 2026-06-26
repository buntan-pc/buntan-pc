# USB デバイス検出が動作しないバグ

## 症状

USB の D+/D- を使って USB デバイスの接続を検出しようとしたが、デバイスを接続しても検出されない。

### 具体的説明

次のようなコードを書いたとき、`onboard_led` が点灯しなかった。（LED は 0 出力で点灯）
想定では下位 2 ビットが USB デバイスのスピードに応じて点灯するはずだった。

```veryl
var usb_dp : 'usb logic; // usb_clk に同期した USB D+ 信号
var usb_dm : 'usb logic; // usb_clk に同期した USB D- 信号

var usb_dir_out : 'usb bit;
var usb_dp_out : 'usb logic;
var usb_dm_out : 'usb logic;
assign usb_dp_raw = if usb_dir_out ? usb_dp_out : 1'bz;
assign usb_dm_raw = if usb_dir_out ? usb_dm_out : 1'bz;

enum USBDevSpeed: bit<2> {
  None,
  LS,
  FS,
}
var usb_dev_speed : 'usb USBDevSpeed;

always_ff (usb_clk, usb_rst_n) {
  if_reset {
    usb_dev_speed = USBDevSpeed::None;
  } else if (usb_dm) {
    usb_dev_speed = USBDevSpeed::LS;
  } else if (usb_dp) {
    usb_dev_speed = USBDevSpeed::FS;
  }
}

unsafe (cdc) {
  assign onboard_led = {if ~usb_rst_n ? 4'b0000 : 4'b1111, ~usb_dev_speed};
}
```

## 原因

`usb_dir_out`（USB ライン送受信の方向制御）が未初期化だった。

```veryl
var usb_dir_out : 'usb bit;  // ドライバなし

assign usb_dp_raw = if usb_dir_out ? usb_dp_out : 1'bz;
assign usb_dm_raw = if usb_dir_out ? usb_dm_out : 1'bz;
```

Gowin の合成ツールが未ドライブの `logic` を 0 に最適化せず、`usb_dir_out = 1` であるかのように動作した。その結果 D+/D- が 0 にドライブされた。`usb_dp` と `usb_dm` が 0 として読み出され、`usb_dev_speed` が `None` から更新されなかった。

## 解決策

`usb_dir_out` を明示的に 0 （受信モード）に設定してから D+/D- を読む。
試験的に `assign` で 0 に固定したところ、LED が光るようになった。

```veryl
assign usb_dir_out = 0;
```

## 教訓

双方向 I/O の方向制御信号は必ず明示的に初期化する。
未ドライブの配線/レジスタが合成ツールによって 0 と扱われるとは限らない。
