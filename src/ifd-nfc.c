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
#include "atr.h"
#include <debuglog.h>
#include <ifdhandler.h>
#include <nfc/nfc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct nfc_slot {
    bool present;
    nfc_target_t nt;
    unsigned char atr[MAX_ATR_SIZE];
    size_t atrlen;
};

static struct nfc_slot ifd_slot;
static nfc_device_t *ifd_device;
static bool active;

#define release_return(code) { release(&ifd_device, &ifd_slot); return code; }

#define NFC_MAX_DEVICE_COUNT 4

static const char str_NM_ISO14443A_106[]  = "ISO14443-A (NXP MIFARE)";
static const char str_NM_FELICA_424[]     = "JIS X 6319-4 (Sony Felica)";
static const char str_NM_ISO14443B_106[]  = "ISO14443-B";
static const char str_NM_JEWEL_106[]   = "Jewel Topaz";
static const char str_NM_ACTIVE_DEP[]  = "DEP";
static const char str_unknown[]           = "unknown";
static const char *str_modulation(nfc_modulation_t m) {
    switch (m.nmt) {
        case NMT_ISO14443A:
            return str_NM_ISO14443A_106;
        case NMT_FELICA:
            return str_NM_FELICA_424;
        case NMT_ISO14443B:
            return str_NM_ISO14443B_106;
        case NMT_JEWEL:
            return str_NM_JEWEL_106;
        case NMT_DEP:
            return str_NM_ACTIVE_DEP;
        default:
            return str_unknown;
    }
}

static const nfc_modulation_t ct_modulations[] = {
    { NMT_ISO14443A, NBR_106 },
    { NMT_ISO14443B, NBR_106 },
};

static void release(nfc_device_t **device, struct nfc_slot *slot)
{
    if (device && *device) {
        if (slot && slot->present && !nfc_initiator_deselect_target(*device))
            Log3(PCSC_LOG_ERROR, "Could not disconnect from %s (%s).",
                    str_modulation(slot->nt.nm),
                    nfc_strerror(*device));

        nfc_disconnect(*device);

        slot->present = false;
        *device = NULL;
    }
}

static bool get_slot_atr(const struct nfc_slot *nslot,
        unsigned char *atr, size_t *atr_len)
{
    unsigned char atqb[12];

    switch (nslot->nt.nm.nmt) {
        case NMT_ISO14443A:
            /* libnfc already strips TL and CRC1/CRC2 */
            if (!get_atr(ATR_ISO14443A_106,
                    nslot->nt.nti.nai.abtAts, nslot->nt.nti.nai.szAtsLen,
                    (unsigned char *) nslot->atr, atr_len)) {
                return false;
            }
            break;
        case NMT_ISO14443B:
            // First ATQB byte always equal to 0x50
            atqb[0] = 0x50;

            // Store the PUPI (Pseudo-Unique PICC Identifier)
            memcpy (&atqb[1], nslot->nt.nti.nbi.abtPupi, 4);

            // Store the Application Data
            memcpy (&atqb[5], nslot->nt.nti.nbi.abtApplicationData, 4);

            // Store the Protocol Info
            memcpy (&atqb[9], nslot->nt.nti.nbi.abtProtocolInfo, 3);

            if (!get_atr(ATR_ISO14443A_106, atqb, sizeof(atqb),
                        (unsigned char *) nslot->atr, atr_len))
                return false;
            break;
        default:
            /* for all other types: Empty ATR */
            Log1(PCSC_LOG_INFO, "Returning empty ATR "
                    "for card without APDU support.");
            *atr_len = 0;
            return true;
    }

    return true;
}

static bool get_device(nfc_device_t **device)
{
    if (!device)
        return false;
    if (*device)
        return true;

    *device = nfc_connect(NULL);
    if (!*device) {
        Log1(PCSC_LOG_ERROR, "Could not connect to NFC device");
        return false;
    }

    if (!nfc_initiator_init(*device)
            || !nfc_configure(ifd_device, NDO_HANDLE_CRC, true)
            || !nfc_configure(ifd_device, NDO_HANDLE_PARITY, true)) {
        Log4(PCSC_LOG_ERROR, "Could not initialize %.*s as reader (%s).",
                DEVICE_NAME_LENGTH, (*device)->acName, nfc_strerror(*device));
        return false;
    }

    if (!nfc_configure(*device, NDO_INFINITE_SELECT, false))
        Log2(PCSC_LOG_ERROR,
                "Could not deactivate infinite polling for targets (%s)."
                " This might block the application sometimes...",
                nfc_strerror(*device));

    Log3(PCSC_LOG_INFO, "Connected to %.*s.",
                DEVICE_NAME_LENGTH, (*device)->acName);

    return true;
}

