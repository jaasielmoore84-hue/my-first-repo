#ifndef _APP_LOG_H_
#define _APP_LOG_H_

#include <stdio.h>

typedef enum
{
    APP_LOG_LEVEL_ERROR = 0,
    APP_LOG_LEVEL_WARN,
    APP_LOG_LEVEL_INFO,
    APP_LOG_LEVEL_DEBUG,
    APP_LOG_LEVEL_TRACE
} AppLogLevel_t;

#ifndef APP_LOG_LEVEL
#define APP_LOG_LEVEL APP_LOG_LEVEL_INFO
#endif

#define APP_LOGE(fmt, ...) do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_ERROR) printf("[E] " fmt "\r\n", ##__VA_ARGS__); } while (0)
#define APP_LOGW(fmt, ...) do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_WARN)  printf("[W] " fmt "\r\n", ##__VA_ARGS__); } while (0)
#define APP_LOGI(fmt, ...) do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_INFO)  printf("[I] " fmt "\r\n", ##__VA_ARGS__); } while (0)
#define APP_LOGD(fmt, ...) do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_DEBUG) printf("[D] " fmt "\r\n", ##__VA_ARGS__); } while (0)
#define APP_LOGT(fmt, ...) do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_TRACE) printf("[T] " fmt "\r\n", ##__VA_ARGS__); } while (0)

#endif
