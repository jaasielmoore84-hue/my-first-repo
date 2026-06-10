#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include <stdint.h>

#define APP_VERSION_DEFAULT             "1.0"
#define OTA_DOWNLOAD_SPLIT_SIZE         512U
#define OTA_CHECK_VERSION_PERIOD_MS     3000UL
#define OTA_PROGRESS_SAVE_BYTES         OTA_DOWNLOAD_SPLIT_SIZE

typedef struct
{
    const char *wifiSsid;
    const char *wifiPassword;

    const char *otaHost;
    uint16_t otaPort;
    const char *productId;
    const char *deviceName;
    const char *authorization;
    const char *firmwareType;

    uint32_t atTimeoutShortMs;
    uint32_t atTimeoutNormalMs;
    uint32_t atTimeoutLongMs;
} AppConfig_t;

void AppConfigInit(void);
const AppConfig_t *AppConfigGet(void);

#endif
