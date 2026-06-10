#include "app_config.h"

static AppConfig_t g_appConfig;

void AppConfigInit(void)
{
    g_appConfig.wifiSsid = "pratol";
    g_appConfig.wifiPassword = "123456789";

    g_appConfig.otaHost = "iot-api.heclouds.com";
    g_appConfig.otaPort = 80;
    g_appConfig.productId = "KPw28SAyeA";
    g_appConfig.deviceName = "board1";
    g_appConfig.authorization = "version=2018-10-31&res=products%2FKPw28SAyeA%2Fdevices%2Fboard1&et=1800523492&method=md5&sign=NUF4HQG2vJ6iZK281Ugp5g%3D%3D";
    g_appConfig.firmwareType = "2";

    g_appConfig.atTimeoutShortMs = 500;
    g_appConfig.atTimeoutNormalMs = 5000;
    g_appConfig.atTimeoutLongMs = 15000;
}

const AppConfig_t *AppConfigGet(void)
{
    return &g_appConfig;
}
