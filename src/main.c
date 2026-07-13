#include <stdio.h>
#include <stdlib.h>

#include "config.h"

int main(void)
{
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

    configFree(cfg);
    return EXIT_SUCCESS;
}