#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void logMsg(const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    fprintf(stderr, "[%02d:%02d:%02d] ",
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}