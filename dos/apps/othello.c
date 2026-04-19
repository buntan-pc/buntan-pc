#include "syscall.h"
#include "mmio.h"

//#include <stdio.h>

unsigned int board[8];
unsigned int board_prev[8];
int cx;
int cy;
int turn = 0; // 0=black 1=none 2=white
int ai_turn = 2;
int ai_lastx = -1;
int ai_lasty = -1;

void print_board_init() {
  sys_put_string("\x1B[H", -1);  // カーソルを左上へ
  sys_put_string("ai_turn: ", -1);
  sys_put_string("*?o" + ai_turn, 1);
  sys_put_string("\n", 1);
  sys_put_string("O/@: AI's last move\n\n", -1);

  sys_put_string("current turn: ", -1);
  sys_put_string("*?o" + turn, 1);
  sys_put_string("\n", 1);

  char line_name[2];
  sys_put_string("   A B C D E F G H\n", -1);
  for (int y = 0; y < 8; ++y) {
    unsigned int line = board[y];
    line_name[0] = '1' + y;
    line_name[1] = ' ';
    sys_put_string(line_name, 2);
    for (int x = 0; x < 8; ++x) {
      if (cy == y && cx == x) {
        sys_put_string(">", 1);
      } else {
        sys_put_string(" ", 1);
      }
      sys_put_string("*_o?" + (line & 3), 1);
      line = line >> 2;
    }
    sys_put_string("\n", 1);
  }
}

