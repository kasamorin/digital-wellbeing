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
#include "breakWindow.h"
#include "service.h"
#include "log.h"
#include "i18n.h"
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
        logMsg(_("daemon: forked child pid %d, exiting parent"),
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

        logMsg(_("Work period started — %u minutes"),
            cfg->workMinutes);

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
            sendNotification(_("5 minutes until break time"), 30000);
        }

        for (uint32_t s = 0; s < remainingWorkSec && gRunning; s++)
        {
            sleep(1);
        }
        if (!gRunning)
        {
            break;
        }

        logMsg(_("Break period started — %u minutes"),
            cfg->breakMinutes);

        bool skipped = breakWindowShow(
            (int)cfg->breakMinutes * 60);

        if (!gRunning)
        {
            break;
        }

        logMsg(skipped
            ? _("Break skipped, starting new work period")
            : _("Break completed, starting new work period"));
    }
}

/* ---------- usage ---------- */
static void printUsage(void)
{
    fprintf(stderr,
        "Usage: digital-wellbeing [OPTION]\n"
        "       digital-wellbeing install\n"
        "\n"
        "Run a pomodoro-style break reminder.\n"
        "\n"
        "Options:\n"
        "  -d, --daemon    Run in background (fork and detach)\n"
        "  -h, --help      Show this help message\n"
        "\n"
        "Commands:\n"
        "  install         Install systemd user service file and exit\n");
}

/* ---------- entry point ---------- */
int main(int argc, char *argv[])
{
    /* ---- subcommand: install ---- */
    if (argc > 1 && strcmp(argv[1], "install") == 0)
    {
        i18nInit();
        if (serviceInstall() != 0)
        {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    /* ---- parse flags ---- */
    bool daemonMode = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--daemon") == 0
            || strcmp(argv[i], "-d") == 0)
        {
            daemonMode = true;
        }
        else if (strcmp(argv[i], "--help") == 0
            || strcmp(argv[i], "-h") == 0)
        {
            printUsage();
            return EXIT_SUCCESS;
        }
        else if (strcmp(argv[i], "--fg") == 0
            || strcmp(argv[i], "-f") == 0)
        {
            /* Accept but warn — foreground is now the default */
            logMsg(_("--fg/-f is deprecated; "
                "foreground is now the default mode"));
        }
    }

    /* ---- i18n ---- */
    i18nInit();

    /* ---- config ---- */
    Config *cfg = configLoad();
    if (!cfg)
    {
        logMsg(_("configLoad failed"));
        return EXIT_FAILURE;
    }

    logMsg(_("Starting digital-wellbeing — "
        "%u min work, %u min break"),
        cfg->workMinutes, cfg->breakMinutes);

    /* ---- daemonize (only when --daemon) ---- */
    if (daemonMode)
    {
        if (daemonize() != 0)
        {
            logMsg(_("daemonize failed"));
            configFree(cfg);
            return EXIT_FAILURE;
        }
    }

    /* ---- init Qt (AFTER fork, so child gets fresh QApplication) ---- */
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    /* ---- signals ---- */
    installSignals();

    /* ---- main loop ---- */
    mainLoop(cfg);

    configFree(cfg);
    return EXIT_SUCCESS;
}