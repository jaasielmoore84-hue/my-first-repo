#include <string.h>
#include <stdlib.h>
#include "http_parser.h"

static int ToLowerChar(int ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return ch - 'A' + 'a';
    }
    return ch;
}

static bool MemEqualNoCase(const uint8_t *buf, const char *str, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        if (ToLowerChar(buf[i]) != ToLowerChar(str[i]))
        {
            return false;
        }
    }
    return true;
}

static int MemFind(const uint8_t *buf, uint32_t len, const char *pat, uint32_t patLen)
{
    uint32_t i;

    if (patLen == 0 || len < patLen)
    {
        return -1;
    }

    for (i = 0; i <= len - patLen; i++)
    {
        if (memcmp(&buf[i], pat, patLen) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

static int MemFindNoCase(const uint8_t *buf, uint32_t len, const char *pat)
{
    uint32_t i;
    uint32_t patLen = strlen(pat);

    if (patLen == 0 || len < patLen)
    {
        return -1;
    }

    for (i = 0; i <= len - patLen; i++)
    {
        if (MemEqualNoCase(&buf[i], pat, patLen))
        {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t ParseUint(const uint8_t *buf, uint32_t len)
{
    uint32_t val = 0;
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        if (buf[i] < '0' || buf[i] > '9')
        {
            break;
        }
        val = val * 10 + (uint32_t)(buf[i] - '0');
    }
    return val;
}

static bool ParseHeaderUint(const uint8_t *header, uint32_t headerLen, const char *name, uint32_t *value)
{
    int pos = MemFindNoCase(header, headerLen, name);
    const uint8_t *p;
    const uint8_t *end = header + headerLen;

    if (pos < 0)
    {
        return false;
    }

    p = header + pos + strlen(name);
    while (p < end && (*p == ' ' || *p == '\t' || *p == ':'))
    {
        p++;
    }

    *value = ParseUint(p, (uint32_t)(end - p));
    return true;
}

static void ParseContentRange(const uint8_t *header, uint32_t headerLen, HttpResponse_t *rsp)
{
    int pos = MemFindNoCase(header, headerLen, "Content-Range:");
    const uint8_t *p;
    const uint8_t *end = header + headerLen;

    if (pos < 0)
    {
        return;
    }

    p = header + pos + strlen("Content-Range:");
    while (p < end && *p != ' ')
    {
        p++;
    }
    while (p < end && *p == ' ')
    {
        p++;
    }

    rsp->rangeStart = ParseUint(p, (uint32_t)(end - p));
    while (p < end && *p != '-')
    {
        p++;
    }
    if (p >= end)
    {
        return;
    }
    p++;
    rsp->rangeEnd = ParseUint(p, (uint32_t)(end - p));

    while (p < end && *p != '/')
    {
        p++;
    }
    if (p >= end)
    {
        return;
    }
    p++;
    rsp->rangeTotal = ParseUint(p, (uint32_t)(end - p));
    rsp->hasContentRange = true;
}

bool HttpParseResponse(const uint8_t *buf, uint32_t len, HttpResponse_t *rsp)
{
    int httpPos;
    int headerEnd;
    const uint8_t *header;
    uint32_t headerLen;

    if (buf == 0 || rsp == 0)
    {
        return false;
    }

    memset(rsp, 0, sizeof(HttpResponse_t));

    httpPos = MemFind(buf, len, "HTTP/", 5);
    if (httpPos < 0)
    {
        return false;
    }

    headerEnd = MemFind(&buf[httpPos], len - (uint32_t)httpPos, "\r\n\r\n", 4);
    if (headerEnd < 0)
    {
        return false;
    }

    rsp->headerOffset = (uint32_t)httpPos;
    rsp->headerLength = (uint32_t)headerEnd + 4;
    rsp->bodyOffset = rsp->headerOffset + rsp->headerLength;
    rsp->bodyLengthInPacket = (rsp->bodyOffset <= len) ? (len - rsp->bodyOffset) : 0;

    if ((uint32_t)httpPos + 12 > len)
    {
        return false;
    }
    rsp->statusCode = (uint16_t)ParseUint(&buf[httpPos + 9], 3);

    header = &buf[rsp->headerOffset];
    headerLen = rsp->headerLength;

    rsp->hasContentLength = ParseHeaderUint(header, headerLen, "Content-Length:", &rsp->contentLength);
    ParseContentRange(header, headerLen, rsp);
    rsp->isChunked = (MemFindNoCase(header, headerLen, "Transfer-Encoding: chunked") >= 0);

    if (rsp->hasContentLength && rsp->contentLength < rsp->bodyLengthInPacket)
    {
        rsp->bodyLengthInPacket = rsp->contentLength;
    }

    return true;
}

bool HttpValidateRangeResponse(const HttpResponse_t *rsp, uint32_t expectedStart, uint32_t expectedLen)
{
    if (rsp == 0 || rsp->isChunked)
    {
        return false;
    }

    if (expectedStart == 0 && rsp->statusCode == 200)
    {
        return (!rsp->hasContentLength || rsp->contentLength >= expectedLen);
    }

    if (rsp->statusCode != 206 || !rsp->hasContentRange)
    {
        return false;
    }

    if (rsp->rangeStart != expectedStart)
    {
        return false;
    }

    if ((rsp->rangeEnd - rsp->rangeStart + 1) < expectedLen)
    {
        return false;
    }

    return true;
}
