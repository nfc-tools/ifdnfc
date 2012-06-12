/*
 * Copyright (C) 2010 Frank Morgner
 *
 * This file is part of ifdnfc.
 *
 * ifdnfc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * ifdnfc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ifdnfc.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "atr.h"
#include <string.h>
#include <debuglog.h>

int get_atr(enum atr_modulation modulation,
            const unsigned char *in, size_t inlen,
            unsigned char *atr, size_t *atr_len)
{
  unsigned char hb[0xff - 1], tck;
  size_t hb_len, idx, len;

  if (!in || !atr_len)
    return 0;

  /* get the Historical Bytes */
  switch (modulation) {
    case ATR_ISO14443A_106:
      LogXxd(PCSC_LOG_DEBUG, "Will calculate ATR from this ATS payload: ",
             in, inlen);

      if (inlen) {
        idx = 1;

        /* Bits 5 to 7 tell if TA1/TB1/TC1 are available */
        if (in[0] & 0x10) { // TA
          idx++;
        }
        if (in[0] & 0x20) { // TB
          idx++;
        }
        if (in[0] & 0x40) { // TC
          idx++;
        }

        if (idx < inlen) {
          hb_len = inlen - idx;
          memcpy(hb, in + idx, hb_len);

          Log3(PCSC_LOG_DEBUG, "Found %zu interface byte(s)"
               " and %zu historical byte(s)",
               idx - 2, hb_len);
        } else {
          hb_len = 0;
        }
      } else {
        hb_len = 0;
      }
      break;
    case ATR_ISO14443B_106:
      LogXxd(PCSC_LOG_DEBUG, "Will calculate ATR from this ATQB: ",
             in, inlen);
      if (inlen < 12) {
        Log1(PCSC_LOG_INFO, "ATQB too short "
             "to contain historical bytes");
        hb_len = 0;
        break;
      }
      memcpy(hb, in + 5, 7);
      hb[7] = 0;
      hb_len = 8;
      break;
    case ATR_DEFAULT:
      hb_len = 0;
      break;
    default:
      /* for all other types: Empty ATR */
      Log1(PCSC_LOG_INFO, "Returning empty ATR "
           "for card without APDU support.");
      *atr_len = 0;
      return 1;
  }

  /* length of ATR without TCK */
  len = 4 + hb_len;

  if (*atr_len < len + 1)
    return 0;

  atr[0] = 0x3b;
  atr[1] = 0x80 + hb_len;
  atr[2] = 0x80;
  atr[3] = 0x01;
  memcpy(&atr[4], hb, hb_len);

  /* calculate TCK */
  tck = atr[1];
  for (idx = 2; idx < len; idx++) {
    tck ^= atr[idx];
  }

  atr[len] = tck;

  *atr_len = len + 1;

  return 1;
}
