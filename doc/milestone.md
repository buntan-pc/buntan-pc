# プロジェクトの中間目標

## 2/21 OSC2025 Tokyo/Spring

2/21、22 に開催される [OSC2025 Tokyo/Spring](https://event.ospn.jp/osc2025-spring/) に展示したい。

最低限、次の状態を達成したい！

- BuntanPC 上で何らかの「テキストエディタ」が動き、ファイルを SD カードに保存できる。
  - UART で入出力する前提なので、GUI ではなく CUI あるいは TUI。
  - 最低限は、コマンドラインで 1 行だけ書き込めるもの。
    - 例えば `writestr file.txt foobar hogera` とすると「foobar hogera」とファイルに書かれるとか。
  - Vi のように画面があり、カーソルが動きまわるエディタが作れたら最高。
    - スクロールできない（スクロールせず扱える行数しか扱えない）という仕様もアリ。

可能なら、次の状態も達成したい。

- HEX ファイルを読み取って pmem へ転送し、実行できる。
  - 上述のテキストエディタと組み合わせると、BuntanPC 上でプログラミングできる。
  - ただしハンドアセンブル必須。
- 音楽ファイルを読み取ってスピーカで再生できる。
  - MML のような音楽情報が書かれたファイルを想定。
  - GPIO に接続された圧電スピーカを用いて音楽を再生する。
