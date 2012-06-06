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
#include <inttypes.h>

/*
 * This implementation was written based on information provided by the
 * following documents:
 *
 * From PC/SC specifications:
 * http://www.pcscworkgroup.com/specifications/specdownload.php
 *
 *  Interoperability Specification for ICCs and Personal Computer Systems
 *   Part 3. Requirements for PC-Connected Interface Devices
 *   PCSC_Part3 - v2.01.09
 *   http://www.pcscworkgroup.com/specifications/files/pcsc3_v2.01.09.pdf
 */

struct ifd_slot {
  bool present;
  bool powered;
  nfc_target target;
  unsigned char atr[MAX_ATR_SIZE];
  size_t atr_len;
};

struct ifd_device {
  nfc_device *device;
  struct ifd_slot slot;
  bool connected;
};

static struct ifd_device ifdnfc;

static nfc_connstring ifd_connstring;

static const nfc_modulation supported_modulations[] = {
  { NMT_ISO14443A, NBR_106 },
};

static void ifdnfc_disconnect(void)
{
  if (ifdnfc.connected) {
    if (ifdnfc.slot.present) {
      if (nfc_initiator_deselect_target(ifdnfc.device) < 0) {
        Log3(PCSC_LOG_ERROR, "Could not disconnect from %s (%s).", str_nfc_modulation_type(ifdnfc.slot.target.nm.nmt), nfc_strerror(ifdnfc.device));
      } else {
        ifdnfc.slot.present = false;
      }
    }
    nfc_close(ifdnfc.device);
    ifdnfc.connected = false;
    ifdnfc.device = NULL;
  }
}

static bool ifdnfc_target_to_atr(void)
{
  unsigned char atqb[12];
  ifdnfc.slot.atr_len = sizeof(ifdnfc.slot.atr);

  switch (ifdnfc.slot.target.nm.nmt) {
    case NMT_ISO14443A:
      /* libnfc already strips TL and CRC1/CRC2 */
      if (!get_atr(ATR_ISO14443A_106,
                   ifdnfc.slot.target.nti.nai.abtAts, ifdnfc.slot.target.nti.nai.szAtsLen,
                   (unsigned char *) ifdnfc.slot.atr, &(ifdnfc.slot.atr_len))) {
        ifdnfc.slot.atr_len = 0;
        return false;
      }
      break;
    case NMT_ISO14443B:
      // First ATQB byte always equal to 0x50
      atqb[0] = 0x50;

      // Store the PUPI (Pseudo-Unique PICC Identifier)
      memcpy(&atqb[1], ifdnfc.slot.target.nti.nbi.abtPupi, 4);

      // Store the Application Data
      memcpy(&atqb[5], ifdnfc.slot.target.nti.nbi.abtApplicationData, 4);

      // Store the Protocol Info
      memcpy(&atqb[9], ifdnfc.slot.target.nti.nbi.abtProtocolInfo, 3);

      if (!get_atr(ATR_ISO14443A_106, atqb, sizeof(atqb),
                   (unsigned char *) ifdnfc.slot.atr, &(ifdnfc.slot.atr_len)))
        ifdnfc.slot.atr_len = 0;
      return false;
      break;
    case NMT_ISO14443BI:
    case NMT_ISO14443B2CT:
    case NMT_ISO14443B2SR:
    case NMT_JEWEL:
    case NMT_FELICA:
    case NMT_DEP:
      /* for all other types: Empty ATR */
      Log1(PCSC_LOG_INFO, "Returning empty ATR for card without APDU support.");
      ifdnfc.slot.atr_len = 0;
      return true;
  }

  return true;
}

static bool ifdnfc_reselect_target(void)
{
  switch (ifdnfc.slot.target.nm.nmt) {
    case NMT_ISO14443A: {
      if (nfc_device_set_property_bool(ifdnfc.device, NP_INFINITE_SELECT, false) < 0) {
        Log2(PCSC_LOG_ERROR, "Could not set infinite-select property (%s)", nfc_strerror(ifdnfc.device));
        ifdnfc.slot.present = false;
        return false;
      }
      nfc_target nt;
      if (nfc_initiator_select_passive_target(ifdnfc.device, ifdnfc.slot.target.nm, ifdnfc.slot.target.nti.nai.abtUid, ifdnfc.slot.target.nti.nai.szUidLen, &nt) < 1) {
        Log3(PCSC_LOG_DEBUG, "Could not select target %s. (%s)", str_nfc_modulation_type(ifdnfc.slot.target.nm.nmt), nfc_strerror(ifdnfc.device));
        ifdnfc.slot.present = false;
        return false;
      } else {
        return true;
      }
    }
    break;
    default:
      // TODO Implement me :)
      break;
  }
  return false;
}

