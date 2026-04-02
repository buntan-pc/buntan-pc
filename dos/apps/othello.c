#include "syscall.h"
#include "mmio.h"

unsigned int board[8];
int cx;
int cy;
int turn = 1; // 1=black 2=white
int ai_turn = 2;
int ai_lastx = -1;
int ai_lasty = -1;

void print_board_init() {
  sys_put_string("\x1B[H", -1);  // カーソルを左上へ
  sys_put_string("ai_turn: ", -1);
  sys_put_string("*o" + (ai_turn - 1), 1);
  sys_put_string("\n", 1);
  sys_put_string("O/@: AI's last move\n\n", -1);

  sys_put_string("current turn: ", -1);
  sys_put_string("*o" + (turn - 1), 1);
  sys_put_string("\n", 1);

  for (int y = 0; y < 8; ++y) {
    unsigned int line = board[y];
    for (int x = 0; x < 8; ++x) {
      if (cy == y && cx == x) {
        sys_put_string(">", 1);
      } else {
        sys_put_string(" ", 1);
      }
      sys_put_string("_*o?" + (line & 3), 1);
      line = line >> 2;
    }
    sys_put_string("\n", 1);
  }
}

void print_board() {
  sys_put_string("\x1B[4;15H", -1);  // カーソルを turn: の次へ
  sys_put_string("*o" + (turn - 1), 1);
  sys_put_string("\x1B[E", -1); // カーソルを次の行の左端へ

  for (int y = 0; y < 8; ++y) {
    unsigned int line = board[y];
    for (int x = 0; x < 8; ++x) {
      if (cy == y && cx == x) {
        sys_put_string(">", 1);
      } else {
        sys_put_string(" ", 1);
      }
      int st;
      if (ai_lasty == y && ai_lastx == x) {
        if (ai_turn == 1) {
          st = '@';
        } else {
          st = 'O';
        }
      } else {
        st = "_*o?"[line & 3];
      }
      sys_put_string(&st, 1);
      line = line >> 2;
    }
    sys_put_string("\x1B[E", -1); // カーソルを次の行の左端へ
  }
}

int get_stone(unsigned int *board, int x, int y) {
  return (board[y] >> 2*x) & 3;
}

int try_put_stone(unsigned int *board, int cx, int cy, int stone) {
  if (get_stone(board, cx, cy) != 0) {
    return -1;
  }

  int rev_cnt = 0;
  for (int dir = 0; dir < 8; ++dir) {
    int dx = 0;
    if (dir <= 1 || dir == 7) {
      dx = 1;
    } else if (3 <= dir && dir <= 5) {
      dx = -1;
    }
    int dy = 0;
    if (1 <= dir && dir <= 3) {
      dy = -1;
    } else if (dir >= 5) {
      dy = 1;
    }

    int x = cx + dx;
    int y = cy + dy;
    int prev_st = get_stone(board, x, y);
    if (prev_st == 0) {
      continue;
    }

    x += dx;
    y += dy;

    int i = 1;

    while (0 <= x && x <= 7 && 0 <= y && y <= 7) {
      int st = get_stone(board, x, y);
      if (st == 0) {
        break;
      } else if (st == stone) {
        if (prev_st == 3 - stone) {
          rev_cnt += i;
        }
        int putx = x - dx;
        int puty = y - dy;
        for (int j = 0; j < i; ++j) {
          board[puty] = (board[puty] & ~(3 << 2*putx)) | (stone << 2*putx);
          putx = putx - dx;
          puty = puty - dy;
        }
        break;
      }
      x += dx;
      y += dy;
      ++i;
    }
  }

  return rev_cnt;
}

// (x, y) に打った場合の評価値を計算
// depth: 探索深さ。あとどれだけ深く探索するか。
//        0なら、その手を打った際の評価値をそのまま返す。
int eval_move(unsigned int *board, int x, int y, int stone, int depth) {
  if (depth <= 0) {
    int board_ai[8];
    for (int i = 0; i < 8; ++i) {
      board_ai[i] = board[i];
    }
    int rev_cnt = try_put_stone(board_ai, x, y, stone);
    // とりあえず裏返せる数が一番多いときに評価値を最大とする
    return rev_cnt;
  } else { // depth > 0
    int board_ai[8];
    for (int i = 0; i < 8; ++i) {
      board_ai[i] = board[i];
    }
    int rev_cnt = try_put_stone(board_ai, x, y, stone);
  }
}

int buntan_main(int *info) {
  init_syscall(info);

  for (int i = 0; i < 8; ++i) {
    board[i] = 0;
  }
  board[3] = 0x0180;
  board[4] = 0x0240;
  cx = 3;
  cy = 2;

  sys_put_string("\x1b[?1049h", -1); // 代替バッファへ切り替え
  print_board_init();

  while (1) {
    print_board();
    if (turn == ai_turn) {
      timer_cnt = 10000; // 思考時間を計るためのタイマ初期値

      unsigned int board_ai[8];
      int max_rev_cnt = 0;
      int max_x;
      int max_y;

      for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
          for (int i = 0; i < 8; ++i) {
            board_ai[i] = board[i];
          }
          int rev_cnt = try_put_stone(board_ai, x, y, turn);
          if (max_rev_cnt < rev_cnt) {
            max_rev_cnt = rev_cnt;
            max_x = x;
            max_y = y;
          }
        }
      }

      ai_lastx = max_x;
      ai_lasty = max_y;

      try_put_stone(board, ai_lastx, ai_lasty, turn);
      board[ai_lasty] |= ai_turn << 2*ai_lastx;
      turn = 3 - turn;

      unsigned int ai_time = 10000 - timer_cnt;
      char s[8];
      sys_int2dec(ai_time, s, 5);
      sys_put_string("AI's think time: ", -1);
      sys_put_string(s, 5);
      sys_put_string("ms\n", -1);
    } else {
      int c = sys_getc();
      if (c <= 0) {
        break;
      }

      // カーソルから右側を消去（ステータスラインを消去）
      sys_put_string("\x1B[K", -1);

      if (cx > 0 && (c == 'h' || c == 0x1F)) {
        --cx;
      } else if (cy < 7 && (c == 'j' || c == 0x1D)) {
        ++cy;
      } else if (cy > 0 && (c == 'k' || c == 0x1C)) {
        --cy;
      } else if (cx < 7 && (c == 'l' || c == 0x1E)) {
        ++cx;
      } else if (c == ' ') {
        if (get_stone(board, cx, cy) != 0) {
          sys_put_string("cannot put a stone\n", -1);
          continue;
        } else {
          if (try_put_stone(board, cx, cy, turn) > 0) {
            board[cy] |= turn << (2*cx);
            turn = 3 - turn;
          } else {
            sys_put_string("cannot put a stone\n", -1);
          }
        }
      } else if (c == 'q') {
        sys_put_string("Quitting. Please press a key...\n", -1);
        sys_getc();
        break;
      } else {
        continue;
      }
    }
  }

  sys_put_string("\x1b[?1049l", -1); // メインバッファへ戻す
  return 0;
}
