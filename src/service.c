#include "service.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SERVICE_DIR  ".config/systemd/user"
#define SERVICE_FILE "digital-wellbeing.service"

/*
 * Built-in service unit.
 * %h is expanded by systemd to the user's home directory.
 * The binary is installed to ~/.local/bin/ per KISS: no root needed.
 */
static const char *kServiceUnit =
    "[Unit]\n"
    "Description=Digital Wellbeing — pomodoro-style break reminder\n"
    "\n"
    "[Service]\n"
    "Type=simple\n"
    "ExecStart=%h/.local/bin/digital-wellbeing\n"
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
            fprintf(stderr,
                "service: failed to create dir %s\n", dir);
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
        fprintf(stderr,
            "service: cannot ensure service directory\n");
        return -1;
    }

    char *path = servicePath();
    if (!path)
    {
        fprintf(stderr,
            "service: cannot build service path\n");
        return -1;
    }

    /* Check if already installed */
    if (access(path, F_OK) == 0)
    {
        fprintf(stderr,
            "service: %s already exists, skipping install\n",
            path);
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

    fprintf(f, "%s", kServiceUnit);
    fclose(f);

    fprintf(stderr,
        "service: installed %s\n", path);
    free(path);

    fprintf(stderr,
        "service: run 'systemctl --user enable "
        "digital-wellbeing.service' to start on login\n");

    return 0;
}