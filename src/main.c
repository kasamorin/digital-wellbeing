#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "idle.h"

int main(void)
{
    /* --- config --- */
    Config *cfg = configLoad();
    if (!cfg)
    {
        fprintf(stderr, "configLoad failed\n");
        return EXIT_FAILURE;
    }

    printf("=== Digital Wellbeing Config ===\n");
    printf("workMinutes               : %u\n", cfg->workMinutes);
    printf("breakMinutes              : %u\n", cfg->breakMinutes);
    printf("monitorWindowMinutes      : %u\n", cfg->monitorWindowMinutes);
    printf("idleThresholdSeconds      : %u\n", cfg->idleThresholdSeconds);
    printf("lockScreenCommand         : %s\n",
        cfg->lockScreenCommand ? cfg->lockScreenCommand : "(auto)");
    printf("notificationStrategy      : %s\n",
        cfg->notificationStrategy == NOTIFY_SPAM ? "spam" : "persistent");
    printf("spamIntervalSeconds       : %u\n", cfg->spamIntervalSeconds);
    printf("persistentNotificationTimeout: %u\n",
        cfg->persistentNotificationTimeout);

    /* --- idle detection --- */
    IdleBackend backend = idleInit();
    if (backend == IDLE_BACKEND_NONE)
    {
        fprintf(stderr, "Cannot detect idle time — no backend available\n");
        configFree(cfg);
        return EXIT_FAILURE;
    }

    printf("\n=== Idle Detection Test ===\n");
    printf("Backend: %s\n", idleBackendName());
    printf("Polling idle time for 5 seconds (move the mouse to see changes):\n");

    for (int i = 0; i < 5; i++)
    {
        uint64_t ms = idleGetMs();
        if (ms == UINT64_MAX)
        {
            printf("  [%d] error reading idle time\n", i + 1);
        }
        else
        {
            printf("  [%d] idle = %lu ms (%.1f s)\n", i + 1,
                (unsigned long)ms, ms / 1000.0);
        }
        fflush(stdout);
        sleep(1);
    }

    idleShutdown();
    configFree(cfg);
    return EXIT_SUCCESS;
}