void print_board(unsigned int *board) {
  sys_put_string("\x1B[4;15H", -1);  // カーソルを turn: の次へ
  sys_put_string("*?o" + turn, 1);
  sys_put_string("\x1B[E", -1); // カーソルを次の行の左端へ

  char line_name[2];
  sys_put_string("   A B C D E F G H\n", -1);
  for (int y = 0; y < 8; ++y) {
    unsigned int line = board[y];
    line_name[0] = '1' + y;
    line_name[1] = ' ';
    sys_put_string(line_name, 2);
    for (int x = 0; x < 8; ++x) {
      if (cy == y && cx == x) {
        sys_put_string(">", 1);
      } else {
        sys_put_string(" ", 1);
      }
      int st = line & 3;
      if (st != 1 && ai_lasty == y && ai_lastx == x) {
        if (ai_turn == 0) {
          st = '@';
        } else {
          st = 'O';
        }
      } else {
        st = "*_o?"[st];
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

void put_stone(unsigned int *board, int x, int y, int stone) {
  board[y] = (board[y] & ~(3 << 2*x)) | (stone << 2*x);
}

int try_put_stone(unsigned int *board, int cx, int cy, int stone) {
  if (get_stone(board, cx, cy) != 1) {
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
    if (prev_st == 1) {
      continue;
    }

    x += dx;
    y += dy;

    int i = 1;

    while (0 <= x && x <= 7 && 0 <= y && y <= 7) {
      int st = get_stone(board, x, y);
      if (st == 1) {
        break;
      } else if (st == stone) {
        if (prev_st == 2 - stone) {
          rev_cnt += i;
        }
        int putx = x - dx;
        int puty = y - dy;
        for (int j = 0; j < i; ++j) {
          put_stone(board, putx, puty, stone);
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

  if (rev_cnt > 0) {
    put_stone(board, cx, cy, stone);
  }

  return rev_cnt;
}

// もし AI が黒番 => return black=+1, white=-1
// もし AI が白番 => return black=-1, white=+1
int st2ev(int stone) {
  return (ai_turn - 1) * (stone - 1);
}

// 黒番から見た盤面評価値を計算
int eval_board(unsigned int *board) {
  int eval = 0;

  // 四隅
  eval += st2ev(get_stone(board, 0, 0)) * 3;
  eval += st2ev(get_stone(board, 7, 0)) * 3;
  eval += st2ev(get_stone(board, 0, 7)) * 3;
  eval += st2ev(get_stone(board, 7, 7)) * 3;

  // 辺
  for (int i = 1; i < 7; ++i) {
    eval += st2ev(get_stone(board, i, 0)) * 2;
    eval += st2ev(get_stone(board, i, 7)) * 2;
    eval += st2ev(get_stone(board, 0, i)) * 2;
    eval += st2ev(get_stone(board, 7, i)) * 2;
  }

  // その他
  for (int y = 1; y < 7; ++y) {
    for (int x = 1; x < 7; ++x) {
      eval += st2ev(get_stone(board, x, y));
    }
  }

  return eval;
}

// (x, y) に打った場合の評価値を計算
// depth: 探索深さ。あとどれだけ深く探索するか。
//        0なら、その手を打った際の評価値をそのまま返す。
int eval_move(unsigned int *board, int x, int y, int stone, int depth) {
  //fprintf(stderr, "eval_move: %d,%d stone=%d depth=%d\n", x, y, stone, depth);

  if (depth <= 0) {
    int board_next[8];
    for (int i = 0; i < 8; ++i) {
      board_next[i] = board[i];
    }
    int rev_cnt = try_put_stone(board_next, x, y, stone);
    if (rev_cnt <= 0) {
      return -30000;
    }
    int ev = eval_board(board_next);
    //fprintf(stderr, "eval_move(%d,%d,stone=%d): eval_board -> %d\n", x, y, stone, ev);
    return ev;
  } else { // depth > 0
    int board_next[8];
    for (int i = 0; i < 8; ++i) {
      board_next[i] = board[i];
    }
    int rev_cnt = try_put_stone(board_next, x, y, stone);
    if (rev_cnt <= 0) {
      // ここは石を置けない場所だった
      return -30000;
    }

    int board_nn[8];

    // 相手のターン
    int max_ev = -30000;
    int min_ev = 30000;
    for (int oy = 0; oy < 8; ++oy) {
      for (int ox = 0; ox < 8; ++ox) {
        for (int i = 0; i < 8; ++i) {
          board_nn[i] = board_next[i];
        }
        int ev = eval_move(board_nn, ox, oy, 2 - stone, depth - 1);
        if (ev == -30000) {
          continue;
        }
        if (max_ev < ev) {
          max_ev = ev;
        }
        if (min_ev > ev) {
          min_ev = ev;
        }
      }
    }

    if (stone == ai_turn && ai_turn == 0) {
      // AI の手番の評価、かつ AI が黒石
      // 相手=白 は評価値が最低になるように打つと考える
      return min_ev;
    } else {
      return max_ev;
    }
  }
}

void print_kifu(unsigned char *kifu, unsigned int len) {
  char s[2];
  for (int i = 0; i < len; ++i) {
    s[0] = 'a' + (kifu[i] >> 4);
    s[1] = '0' + (kifu[i] & 0x0f);
    sys_put_string(s, 2);
  }
}

int buntan_main(int *info) {
  char kifu[60];
  unsigned int kifu_len = 0;

  init_syscall(info);
  //__builtin_reset_sr(0);

  turn = 0;
  ai_turn = 2;
  ai_lastx = -1;
  ai_lasty = -1;

  int argc = info[2];
  char **argv = info[3];
  if (argc >= 2) {
    int ai_init = argv[1][0];
    if (ai_init == 'b') {
      ai_turn = 0;
    } else if (ai_init == 'w') {
      ai_turn = 2;
    }
  }

  for (int i = 0; i < 8; ++i) {
    board[i] = 0x5555;
  }
  board[3] = 0x5495;
  board[4] = 0x5615;
  cx = 3;
  cy = 2;

  sys_put_string("\x1b[?1049h", -1); // 代替バッファへ切り替え
  print_board_init();

  while (1) {
    print_board(board);
    if (turn == ai_turn) {
      timer_cnt = 10000; // 思考時間を計るためのタイマ初期値

      unsigned int board_ai[8];
      int max_ev = -30000;
      int max_x;
      int max_y;

      for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
          for (int i = 0; i < 8; ++i) {
            board_ai[i] = board[i];
          }
          int st = get_stone(board_ai, x, y);
          int ev = eval_move(board_ai, x, y, turn, 1);
          if (st != 1 && ev != -30000) {
            //fprintf(stderr, "err\n");
          }
          if (max_ev < ev) {
            //fprintf(stderr, "eval_move (max renewed): %d,%d -> %d\n", x, y, ev);
            max_ev = ev;
            max_x = x;
            max_y = y;
          }
        }
      }

      ai_lastx = max_x;
      ai_lasty = max_y;

      for (int i = 0; i < 8; ++i) {
        board_prev[i] = board[i];
      }

      try_put_stone(board, ai_lastx, ai_lasty, turn);
      kifu[kifu_len++] = (ai_lastx << 4) | ai_lasty;
      turn = 2 - turn;

      char s[8];
      sys_put_string("AI's move: ", -1);
      s[0] = 'A' + ai_lastx;
      s[1] = '1' + ai_lasty;
      sys_put_string(s, 2);
      sys_put_string(" (x=", -1);
      s[0] = '0' + ai_lastx;
      s[1] = '0' + ai_lasty;
      sys_put_string(s + 0, 1);
      sys_put_string(",y=", -1);
      sys_put_string(s + 1, 1);
      sys_put_string(")\n", -1);

      unsigned int ai_time = 10000 - timer_cnt;
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
      } else if (c == 'p') {
        print_board(board_prev);
        sys_put_string("press any key to continue", -1);
        sys_getc();
        sys_put_string("\x1B[1G\x1B[K", -1);
      } else if (c == 'r') {
        print_kifu(kifu, kifu_len);
      } else if (c == ' ') {
        if (get_stone(board, cx, cy) != 1) {
          sys_put_string("cannot put a stone\n", -1);
          continue;
        } else {
          if (try_put_stone(board, cx, cy, turn) > 0) {
            kifu[kifu_len++] = (cx << 4) | cy;
            turn = 2 - turn;
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

  sys_put_string("kifu:\n", -1);
  print_kifu(kifu, kifu_len);
  sys_put_string("\n", 1);

  //unsigned int fpmin = __builtin_get_sr(0);
  //char s[4];
  //sys_int2hex(fpmin, s, 4);
  //sys_put_string("FPMIN=", -1);
  //sys_put_string(s, 4);
  //sys_put_string("\n", 1);
  return 0;
}
