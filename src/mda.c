/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:     mda.c                                                      *
 * Created:       2003-04-13 by Hampa Hug <hampa@hampa.ch>                   *
 * Last modified: 2003-04-14 by Hampa Hug <hampa@hampa.ch>                   *
 * Copyright:     (C) 2003 by Hampa Hug <hampa@hampa.ch>                     *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

/* $Id: mda.c,v 1.1 2003/04/15 04:03:56 hampa Exp $ */


#include <stdio.h>

#include <pce.h>


mda_t *mda_new (void)
{
  unsigned i;
  mda_t    *mda;

  mda = (mda_t *) malloc (sizeof (mda_t));
  if (mda == NULL) {
    return (NULL);
  }

  for (i = 0; i < 16; i++) {
    mda->crtc_reg[i] = 0;
  }

  mda->cur_pos = 0;

  mda->mem = mem_blk_new (0xb0000, 16384, 1);
  mda->mem->ext = mda;
  mda->mem->set_uint8 = &mda_mem_set_uint8;
  mda->mem->set_uint16 = &mda_mem_set_uint16;

  mda->crtc = mem_blk_new (0x3b4, 16, 1);
  mda->crtc->ext = mda;
  mda->crtc->set_uint8 = &mda_crtc_set_uint8;
  mda->crtc->set_uint16 = &mda_crtc_set_uint16;
  mda->crtc->get_uint8 = &mda_crtc_get_uint8;

  return (mda);
}

void mda_del (mda_t *mda)
{
  if (mda != NULL) {
    mem_blk_del (mda->mem);
    mem_blk_del (mda->crtc);
    free (mda);
  }
}

void mda_clock (mda_t *mda)
{
}

void mda_set_pos (mda_t *mda, unsigned pos)
{
  unsigned x, y;

  if (mda->cur_pos == pos) {
    return;
  }

  mda->cur_pos = pos;

  x = pos % 80;
  y = pos / 80;

  fprintf (mda->fp, "\x1b[%u;%uH", y + 1, x + 1);

  fflush (mda->fp);
}

void mda_print (mda_t *mda, unsigned x, unsigned y, unsigned char c, unsigned char a)
{
  unsigned it, fg, bg;

  fprintf (mda->fp, "\x1b[%u;%uH", y + 1, x + 1);

  it = (a & 0x08) ? 1 : 0;
  fg = (a & 0x07) ? 7 : 0;
  bg = (a & 0x70) ? 7 : 0;

  fprintf (mda->fp, "\x1b[%u;%u;%um", it, 30 + fg, 40 + bg);

  if ((c >= 32) && (c < 128)) {
    fputc (c, mda->fp);
  }
  else if (c == 0) {
    fputc (' ', mda->fp);
  }
  else {
    fputc ('.', mda->fp);
  }

  fflush (mda->fp);
}

void mda_mem_set_uint8 (void *obj, unsigned long addr, unsigned char val)
{
  unsigned      x, y;
  unsigned char c, a;
  mda_t         *mda;

  mem_blk_t *mem;

  mem = (mem_blk_t *) obj;

  if (mem->data[addr] == val) {
    return;
  }

  mem->data[addr] = val;

  if (addr >= 4000) {
    return;
  }

  x = (addr >> 1) % 80;
  y = (addr >> 1) / 80;

  if (addr & 1) {
    c = mem->data[addr - 1];
    a = val;
  }
  else {
    c = val;
    a = mem->data[addr + 1];
  }

  mda = (mda_t *) mem->ext;

  mda_print (mda, x, y, c, a);
}

void mda_mem_set_uint16 (void *obj, unsigned long addr, unsigned short val)
{
  mem_blk_t *mem;

  mem = (mem_blk_t *) obj;

  mda_mem_set_uint8 (obj, addr, val & 0xff);

  if (addr < mem->end) {
    mda_mem_set_uint8 (obj, addr + 1, val >> 8);
  }
}

void mda_crtc_set_reg (mda_t *mda, unsigned reg, unsigned char val)
{
  if (reg > 15) {
    return;
  }

  mda->crtc_reg[reg] = val;

  switch (reg) {
    case 0x0e:
      mda_set_pos (mda, (val << 8) | (mda->crtc_reg[0x0f] & 0xff));
      break;

    case 0x0f:
      mda_set_pos (mda, (mda->crtc_reg[0x0e] << 8) | val);
      break;
  }
}

void mda_crtc_set_uint8 (void *obj, unsigned long addr, unsigned char val)
{
  mem_blk_t *blk;
  mda_t     *mda;

  blk = (mem_blk_t *) obj;
  mda = (mda_t *) blk->ext;

  switch (addr) {
    case 0x00:
      blk->data[addr] = val;
      break;

    case 0x01:
      mda_crtc_set_reg (mda, blk->data[0], val);
      break;

    default:
      blk->data[addr] = val;
      break;
  }
}

void mda_crtc_set_uint16 (void *obj, unsigned long addr, unsigned short val)
{
  mem_blk_t *mem;

  mem = (mem_blk_t *) obj;

  mda_mem_set_uint8 (obj, addr, val & 0xff);

  if (addr < mem->end) {
    mda_mem_set_uint8 (obj, addr + 1, val >> 8);
  }
}

unsigned char mda_crtc_get_uint8 (void *obj, unsigned long addr)
{
  mem_blk_t *blk;

  blk = (mem_blk_t *) obj;

  return (blk->data[addr]);
}