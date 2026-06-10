#include <string.h>
#include "ota_rtos.h"
#include "wifi_app.h"

#define OTA_CMD_QUEUE_DEPTH 4U

static OtaCommand_t g_cmdQueue[OTA_CMD_QUEUE_DEPTH];
static uint8_t g_cmdHead;
static uint8_t g_cmdTail;
static uint8_t g_cmdCount;
static uint32_t g_eventBits;

void OtaRtosInit(void)
{
    memset(g_cmdQueue, 0, sizeof(g_cmdQueue));
    g_cmdHead = 0;
    g_cmdTail = 0;
    g_cmdCount = 0;
    g_eventBits = 0;
}

bool OtaRtosPostCommand(OtaCommand_t cmd)
{
    if (g_cmdCount >= OTA_CMD_QUEUE_DEPTH)
    {
        return false;
    }

    g_cmdQueue[g_cmdTail] = cmd;
    g_cmdTail = (uint8_t)((g_cmdTail + 1U) % OTA_CMD_QUEUE_DEPTH);
    g_cmdCount++;
    return true;
}

bool OtaRtosFetchCommand(OtaCommand_t *cmd)
{
    if (cmd == 0 || g_cmdCount == 0)
    {
        return false;
    }

    *cmd = g_cmdQueue[g_cmdHead];
    g_cmdHead = (uint8_t)((g_cmdHead + 1U) % OTA_CMD_QUEUE_DEPTH);
    g_cmdCount--;
    return true;
}

void OtaRtosSetEvents(uint32_t events)
{
    g_eventBits |= events;
}

void OtaRtosClearEvents(uint32_t events)
{
    g_eventBits &= ~events;
}

uint32_t OtaRtosGetEvents(void)
{
    return g_eventBits;
}

void OtaRtosNetworkTask(void)
{
    OtaCommand_t cmd;

    while (OtaRtosFetchCommand(&cmd))
    {
        if (cmd == OTA_CMD_START || cmd == OTA_CMD_RESUME)
        {
            OtaRtosSetEvents(OTA_EVT_START);
        }
        else if (cmd == OTA_CMD_CANCEL || cmd == OTA_CMD_PAUSE)
        {
            OtaRtosClearEvents(OTA_EVT_START);
        }
    }

    if (OtaRtosGetEvents() & OTA_EVT_START)
    {
        WifiNetworkTask();
    }
}