static bool get_target(nfc_device_t *device, struct nfc_slot *slot)
{
    if (!slot)
        return false;

    if (slot->present)
        return true;

    int i;

    /* find new connection */
    for (i = 0; i < sizeof(ct_modulations); i++) {
        ifd_slot.atrlen = sizeof(ifd_slot.atr);

        if (nfc_initiator_select_passive_target(device, ct_modulations[i], NULL, 0,
                    &slot->nt)
                && get_slot_atr(&ifd_slot, ifd_slot.atr, &ifd_slot.atrlen)) {
            slot->present = true;

            Log2(PCSC_LOG_INFO, "Connected to %s.",
                    str_modulation(slot->nt.nm));

            return true;
        }
    }

    Log1(PCSC_LOG_DEBUG, "Could not find any NFC targets.");

    return false;
}


/*
 * List of Defined Functions Available to IFD_Handler 3.0
 */
RESPONSECODE
IFDHCreateChannelByName(DWORD Lun, LPSTR DeviceName)
{
    ifd_slot.present = false;
    ifd_device = NULL;
    active = false;

    Log2(PCSC_LOG_INFO, "Will not open %s, "
            "but connect to NFC devices on demand.", DeviceName);

    Log2(PCSC_LOG_INFO, "IFD-handler for libnfc %sactive.",
            active ? "" : "not ");

    return IFD_SUCCESS;
}

RESPONSECODE
IFDHCreateChannel (DWORD Lun, DWORD Channel)
{
    char str[16];
    snprintf(str, sizeof str, "/dev/pcsc/%lu", (unsigned long) Channel);

    return IFDHCreateChannelByName(Lun, str);
}

RESPONSECODE
IFDHCloseChannel(DWORD Lun)
{
    /* nfc_configure doesn't check ifd_device... */
    if (ifd_device)
        if (!nfc_configure(ifd_device, NDO_ACTIVATE_FIELD, false))
            Log2(PCSC_LOG_ERROR, "Could not deactivate NFC field (%s).",
                    nfc_strerror(ifd_device));

    release_return(IFD_SUCCESS);
}

RESPONSECODE
IFDHGetCapabilities(DWORD Lun, DWORD Tag, PDWORD Length, PUCHAR Value)
{
    if (!active || !Length || !Value)
        return IFD_COMMUNICATION_ERROR;

    switch(Tag) {
        case TAG_IFD_ATR:
#ifdef SCARD_ATTR_ATR_STRING
        case SCARD_ATTR_ATR_STRING:
#endif
            if (!get_device(&ifd_device)
                    || !get_target(ifd_device, &ifd_slot))
                release_return(IFD_COMMUNICATION_ERROR);
            if (*Length < ifd_slot.atrlen)
                return IFD_COMMUNICATION_ERROR;

            memcpy(Value, ifd_slot.atr, ifd_slot.atrlen);
            *Length = ifd_slot.atrlen;
            break;
        case TAG_IFD_SLOTS_NUMBER:
            if (*Length < 1)
                return IFD_COMMUNICATION_ERROR;

            *Value  = 1;
            *Length = 1;
            release(&ifd_device, &ifd_slot);
            break;
        default:
            Log2(PCSC_LOG_ERROR, "Tag %lu not supported", (unsigned long) Tag);
            return IFD_ERROR_TAG;
    }

    return IFD_SUCCESS;
}

RESPONSECODE
IFDHSetCapabilities(DWORD Lun, DWORD Tag, DWORD Length, PUCHAR Value)
{
    return IFD_ERROR_VALUE_READ_ONLY;
}

RESPONSECODE
IFDHSetProtocolParameters(DWORD Lun, DWORD Protocol, UCHAR Flags, UCHAR PTS1,
        UCHAR PTS2, UCHAR PTS3)
{
    if (Protocol != SCARD_PROTOCOL_T1)
        return IFD_PROTOCOL_NOT_SUPPORTED;

    return IFD_SUCCESS;
}

