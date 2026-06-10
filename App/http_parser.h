#ifndef _HTTP_PARSER_H_
#define _HTTP_PARSER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint16_t statusCode;
    uint32_t headerOffset;
    uint32_t headerLength;
    uint32_t bodyOffset;
    uint32_t bodyLengthInPacket;

    bool hasContentLength;
    uint32_t contentLength;

    bool hasContentRange;
    uint32_t rangeStart;
    uint32_t rangeEnd;
    uint32_t rangeTotal;

    bool isChunked;
} HttpResponse_t;

bool HttpParseResponse(const uint8_t *buf, uint32_t len, HttpResponse_t *rsp);
bool HttpValidateRangeResponse(const HttpResponse_t *rsp, uint32_t expectedStart, uint32_t expectedLen);

#endif
