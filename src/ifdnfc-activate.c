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
#include "ifd-nfc.h"
#include <pcsclite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winscard.h>

int
main(int argc, char *argv[])
{
    LONG rv;
    SCARDCONTEXT hContext;
    SCARDHANDLE hCard;
    LPSTR mszReaders = NULL;
    char *reader;
    BYTE pbSendBuffer[1];
    BYTE pbRecvBuffer[1];
    DWORD dwActiveProtocol, dwRecvLength, dwReaders;

    if (argc == 1 ||
            (argc == 2 && (strncmp(argv[1], "yes", strlen("yes")) == 0)))
        pbSendBuffer[0] = IFDNFC_SET_ACTIVE;
    else if (argc == 2 && (strncmp(argv[1], "no", strlen("no")) == 0))
        pbSendBuffer[0] = IFDNFC_SET_INACTIVE;
    else if (argc == 2 && (strncmp(argv[1], "status", strlen("status")) == 0))
        pbSendBuffer[0] = IFDNFC_GET_STATUS;
    else {
        printf("Usage: %s [yes|no|status]\n", argv[0]);
        exit(1);
    }


    rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    if (rv < 0)
        goto err;

    dwReaders = SCARD_AUTOALLOCATE;
    rv = SCardListReaders(hContext, NULL, (LPSTR)&mszReaders, &dwReaders);
    if (rv < 0)
        goto err;

    int l;
    for (reader = mszReaders;
            dwReaders > 0;
            l = strlen(reader) + 1, dwReaders -= l, reader += l) {

        if (strcmp(IFDNFC_READER_NAME, reader) <= 0)
            break;
    }
    if (dwReaders <= 0) {
        printf("Could not find %s\n", IFDNFC_READER_NAME);
        rv = SCARD_E_NO_READERS_AVAILABLE;
        goto err;
    }


    rv = SCardConnect(hContext, reader, SCARD_SHARE_DIRECT, 0, &hCard,
            &dwActiveProtocol);
    if (rv < 0)
        goto err;


    rv = SCardControl(hCard, IFDNFC_CTRL_ACTIVE, pbSendBuffer,
            sizeof(pbSendBuffer), pbRecvBuffer, sizeof(pbRecvBuffer),
            &dwRecvLength);
    if (rv < 0)
        goto err;
    if (dwRecvLength != 1) {
        rv = SCARD_F_INTERNAL_ERROR;
        goto err;
    }

    switch (pbRecvBuffer[0]) {
        case IFDNFC_IS_ACTIVE:
            printf("%s is active.\n", IFDNFC_READER_NAME);
            break;
        case IFDNFC_IS_INACTIVE:
            printf("%s is inactive.\n", IFDNFC_READER_NAME);
            break;
        default:
            rv = SCARD_F_INTERNAL_ERROR;
            goto err;
    }


    rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    if (rv < 0)
        goto err;

    rv = SCardFreeMemory(hContext, mszReaders);
    if (rv < 0)
        goto err;


    exit(0);

err:
    puts(pcsc_stringify_error(rv));
    if (mszReaders)
        SCardFreeMemory(hContext, mszReaders);


    exit(1);
}
