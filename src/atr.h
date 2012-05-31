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
#ifndef _ATR_H_
#define _ATR_H_

#include <stddef.h>

enum atr_modulation {
    ATR_ISO14443A_106,
    ATR_ISO14443B_106,
    ATR_DEFAULT,
};

/** 
 * @brief 
 * 
 * @param [in]     modulation
 * @param [in]     in      ATS without TL/CRC1/CRC2 for \c ATR_ISO14443A_106 and ATQB for \c ATR_ISO14443B_106
 * @param [in]     inlen   Length of \a in
 * @param [in,out] atr     where to store the ATR. Sould be big enough.
 * @param [in,out] atr_len Length of \a atr
 * 
 * @return 
 */
int get_atr(enum atr_modulation modulation,
        const unsigned char *in, size_t inlen,
        unsigned char *atr, size_t *atr_len);

#endif
