#ifndef _OTA_RESUME_H_
#define _OTA_RESUME_H_

#include <stdint.h>
#include <stdbool.h>
#include "inflash_drv.h"

#define OTA_PROGRESS_MAGIC          (0x4F544150UL)
#define OTA_PROGRESS_SLOT_SIZE      FLASH_PAGE_SIZE
#define OTA_PROGRESS_SLOT_A_ADDR    (DOWNLOAD_AREA_ADDR_IN_FLASH + DOWNLOAD_AREA_SIZE - (2U * FLASH_PAGE_SIZE))
#define OTA_PROGRESS_SLOT_B_ADDR    (DOWNLOAD_AREA_ADDR_IN_FLASH + DOWNLOAD_AREA_SIZE - FLASH_PAGE_SIZE)
#define OTA_FIRMWARE_MAX_SIZE       (DOWNLOAD_AREA_SIZE - (2U * FLASH_PAGE_SIZE))

typedef enum
{
    OTA_PROGRESS_EMPTY = 0,
    OTA_PROGRESS_DOWNLOADING,
    OTA_PROGRESS_DOWNLOADED,
    OTA_PROGRESS_VERIFIED
} OtaProgressState_t;

#pragma pack(1)
typedef struct
{
    uint32_t magicCode;
    uint32_t sequence;
    uint32_t state;
    uint32_t fwSize;
    uint32_t downloadedSize;
    char tid[10];
    char version[10];
    char md5[33];
    uint8_t crcVal;
} OtaProgress_t;
#pragma pack()

bool OtaProgressLoad(OtaProgress_t *progress);
bool OtaProgressSave(OtaProgress_t *progress);
void OtaProgressInvalidate(void);
bool OtaProgressMatch(const OtaProgress_t *progress, uint32_t fwSize, const char *tid, const char *version, const char *md5);

#endif
