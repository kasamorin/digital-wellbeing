#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONFIG_DIR  ".config/digital-wellbeing"
#define CONFIG_FILE "config.json"

/* ---------- defaults ---------- */
#define DEFAULT_WORK_MINUTES           25
#define DEFAULT_BREAK_MINUTES          5
#define DEFAULT_MONITOR_WINDOW_MINUTES 3
#define DEFAULT_IDLE_THRESHOLD_SECONDS 30
#define DEFAULT_NOTIFY_STRATEGY        NOTIFY_SPAM
#define DEFAULT_SPAM_INTERVAL_SECONDS  4
#define DEFAULT_PERSISTENT_TIMEOUT     0

/* ---------- helpers ---------- */
static char *configPath(void)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        home = "/tmp";
    }

    size_t len = strlen(home) + strlen(CONFIG_DIR) + strlen(CONFIG_FILE) + 4;
    char * path = malloc(len);
    if (!path)
    {
        return NULL;
    }
    snprintf(path, len, "%s/%s/%s", home, CONFIG_DIR, CONFIG_FILE);
    return path;
}

static int ensureConfigDir(void)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        return -1;
    }

    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", home, CONFIG_DIR);

    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST)
        {
            perror("mkdir config dir");
            return -1;
        }
    }
    return 0;
}

static const char *notifyStrategyToString(NotificationStrategy s)
{
    switch (s)
    {
    case NOTIFY_SPAM:
        return "spam";
    case NOTIFY_PERSISTENT:
        return "persistent";
    }
    return "spam";
}

static NotificationStrategy notifyStrategyFromString(const char *s)
{
    if (!s)
    {
        return NOTIFY_SPAM;
    }
    if (strcmp(s, "persistent") == 0)
    {
        return NOTIFY_PERSISTENT;
    }
    return NOTIFY_SPAM;
}

/* ---------- default generation ---------- */
static int writeDefaultConfig(const char *path)
{
    struct json_object *root = json_object_new_object();
    if (!root)
    {
        return -1;
    }

    json_object_object_add(root, "workMinutes",
        json_object_new_int(DEFAULT_WORK_MINUTES));
    json_object_object_add(root, "breakMinutes",
        json_object_new_int(DEFAULT_BREAK_MINUTES));
    json_object_object_add(root, "monitorWindowMinutes",
        json_object_new_int(DEFAULT_MONITOR_WINDOW_MINUTES));
    json_object_object_add(root, "idleThresholdSeconds",
        json_object_new_int(DEFAULT_IDLE_THRESHOLD_SECONDS));
    json_object_object_add(root, "lockScreenCommand",
        json_object_new_string(""));
    json_object_object_add(root, "notificationStrategy",
        json_object_new_string("spam"));
    json_object_object_add(root, "spamIntervalSeconds",
        json_object_new_int(DEFAULT_SPAM_INTERVAL_SECONDS));
    json_object_object_add(root, "persistentNotificationTimeout",
        json_object_new_int(DEFAULT_PERSISTENT_TIMEOUT));

    const char *jsonStr =
        json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    FILE *f = fopen(path, "w");
    if (!f)
    {
        perror("fopen config write");
        json_object_put(root);
        return -1;
    }

    fprintf(f, "%s\n", jsonStr);
    fclose(f);
    json_object_put(root);

    printf("Wrote default config to %s\n", path);
    return 0;
}

/* ---------- parse helpers ---------- */
static uint32_t jsonGetUint(struct json_object *obj, const char *key,
    uint32_t fallback)
{
    struct json_object *val =
        json_object_object_get(obj, key);
    if (!val)
    {
        return fallback;
    }
    int32_t i = json_object_get_int(val);
    if (i < 0)
    {
        i = 0;
    }
    return (uint32_t)i;
}

static bool jsonGetString(struct json_object *obj, const char *key,
    char **out)
{
    struct json_object *val =
        json_object_object_get(obj, key);
    if (!val || !json_object_is_type(val, json_type_string))
    {
        return false;
    }
    const char *s = json_object_get_string(val);
    *out = s ? strdup(s) : NULL;
    return true;
}

