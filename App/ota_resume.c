#include <string.h>
#include "ota_resume.h"

static uint8_t OtaCalcCrc8(uint8_t *buf, uint32_t len)
{
    uint8_t crc = 0xFF;
    uint32_t byte;
    uint8_t i;

    for (byte = 0; byte < len; byte++)
    {
        crc ^= buf[byte];
        for (i = 8; i > 0; --i)
        {
            if (crc & 0x80)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static bool OtaProgressReadSlot(uint32_t addr, OtaProgress_t *progress)
{
    uint8_t crcVal;

    if (!FlashRead(addr, (uint8_t *)progress, sizeof(OtaProgress_t)))
    {
        return false;
    }

    if (progress->magicCode != OTA_PROGRESS_MAGIC)
    {
        return false;
    }

    crcVal = OtaCalcCrc8((uint8_t *)progress, sizeof(OtaProgress_t) - 1);
    if (crcVal != progress->crcVal)
    {
        return false;
    }

    if (progress->downloadedSize > progress->fwSize || progress->fwSize > OTA_FIRMWARE_MAX_SIZE)
    {
        return false;
    }

    return true;
}

bool OtaProgressLoad(OtaProgress_t *progress)
{
    OtaProgress_t slotA;
    OtaProgress_t slotB;
    bool validA;
    bool validB;

    if (progress == 0)
    {
        return false;
    }

    validA = OtaProgressReadSlot(OTA_PROGRESS_SLOT_A_ADDR, &slotA);
    validB = OtaProgressReadSlot(OTA_PROGRESS_SLOT_B_ADDR, &slotB);

    if (validA && validB)
    {
        *progress = (slotA.sequence >= slotB.sequence) ? slotA : slotB;
        return true;
    }
    if (validA)
    {
        *progress = slotA;
        return true;
    }
    if (validB)
    {
        *progress = slotB;
        return true;
    }
    return false;
}

bool OtaProgressSave(OtaProgress_t *progress)
{
    OtaProgress_t current;
    uint32_t writeAddr = OTA_PROGRESS_SLOT_A_ADDR;

    if (progress == 0 || progress->fwSize > OTA_FIRMWARE_MAX_SIZE)
    {
        return false;
    }

    if (OtaProgressLoad(&current))
    {
        progress->sequence = current.sequence + 1;
        writeAddr = (current.sequence & 0x01U) ? OTA_PROGRESS_SLOT_A_ADDR : OTA_PROGRESS_SLOT_B_ADDR;
    }
    else
    {
        progress->sequence = 1;
    }

    progress->magicCode = OTA_PROGRESS_MAGIC;
    progress->crcVal = OtaCalcCrc8((uint8_t *)progress, sizeof(OtaProgress_t) - 1);

    if (!FlashErase(writeAddr, OTA_PROGRESS_SLOT_SIZE))
    {
        return false;
    }
    if (!FlashWrite(writeAddr, (uint8_t *)progress, sizeof(OtaProgress_t)))
    {
        return false;
    }
    return true;
}

void OtaProgressInvalidate(void)
{
    FlashErase(OTA_PROGRESS_SLOT_A_ADDR, OTA_PROGRESS_SLOT_SIZE);
    FlashErase(OTA_PROGRESS_SLOT_B_ADDR, OTA_PROGRESS_SLOT_SIZE);
}

bool OtaProgressMatch(const OtaProgress_t *progress, uint32_t fwSize, const char *tid, const char *version, const char *md5)
{
    if (progress == 0 || tid == 0 || version == 0 || md5 == 0)
    {
        return false;
    }

    if (progress->fwSize != fwSize)
    {
        return false;
    }
    if (strncmp(progress->tid, tid, sizeof(progress->tid)) != 0)
    {
        return false;
    }
    if (strncmp(progress->version, version, sizeof(progress->version)) != 0)
    {
        return false;
    }
    if (strncmp(progress->md5, md5, sizeof(progress->md5)) != 0)
    {
        return false;
    }
    return true;
}
