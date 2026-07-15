#include <QApplication>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "config.h"
#include "service.h"
#include "breakWindow.h"
}

/* ---------- graceful shutdown ---------- */
static volatile sig_atomic_t gRunning = 1;

static void signalHandler(int sig)
{
    (void)sig;
    gRunning = 0;
}

static void installSignals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signalHandler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

/* ---------- daemonize ---------- */
static int daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return -1;
    }
    if (pid > 0)
    {
        /* Parent exits, child continues */
        fprintf(stderr,
            "daemon: forked child pid %d, exiting parent\n",
            pid);
        _exit(0);
    }

    /* Child */
    if (setsid() < 0)
    {
        perror("setsid");
        return -1;
    }

    /* Redirect stdin/stdout/stderr to /dev/null */
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0)
    {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
        {
            close(fd);
        }
    }

    return 0;
}

/* ---------- notification helper ---------- */
static void sendNotification(const char *message, int timeoutMs)
{
    char cmd[1024];
    int wrote = snprintf(cmd, sizeof(cmd),
        "notify-send -t %d 'Digital Wellbeing' '%s'",
        timeoutMs, message);
    if (wrote < 0 || (size_t)wrote >= sizeof(cmd))
    {
        return;
    }
    system(cmd);  // fire-and-forget, don't block on return
}

/* ---------- main loop ---------- */
static void mainLoop(const Config *cfg)
{
    while (gRunning)
    {
        uint32_t preNotifySec =
            (cfg->workMinutes > 5)
                ? (cfg->workMinutes - 5) * 60u
                : 0u;
        uint32_t remainingWorkSec =
            (cfg->workMinutes > 5) ? 5u * 60u
                                   : cfg->workMinutes * 60u;

        if (preNotifySec > 0)
        {
            for (uint32_t s = 0; s < preNotifySec && gRunning; s++)
            {
                sleep(1);
            }
            if (!gRunning)
            {
                break;
            }
        }

        if (cfg->workMinutes > 5)
        {
            sendNotification("5 minutes until break time", 30000);
        }

        for (uint32_t s = 0; s < remainingWorkSec && gRunning; s++)
        {
            sleep(1);
        }
        if (!gRunning)
        {
            break;
        }

        breakWindowShow((int)cfg->breakMinutes * 60);
    }
}

/* ---------- entry point ---------- */
int main(int argc, char *argv[])
{
    bool foreground = false;

    /* Parse KISS args */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--fg") == 0
            || strcmp(argv[i], "-f") == 0)
        {
            foreground = true;
        }
    }

    /* --- config --- */
    Config *cfg = configLoad();
    if (!cfg)
    {
        fprintf(stderr, "configLoad failed\n");
        return EXIT_FAILURE;
    }

    fprintf(stderr, "digital-wellbeing: starting daemon\n");
    fprintf(stderr, "  work=%umin break=%umin\n",
        cfg->workMinutes, cfg->breakMinutes);

    /* --- service self-install (first run only) --- */
    if (serviceInstall() != 0)
    {
        fprintf(stderr, "serviceInstall failed (non-fatal, continuing)\n");
    }

    /* --- daemonize (unless --fg) --- */
    if (!foreground)
    {
        if (daemonize() != 0)
        {
            fprintf(stderr, "daemonize failed\n");
            configFree(cfg);
            return EXIT_FAILURE;
        }
    }

    /* --- init Qt (AFTER fork, so child gets a fresh QApplication) --- */
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    /* --- signals --- */
    installSignals();

    /* --- main loop --- */
    mainLoop(cfg);

    configFree(cfg);
    return EXIT_SUCCESS;
}