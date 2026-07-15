#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct
{
    uint32_t workMinutes;
    uint32_t breakMinutes;
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
 * Free a Config struct.
 */
void configFree(Config *cfg);

#endif