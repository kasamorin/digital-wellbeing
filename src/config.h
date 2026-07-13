#ifndef CONFIG_H
#define CONFIG_H

#include <json-c/json.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    NOTIFY_SPAM,
    NOTIFY_PERSISTENT
} NotificationStrategy;

typedef struct
{
    uint32_t workMinutes;
    uint32_t breakMinutes;
    uint32_t monitorWindowMinutes;
    uint32_t idleThresholdSeconds;
    char *   lockScreenCommand;
    NotificationStrategy notificationStrategy;
    uint32_t spamIntervalSeconds;
    uint32_t persistentNotificationTimeout;
} Config;

/*
 * Load config from ~/.config/digital-wellbeing/config.json.
 * If the file does not exist, generate a default one first.
 * Returns NULL and prints to stderr on fatal parse errors.
 * Caller owns the returned Config; free with configFree().
 */
Config *configLoad(void);

/*
 * Write in-memory Config back to the JSON file.
 * Returns 0 on success, -1 on failure.
 */
int configSave(const Config *cfg);

/*
 * Free a Config struct and its owned strings.
 */
void configFree(Config *cfg);

#endif