// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 tas0dev
 */

#include "spi.h"

#include <stdio.h>

#include "debug.h"

#define RESP_QUEUE_SIZE 128
#define CMD_BUF_SIZE 6

typedef struct sd_slave {
  uint8_t cmd_buf[CMD_BUF_SIZE];
  int cmd_idx;
  uint8_t resp_queue[RESP_QUEUE_SIZE];
  int resp_head;
  int resp_tail;
  int last_cmd_was_cmd55;
} sd_slave_t;

static sd_slave_t g_sd;

static void sd_reset(sd_slave_t* s) {
  if (!s) return;
  s->cmd_idx = 0;
  s->resp_head = s->resp_tail = 0;
  s->last_cmd_was_cmd55 = 0;
}

static int resp_empty(const sd_slave_t* s) {
  return s->resp_head == s->resp_tail;
}

static void resp_enqueue(sd_slave_t* s, uint8_t v) {
  int next = (s->resp_tail + 1) % RESP_QUEUE_SIZE;
  if (next == s->resp_head) {
    s->resp_head = (s->resp_head + 1) % RESP_QUEUE_SIZE;
  }
  s->resp_queue[s->resp_tail] = v;
  s->resp_tail = next;
}

static int resp_dequeue(sd_slave_t* s, uint8_t* out) {
  if (resp_empty(s)) return 0;
  *out = s->resp_queue[s->resp_head];
  s->resp_head = (s->resp_head + 1) % RESP_QUEUE_SIZE;
  return 1;
}

static void sd_process_cmd(sd_slave_t* s) {
  uint8_t cmd = s->cmd_buf[0] & 0x3f;
  uint32_t arg = (s->cmd_buf[1] << 24) | (s->cmd_buf[2] << 16) |
                 (s->cmd_buf[3] << 8) | s->cmd_buf[4];
  debug("SD: CMD%u 引数=0x%08x\n", cmd, arg);
  fflush(stdout);
  switch (cmd) {
    case 0:                   // CMD0: Idle 状態へ
      resp_enqueue(s, 0x01);  // R1: Idle
      s->last_cmd_was_cmd55 = 0;
      break;
    case 8:                   // CMD8: バージョン確認（エコー応答）
      resp_enqueue(s, 0x01);  // R1
      resp_enqueue(s, 0x00);
      resp_enqueue(s, 0x00);
      resp_enqueue(s, 0x01);
      resp_enqueue(s, 0xAA);
      s->last_cmd_was_cmd55 = 0;
      break;
    case 55:  // CMD55: アプリコマンドプレフィックス
      resp_enqueue(s, 0x01);
      s->last_cmd_was_cmd55 = 1;
      break;
    case 41:  // ACMD41 (CMD55の後に送られる場合は初期化完了)
      if (s->last_cmd_was_cmd55) {
        // Ready を返す
        resp_enqueue(s, 0x00);
      } else {
        // アプリコマンドではない
        resp_enqueue(s, 0x01);
      }
      s->last_cmd_was_cmd55 = 0;
      break;
    case 58:
      resp_enqueue(s, 0x00);
      resp_enqueue(s, 0xC0);
      resp_enqueue(s, 0x00);
      resp_enqueue(s, 0x00);
      resp_enqueue(s, 0x00);
      s->last_cmd_was_cmd55 = 0;
      break;
    default:
      // Unknown
      resp_enqueue(s, 0x04);
      s->last_cmd_was_cmd55 = 0;
      break;
  }
}

static uint16_t spi_status(const bemu_spi_t* spi) {
  uint16_t v = 0;
  v |= (spi->tx_ready ? 1u : 0u) << 0;
  v |= (spi->cs ? 1u : 0u) << 1;
  return v;
}

void bemu_spi_init(bemu_spi_t* spi) {
  if (!spi) return;
  spi->cs = 1;
  spi->tx_ready = 1;
  spi->pending_tx = 0;
  spi->last_tx = 0xFF;
  sd_reset(&g_sd);
}

void bemu_spi_reset(bemu_spi_t* spi) { bemu_spi_init(spi); }

void bemu_spi_tick(bemu_spi_t* spi) {
  if (!spi) return;
  if (spi->pending_tx) {
    spi->pending_tx = 0;
    spi->tx_ready = 1;
  }
}

uint16_t bemu_spi_read16(bemu_spi_t* spi, uint16_t addr) {
  if (!spi) return 0;
  switch (addr & 0xfffeu) {
    case 0x0020:
      return (uint16_t)spi->last_tx;
    case 0x0022:
      return spi_status(spi);
    default:
      return 0;
  }
}

void bemu_spi_write16(bemu_spi_t* spi, uint16_t addr, uint16_t value) {
  if (!spi) return;
  switch (addr & 0xfffeu) {
    case 0x0020: {
      uint8_t mosi = (uint8_t)(value & 0xffu);
      if (spi->cs) {
        spi->last_tx = 0xFF;
        spi->tx_ready = 0;
        spi->pending_tx = 1;
        g_sd.cmd_idx = 0;
        return;
      }

      if (g_sd.cmd_idx == 0) {
        if ((mosi & 0x40) == 0x40) {
          g_sd.cmd_buf[0] = mosi;
          g_sd.cmd_idx = 1;
        } else {
          uint8_t out = 0xFF;
          if (!resp_dequeue(&g_sd, &out)) out = 0xFF;
          spi->last_tx = out;
          spi->tx_ready = 0;
          spi->pending_tx = 1;
          return;
        }
      } else {
        g_sd.cmd_buf[g_sd.cmd_idx++] = mosi;
      }

      if (g_sd.cmd_idx >= CMD_BUF_SIZE) {
        sd_process_cmd(&g_sd);
        g_sd.cmd_idx = 0;
      }

      uint8_t out = 0xFF;
      if (!resp_dequeue(&g_sd, &out)) out = 0xFF;
      spi->last_tx = out;
      spi->tx_ready = 0;
      spi->pending_tx = 1;
      break;
    }
    case 0x0022:
      spi->cs = (uint8_t)((value >> 1) & 1u);
      if (spi->cs) {
        sd_reset(&g_sd);
      }
      break;
    default:
      break;
  }
}