/* ---------- public API ---------- */
Config *configLoad(void)
{
    if (ensureConfigDir() != 0)
    {
        fprintf(stderr, "Failed to create config directory\n");
        return NULL;
    }

    char *path = configPath();
    if (!path)
    {
        fprintf(stderr, "Failed to build config path\n");
        return NULL;
    }

    /* auto-generate if missing */
    if (access(path, F_OK) != 0)
    {
        if (writeDefaultConfig(path) != 0)
        {
            free(path);
            return NULL;
        }
    }

    struct json_object *root = json_object_from_file(path);
    if (!root)
    {
        fprintf(stderr, "Failed to parse %s: %s\n", path,
            json_util_get_last_err());
        free(path);
        return NULL;
    }

    Config *cfg = calloc(1, sizeof(Config));
    if (!cfg)
    {
        fprintf(stderr, "Out of memory allocating Config\n");
        json_object_put(root);
        free(path);
        return NULL;
    }

    cfg->workMinutes =
        jsonGetUint(root, "workMinutes", DEFAULT_WORK_MINUTES);
    cfg->breakMinutes =
        jsonGetUint(root, "breakMinutes", DEFAULT_BREAK_MINUTES);
    cfg->monitorWindowMinutes = jsonGetUint(root, "monitorWindowMinutes",
        DEFAULT_MONITOR_WINDOW_MINUTES);
    cfg->idleThresholdSeconds = jsonGetUint(root, "idleThresholdSeconds",
        DEFAULT_IDLE_THRESHOLD_SECONDS);

    /* lockScreenCommand: keep NULL if empty string */
    char *lockCmd = NULL;
    if (jsonGetString(root, "lockScreenCommand", &lockCmd) && lockCmd
        && lockCmd[0] == '\0')
    {
        free(lockCmd);
        lockCmd = NULL;
    }
    cfg->lockScreenCommand = lockCmd;

    char *strat = NULL;
    if (jsonGetString(root, "notificationStrategy", &strat))
    {
        cfg->notificationStrategy = notifyStrategyFromString(strat);
        free(strat);
    }
    else
    {
        cfg->notificationStrategy = DEFAULT_NOTIFY_STRATEGY;
    }

    cfg->spamIntervalSeconds = jsonGetUint(root, "spamIntervalSeconds",
        DEFAULT_SPAM_INTERVAL_SECONDS);
    cfg->persistentNotificationTimeout = jsonGetUint(root,
        "persistentNotificationTimeout", DEFAULT_PERSISTENT_TIMEOUT);

    json_object_put(root);
    free(path);
    return cfg;
}

int configSave(const Config *cfg)
{
    if (!cfg)
    {
        return -1;
    }

    char *path = configPath();
    if (!path)
    {
        return -1;
    }

    struct json_object *root = json_object_new_object();
    if (!root)
    {
        free(path);
        return -1;
    }

    json_object_object_add(root, "workMinutes",
        json_object_new_int((int32_t)cfg->workMinutes));
    json_object_object_add(root, "breakMinutes",
        json_object_new_int((int32_t)cfg->breakMinutes));
    json_object_object_add(root, "monitorWindowMinutes",
        json_object_new_int((int32_t)cfg->monitorWindowMinutes));
    json_object_object_add(root, "idleThresholdSeconds",
        json_object_new_int((int32_t)cfg->idleThresholdSeconds));
    json_object_object_add(root, "lockScreenCommand",
        json_object_new_string(
            cfg->lockScreenCommand ? cfg->lockScreenCommand : ""));
    json_object_object_add(root, "notificationStrategy",
        json_object_new_string(
            notifyStrategyToString(cfg->notificationStrategy)));
    json_object_object_add(root, "spamIntervalSeconds",
        json_object_new_int((int32_t)cfg->spamIntervalSeconds));
    json_object_object_add(root, "persistentNotificationTimeout",
        json_object_new_int(
            (int32_t)cfg->persistentNotificationTimeout));

    const char *jsonStr =
        json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    FILE *f = fopen(path, "w");
    if (!f)
    {
        perror("fopen config save");
        json_object_put(root);
        free(path);
        return -1;
    }

    fprintf(f, "%s\n", jsonStr);
    fclose(f);
    json_object_put(root);
    free(path);
    return 0;
}

void configFree(Config *cfg)
{
    if (!cfg)
    {
        return;
    }
    free(cfg->lockScreenCommand);
    free(cfg);
}