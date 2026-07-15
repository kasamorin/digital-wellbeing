#include "service.h"
#include "log.h"
#include "i18n.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SERVICE_DIR  ".config/systemd/user"
#define SERVICE_FILE "digital-wellbeing.service"

/*
 * Built-in service unit format.
 * %%h is expanded by systemd to the user's home directory.
 * The Description is translated at write time via _().
 * The binary is installed to ~/.local/bin/ per KISS: no root needed.
 */
static const char *kServiceUnitFormat =
    "[Unit]\n"
    "Description=%s\n"
    "\n"
    "[Service]\n"
    "Type=simple\n"
    "ExecStart=%%h/.local/bin/digital-wellbeing\n"
    "Restart=on-failure\n"
    "RestartSec=10\n"
    "\n"
    "[Install]\n"
    "WantedBy=default.target\n";

/* ---------- helpers ---------- */
static int ensureServiceDir(void)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        return -1;
    }

    char dir[1024];
    int wrote = snprintf(dir, sizeof(dir), "%s/%s", home, SERVICE_DIR);
    if (wrote < 0 || (size_t)wrote >= sizeof(dir))
    {
        return -1;
    }

    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        /* recursive mkdir for .config/systemd/user */
        char cmd[2048];
        wrote = snprintf(cmd, sizeof(cmd),
            "mkdir -p \"%s\"", dir);
        if (wrote < 0 || (size_t)wrote >= sizeof(cmd))
        {
            return -1;
        }
        if (system(cmd) != 0)
        {
            logMsg(_("Failed to create directory %s"), dir);
            return -1;
        }
    }
    return 0;
}

static char *servicePath(void)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        return NULL;
    }

    size_t len = strlen(home) + strlen(SERVICE_DIR)
        + strlen(SERVICE_FILE) + 4;
    char *path = malloc(len);
    if (!path)
    {
        return NULL;
    }
    snprintf(path, len, "%s/%s/%s",
        home, SERVICE_DIR, SERVICE_FILE);
    return path;
}

/* ---------- public ---------- */
int serviceInstall(void)
{
    if (ensureServiceDir() != 0)
    {
        logMsg(_("Cannot ensure service directory"));
        return -1;
    }

    char *path = servicePath();
    if (!path)
    {
        logMsg(_("Cannot build service path"));
        return -1;
    }

    /* Check if already installed */
    if (access(path, F_OK) == 0)
    {
        logMsg(_("%s already exists, skipping install"), path);
        free(path);
        return 0;
    }

    FILE *f = fopen(path, "w");
    if (!f)
    {
        perror("service: fopen service file");
        free(path);
        return -1;
    }

    fprintf(f, kServiceUnitFormat,
        _("Digital Wellbeing — pomodoro-style break reminder"));
    fclose(f);

    logMsg(_("Installed %s"), path);
    free(path);

    logMsg(_("Run 'systemctl --user enable "
        "digital-wellbeing.service' to start on login"));

    return 0;
}