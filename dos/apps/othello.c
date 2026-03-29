#include "syscall.h"

unsigned int board[8];
int cx, cy;
int turn = 1; // 1=black 2=white

void print_board() {
  sys_put_string("turn: ", -1);
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

int get_stone(unsigned int *board, int x, int y) {
  return (board[y] >> 2*x) & 3;
}

int try_put_stone(unsigned int *board, int cx, int cy) {
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
      } else if (st == turn) {
        if (prev_st == 3 - turn) {
          rev_cnt += i;
        }
        int putx = x - dx;
        int puty = y - dy;
        for (int j = 0; j < i; ++j) {
          board[puty] = (board[puty] & ~(3 << 2*putx)) | (turn << 2*putx);
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

int buntan_main(int *info) {
  init_syscall(info);

  for (int i = 0; i < 8; ++i) {
    board[i] = 0;
  }
  board[3] = 0x0180;
  board[4] = 0x0240;
  cx = 3;
  cy = 2;

  print_board();

  int c;
  while ((c = sys_getc()) > 0) {
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
        if (try_put_stone(board, cx, cy) > 0) {
          board[cy] |= turn << (2*cx);
          turn = 3 - turn;
        } else {
          sys_put_string("cannot put a stone\n", -1);
        }
      }
    } else {
      continue;
    }

    print_board();
  }
  return 0;
}
