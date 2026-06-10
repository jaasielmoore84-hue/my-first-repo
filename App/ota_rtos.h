#ifndef _OTA_RTOS_H_
#define _OTA_RTOS_H_

#include <stdint.h>
#include <stdbool.h>

#define OTA_EVT_WIFI_CONNECTED      (1U << 0)
#define OTA_EVT_WIFI_DISCONNECTED   (1U << 1)
#define OTA_EVT_START               (1U << 2)
#define OTA_EVT_DONE                (1U << 3)
#define OTA_EVT_FAIL                (1U << 4)

typedef enum
{
    OTA_CMD_NONE = 0,
    OTA_CMD_START,
    OTA_CMD_PAUSE,
    OTA_CMD_RESUME,
    OTA_CMD_CANCEL
} OtaCommand_t;

void OtaRtosInit(void);
bool OtaRtosPostCommand(OtaCommand_t cmd);
bool OtaRtosFetchCommand(OtaCommand_t *cmd);
void OtaRtosSetEvents(uint32_t events);
void OtaRtosClearEvents(uint32_t events);
uint32_t OtaRtosGetEvents(void);
void OtaRtosNetworkTask(void);

#endif