RESPONSECODE
IFDHPowerICC(DWORD Lun, DWORD Action, PUCHAR Atr, PDWORD AtrLength)
{
    if (!active || !Atr || !AtrLength)
        return IFD_COMMUNICATION_ERROR;

    switch (Action) {
        case IFD_POWER_DOWN:
            if (!get_device(&ifd_device))
                release_return(IFD_COMMUNICATION_ERROR);

            /* XXX see bug #312754 on https://alioth.debian.org/projects/pcsclite */
#if 0
            *AtrLength = 0;

#endif
            if (!nfc_configure(ifd_device, NDO_ACTIVATE_FIELD, false)) {
                Log2(PCSC_LOG_ERROR, "Could not deactivate NFC field (%s).",
                        nfc_strerror(ifd_device));
                release_return(IFD_ERROR_POWER_ACTION);
            }
            release_return(IFD_SUCCESS);
            break;
        case IFD_RESET:
            release(&ifd_device, &ifd_slot);
        case IFD_POWER_UP:
            if (!get_device(&ifd_device))
                release_return(IFD_COMMUNICATION_ERROR);

            if (!nfc_configure(ifd_device, NDO_ACTIVATE_FIELD, true)) {
                Log2(PCSC_LOG_ERROR, "Could not activate NFC field (%s).",
                        nfc_strerror(ifd_device));
                *AtrLength = 0;
                release_return(IFD_ERROR_POWER_ACTION);
            }
            break;
        default:
            Log2(PCSC_LOG_ERROR, "Action %lu not supported", (unsigned long) Action);
            return IFD_NOT_SUPPORTED;
    }

    if (!get_target(ifd_device, &ifd_slot))
        release_return(IFD_COMMUNICATION_ERROR);
    if (*AtrLength < ifd_slot.atrlen)
        return IFD_COMMUNICATION_ERROR;

    memcpy(Atr, ifd_slot.atr, ifd_slot.atrlen);
    memset(Atr + ifd_slot.atrlen, 0, *AtrLength - ifd_slot.atrlen);
    *AtrLength = ifd_slot.atrlen;

    return IFD_SUCCESS;
}

RESPONSECODE
IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci, PUCHAR TxBuffer, DWORD
        TxLength, PUCHAR RxBuffer, PDWORD RxLength, PSCARD_IO_HEADER RecvPci)
{
    if (!RxLength || !RecvPci)
        return IFD_COMMUNICATION_ERROR;

    if (!active || !ifd_device || !ifd_slot.present) {
        *RxLength = 0;
        return IFD_ICC_NOT_PRESENT;
    }

    LogXxd(PCSC_LOG_INFO, "Sending to NFC target\n", TxBuffer, TxLength);

    size_t tl = TxLength, rl = *RxLength;

    if(!nfc_initiator_transceive_bytes(ifd_device, TxBuffer, tl,
                RxBuffer, &rl)) {
        Log2(PCSC_LOG_ERROR, "Could not transceive data (%s).",
                nfc_strerror(ifd_device));
        *RxLength = 0;
        release_return(IFD_COMMUNICATION_ERROR);
    }

    *RxLength = rl;
    RecvPci->Protocol = 1;

    LogXxd(PCSC_LOG_INFO, "Received from NFC target\n", RxBuffer, *RxLength);

    return IFD_SUCCESS;
}

RESPONSECODE
IFDHICCPresence(DWORD Lun)
{
    if (!active)
        return IFD_ICC_NOT_PRESENT;

        
    if (!get_device(&ifd_device)
            || !get_target(ifd_device, &ifd_slot))
        release_return(IFD_ICC_NOT_PRESENT);

    return IFD_SUCCESS;
}

RESPONSECODE
IFDHControl(DWORD Lun, DWORD dwControlCode, PUCHAR TxBuffer, DWORD TxLength,
        PUCHAR RxBuffer, DWORD RxLength, LPDWORD pdwBytesReturned)
{
    if (pdwBytesReturned)
        *pdwBytesReturned = 0;

    switch(dwControlCode) {
        case IFDNFC_CTRL_ACTIVE:
            if (TxLength != 1 || !TxBuffer || RxLength < 1 || !RxBuffer)
                return IFD_COMMUNICATION_ERROR;

            switch (*TxBuffer) {
                case IFDNFC_SET_ACTIVE:
                    active = true;
                    break;
                case IFDNFC_SET_INACTIVE:
                    release(&ifd_device, &ifd_slot);
                    active = false;
                    break;
                case IFDNFC_GET_STATUS:
                    break;
                default:
                    Log4(PCSC_LOG_ERROR, "Value for active request "
                            "must be one of %lu %lu %lu.",
                            (unsigned long) IFDNFC_SET_ACTIVE,
                            (unsigned long) IFDNFC_SET_INACTIVE,
                            (unsigned long) IFDNFC_GET_STATUS);
                    return IFD_COMMUNICATION_ERROR;
            }

            if (pdwBytesReturned)
                *pdwBytesReturned = 1;
            if (active) {
                Log1(PCSC_LOG_INFO, "IFD-handler for libnfc is active.");
                *RxBuffer = IFDNFC_IS_ACTIVE;
            } else {
                Log1(PCSC_LOG_INFO, "IFD-handler for libnfc is inactive.");
                *RxBuffer = IFDNFC_IS_INACTIVE;
            }
            break;
        default:
            return IFD_ERROR_NOT_SUPPORTED;
    }

    return IFD_SUCCESS;
}