static bool ifdnfc_target_is_available(void)
{
  if (!ifdnfc.connected)
    return false;

  if (ifdnfc.slot.present) {
    if (ifdnfc.slot.powered) {
      // Target is active and just need a ping-like command (handled by libnfc)
      if (nfc_initiator_target_is_present(ifdnfc.device, ifdnfc.slot.target) < 0) {
        Log3(PCSC_LOG_INFO, "Connection lost with %s. (%s)", str_nfc_modulation_type(ifdnfc.slot.target.nm.nmt), nfc_strerror(ifdnfc.device));
        ifdnfc.slot.present = false;
        return false;
      }
      return true;
    } else {
      // Target is not powered and need to be wakeup
      if (nfc_initiator_init(ifdnfc.device) < 0) {
        Log2(PCSC_LOG_ERROR, "Could not initialize initiator mode. (%s)", nfc_strerror(ifdnfc.device));
        ifdnfc.slot.present = false;
        return false;
      }
      if (!ifdnfc_reselect_target()) {
        Log3(PCSC_LOG_INFO, "Connection lost with %s. (%s)", str_nfc_modulation_type(ifdnfc.slot.target.nm.nmt), nfc_strerror(ifdnfc.device));
        ifdnfc.slot.present = false;
        return false;
      }
      if (nfc_initiator_deselect_target(ifdnfc.device) < 0) {
        Log2(PCSC_LOG_ERROR, "Could not deselect target. (%s)", nfc_strerror(ifdnfc.device));
      }
      return true;
    }
  } // else

  // ifdnfc.slot not powered means the field is not active, so when no target
  // is available ifdnfc needs to generated a field
  if (!ifdnfc.slot.powered) {
    if (nfc_initiator_init(ifdnfc.device) < 0) {
      Log2(PCSC_LOG_ERROR, "Could not init NFC device in initiator mode (%s).", nfc_strerror(ifdnfc.device));
      return false;
    }
    // To prevent from multiple init
    ifdnfc.slot.powered = true;
  }

  // find new connection
  size_t i;
  for (i = 0; i < (sizeof(supported_modulations) / sizeof(nfc_modulation)); i++) {
    if (nfc_initiator_list_passive_targets(ifdnfc.device, supported_modulations[i], &(ifdnfc.slot.target), 1) == 1) {
      ifdnfc_target_to_atr();
      ifdnfc.slot.present = true;
      // XXX Should it be on or off after target selection ?
      ifdnfc.slot.powered = true;
      Log2(PCSC_LOG_INFO, "Connected to %s.", str_nfc_modulation_type(ifdnfc.slot.target.nm.nmt));
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
  (void) Lun;
  ifdnfc.device = NULL;
  ifdnfc.connected = false;
  ifdnfc.slot.present = false;
  nfc_init(NULL);

  // USB DeviceNames can be immediately handled, e.g.:
  // usb:1fd3/0608:libudev:0:/dev/bus/usb/002/079
  // => connstring usb:002:079
  int n = strlen(DeviceName) + 1;
  char *vidpid      = malloc(n);
  char *hpdriver    = malloc(n);
  char *ifn         = malloc(n);
  char *devpath     = malloc(n);
  char *dirname     = malloc(n);
  char *filename    = malloc(n);

  int res = sscanf(DeviceName, "usb:%[^:]:%[^:]:%[^:]:%[^:]", vidpid, hpdriver, ifn, devpath);
  if (res == 4) {
    int res = sscanf(devpath, "/dev/bus/usb/%[^/]/%[^/]", dirname, filename);
    if (res == 2) {
      strcpy(ifd_connstring, "usb:xxx:xxx");
      memcpy(ifd_connstring + 4, dirname, 3);
      memcpy(ifd_connstring + 8, filename, 3);
      ifdnfc.device = nfc_open(NULL, ifd_connstring);
      ifdnfc.connected = (ifdnfc.device) ? true : false;
    }
  }
  free(vidpid);
  free(hpdriver);
  free(ifn);
  free(devpath);
  free(dirname);
  free(filename);

  if (!ifdnfc.connected)
    Log2(PCSC_LOG_DEBUG, "\"DEVICENAME    %s\" is not used.", DeviceName);
  else
    Log2(PCSC_LOG_DEBUG, "\"DEVICENAME    %s\" is used by libnfc.", DeviceName);
  Log1(PCSC_LOG_INFO, "IFD-handler for NFC devices is ready.");
  return IFD_SUCCESS;
}

RESPONSECODE
IFDHCreateChannel(DWORD Lun, DWORD Channel)
{
  char str[16];
  snprintf(str, sizeof str, "/dev/pcsc/%lu", (unsigned long) Channel);

  return IFDHCreateChannelByName(Lun, str);
}

RESPONSECODE
IFDHCloseChannel(DWORD Lun)
{
  (void) Lun;
  ifdnfc_disconnect();
  nfc_exit(NULL);
  return IFD_SUCCESS;
}

RESPONSECODE
IFDHGetCapabilities(DWORD Lun, DWORD Tag, PDWORD Length, PUCHAR Value)
{
  Log4(PCSC_LOG_DEBUG, "IFDHGetCapabilities(DWORD Lun (%08x), DWORD Tag (%08x), PDWORD Length (%lu), PUCHAR Value)", Lun, Tag, *Length);
  (void) Lun;
  if (!Length || !Value)
    return IFD_COMMUNICATION_ERROR;

  switch (Tag) {
    case TAG_IFD_ATR:
#ifdef SCARD_ATTR_ATR_STRING
    case SCARD_ATTR_ATR_STRING:
#endif
      if (!ifdnfc.connected || !ifdnfc.slot.present)
        return(IFD_COMMUNICATION_ERROR);
      if (*Length < ifdnfc.slot.atr_len)
        return IFD_COMMUNICATION_ERROR;

      memcpy(Value, ifdnfc.slot.atr, ifdnfc.slot.atr_len);
      *Length = ifdnfc.slot.atr_len;
      break;
    case TAG_IFD_SLOTS_NUMBER:
      if (*Length < 1)
        return IFD_COMMUNICATION_ERROR;

      *Value  = 1;
      *Length = 1;
      break;
    case TAG_IFD_STOP_POLLING_THREAD:
      Log3(PCSC_LOG_ERROR, "Tag %08x (%lu) not supported", Tag, (unsigned long) Tag);
      return IFD_ERROR_TAG;
    default:
      Log3(PCSC_LOG_ERROR, "Tag %08x (%lu) not supported", Tag, (unsigned long) Tag);
      return IFD_ERROR_TAG;
  }

  return IFD_SUCCESS;
}

RESPONSECODE
IFDHSetCapabilities(DWORD Lun, DWORD Tag, DWORD Length, PUCHAR Value)
{
  (void) Lun;
  (void) Tag;
  (void) Length;
  (void) Value;
  return IFD_ERROR_VALUE_READ_ONLY;
}

RESPONSECODE
IFDHSetProtocolParameters(DWORD Lun, DWORD Protocol, UCHAR Flags, UCHAR PTS1,
                          UCHAR PTS2, UCHAR PTS3)
{
  (void) Lun;
  (void) Flags;
  (void) PTS1;
  (void) PTS2;
  (void) PTS3;
  if (Protocol != SCARD_PROTOCOL_T1)
    return IFD_PROTOCOL_NOT_SUPPORTED;

  return IFD_SUCCESS;
}

RESPONSECODE
IFDHPowerICC(DWORD Lun, DWORD Action, PUCHAR Atr, PDWORD AtrLength)
{
  (void) Lun;
  if (!Atr || !AtrLength)
    return IFD_COMMUNICATION_ERROR;

  if (!ifdnfc.connected)
    return(IFD_COMMUNICATION_ERROR);

  switch (Action) {
    case IFD_POWER_DOWN:
      // IFD_POWER_DOWN: Power down the card (Atr and AtrLength should be zeroed)
      if (nfc_idle(ifdnfc.device) < 0) {
        Log2(PCSC_LOG_ERROR, "Could not idle NFC device (%s).", nfc_strerror(ifdnfc.device));
        return IFD_ERROR_POWER_ACTION;
      }
      ifdnfc.slot.powered = false;
      *AtrLength = 0;
      return IFD_SUCCESS;
      break;
    case IFD_RESET:
      // IFD_RESET: Perform a warm reset of the card (no power down). If the card is not powered then power up the card (store and return Atr and AtrLength)
      if (ifdnfc.slot.present) {
        ifdnfc.slot.present = false;
        if (nfc_initiator_deselect_target(ifdnfc.device) < 0) {
          Log2(PCSC_LOG_ERROR, "Could not deselect NFC target (%s).", nfc_strerror(ifdnfc.device));
          return IFD_ERROR_POWER_ACTION;
        }
        if (!ifdnfc_reselect_target()) {
          return IFD_ERROR_POWER_ACTION;
        }
        return IFD_SUCCESS;
      }
      break;
    case IFD_POWER_UP:
      // IFD_POWER_UP: Power up the card (store and return Atr and AtrLength)
      if (ifdnfc_target_is_available()) {
        if (*AtrLength < ifdnfc.slot.atr_len)
          return IFD_COMMUNICATION_ERROR;
        memcpy(Atr, ifdnfc.slot.atr, ifdnfc.slot.atr_len);
        // memset(Atr + ifdnfc.slot.atr_len, 0, *AtrLength - ifd_slot.atr_len);
        *AtrLength = ifdnfc.slot.atr_len;
      } else {
        *AtrLength = 0;
        return IFD_COMMUNICATION_ERROR;
      }
      break;
    default:
      Log2(PCSC_LOG_ERROR, "Action %lu not supported", (unsigned long) Action);
      return IFD_NOT_SUPPORTED;
  }

  return IFD_SUCCESS;
}

RESPONSECODE
IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci, PUCHAR TxBuffer, DWORD
                  TxLength, PUCHAR RxBuffer, PDWORD RxLength, PSCARD_IO_HEADER RecvPci)
{
  (void) Lun;
  (void) SendPci;
  if (!RxLength || !RecvPci)
    return IFD_COMMUNICATION_ERROR;

  if (!ifdnfc.connected || !ifdnfc.slot.present) {
    *RxLength = 0;
    return IFD_ICC_NOT_PRESENT;
  }

  LogXxd(PCSC_LOG_INFO, "Sending to NFC target\n", TxBuffer, TxLength);

  size_t tl = TxLength, rl = *RxLength;
  int res;
  if ((res = nfc_initiator_transceive_bytes(ifdnfc.device, TxBuffer, tl,
                                            RxBuffer, rl, -1)) < 0) {
    Log2(PCSC_LOG_ERROR, "Could not transceive data (%s).",
         nfc_strerror(ifdnfc.device));
    *RxLength = 0;
    return(IFD_COMMUNICATION_ERROR);
  }

  *RxLength = res;
  RecvPci->Protocol = 1;

  LogXxd(PCSC_LOG_INFO, "Received from NFC target\n", RxBuffer, *RxLength);

  return IFD_SUCCESS;
}

RESPONSECODE
IFDHICCPresence(DWORD Lun)
{
  (void) Lun;
  if (!ifdnfc.connected)
    return IFD_ICC_NOT_PRESENT;
  return ifdnfc_target_is_available() ? IFD_SUCCESS : IFD_ICC_NOT_PRESENT;
}

RESPONSECODE
IFDHControl(DWORD Lun, DWORD dwControlCode, PUCHAR TxBuffer, DWORD TxLength,
            PUCHAR RxBuffer, DWORD RxLength, LPDWORD pdwBytesReturned)
{
  (void) Lun;
  if (pdwBytesReturned)
    *pdwBytesReturned = 0;

  switch (dwControlCode) {
    case IFDNFC_CTRL_ACTIVE:
      if (TxLength < 1 || !TxBuffer || RxLength < 1 || !RxBuffer)
        return IFD_COMMUNICATION_ERROR;

      switch (*TxBuffer) {
        case IFDNFC_SET_ACTIVE: {
          uint16_t u16ConnstringLength;
          if (TxLength < (1 + sizeof(u16ConnstringLength)))
            return IFD_COMMUNICATION_ERROR;
          memcpy(&u16ConnstringLength, TxBuffer + 1, sizeof(u16ConnstringLength));
          if ((TxLength - (1 + sizeof(u16ConnstringLength))) != u16ConnstringLength)
            return IFD_COMMUNICATION_ERROR;
          memcpy(ifd_connstring, TxBuffer + (1 + sizeof(u16ConnstringLength)), u16ConnstringLength);
          ifdnfc.device = nfc_open(NULL, ifd_connstring);
          ifdnfc.connected = (ifdnfc.device) ? true : false;
        }
        break;
        case IFDNFC_SET_INACTIVE:
          ifdnfc_disconnect();
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

      if (ifdnfc.connected) {
        Log1(PCSC_LOG_INFO, "IFD-handler for libnfc is active.");
        RxBuffer[0] = IFDNFC_IS_ACTIVE;
        const uint16_t u16ConnstringLength = strlen(ifd_connstring) + 1;
        memcpy(RxBuffer + 1, &u16ConnstringLength, sizeof(u16ConnstringLength));
        memcpy(RxBuffer + 1 + sizeof(u16ConnstringLength), ifd_connstring, u16ConnstringLength);
        if (pdwBytesReturned)
          *pdwBytesReturned = 1 + sizeof(u16ConnstringLength) + u16ConnstringLength;
      } else {
        Log1(PCSC_LOG_INFO, "IFD-handler for libnfc is inactive.");
        if (pdwBytesReturned)
          *pdwBytesReturned = 1;
        *RxBuffer = IFDNFC_IS_INACTIVE;
      }
      break;
    default:
      return IFD_ERROR_NOT_SUPPORTED;
  }

  return IFD_SUCCESS;
}
