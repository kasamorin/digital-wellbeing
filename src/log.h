#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Print a timestamped message to stderr.
 *
 * Format: [HH:MM:SS] message\n
 *
 * The format string is translatable — wrap with _() at call sites.
 * A trailing newline is appended automatically.
 */
void logMsg(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif