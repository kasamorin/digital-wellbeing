#include "idle.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/input.h>

#ifdef HAVE_WAYLAND
#include <wayland-client.h>
#include "ext-idle-notify-v1.h"
#endif

#include <dbus/dbus.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/Xlib.h>

/* ---------- backend state ---------- */
static IdleBackend gBackend = IDLE_BACKEND_NONE;

/* X11 */
static Display *          xDisplay   = NULL;
static XScreenSaverInfo * xSaverInfo = NULL;

/* Wayland */
#ifdef HAVE_WAYLAND
static struct wl_display *            wlDisplay      = NULL;
static struct wl_registry *           wlRegistry     = NULL;
static struct ext_idle_notifier_v1 *  wlIdleNotifier = NULL;
static struct ext_idle_notification_v1 *wlIdleNotif  = NULL;
static struct wl_seat *               wlSeat         = NULL;
static uint64_t                       wlIdleTimeMs   = 0;
static bool                           wlIsIdle       = false;
static bool                           wlHasNotifier  = false;
#endif

/* /dev/input */
static int *  inputFds      = NULL;
static int    inputFdCount  = 0;

/* D-Bus logind */
static DBusConnection *dbusConn        = NULL;
static char *          dbusSessionPath = NULL;

/* ---------- auto-detect ---------- */
static IdleBackend detectBackend(void)
{
    const char *sessionType = getenv("XDG_SESSION_TYPE");
    if (!sessionType)
    {
        if (getenv("DISPLAY"))
        {
            return IDLE_BACKEND_X11;
        }
        if (getenv("WAYLAND_DISPLAY"))
        {
            return IDLE_BACKEND_WAYLAND;
        }
        return IDLE_BACKEND_NONE;
    }

    if (strcmp(sessionType, "wayland") == 0)
    {
        return IDLE_BACKEND_WAYLAND;
    }
    if (strcmp(sessionType, "x11") == 0)
    {
        return IDLE_BACKEND_X11;
    }
    return IDLE_BACKEND_NONE;
}

/* ---------- X11 backend ---------- */
static bool x11Init(void)
{
    xDisplay = XOpenDisplay(NULL);
    if (!xDisplay)
    {
        fprintf(stderr, "idle/x11: failed to open display\n");
        return false;
    }

    int eventBase = 0, errorBase = 0;
    if (!XScreenSaverQueryExtension(xDisplay, &eventBase, &errorBase))
    {
        fprintf(stderr,
            "idle/x11: XScreenSaver extension not available\n");
        XCloseDisplay(xDisplay);
        xDisplay = NULL;
        return false;
    }

    xSaverInfo = XScreenSaverAllocInfo();
    if (!xSaverInfo)
    {
        fprintf(stderr,
            "idle/x11: failed to allocate XScreenSaverInfo\n");
        XCloseDisplay(xDisplay);
        xDisplay = NULL;
        return false;
    }

    return true;
}

static uint64_t x11GetMs(void)
{
    if (!xDisplay || !xSaverInfo)
    {
        return UINT64_MAX;
    }

    XScreenSaverQueryInfo(xDisplay, DefaultRootWindow(xDisplay),
        xSaverInfo);
    return (uint64_t)xSaverInfo->idle;
}

static void x11Shutdown(void)
{
    if (xSaverInfo)
    {
        XFree(xSaverInfo);
        xSaverInfo = NULL;
    }
    if (xDisplay)
    {
        XCloseDisplay(xDisplay);
        xDisplay = NULL;
    }
}

/* ---------- Wayland backend ---------- */
#ifdef HAVE_WAYLAND
static void wlRegistryHandler(void *data, struct wl_registry *registry,
    uint32_t id, const char *interface, uint32_t version)
{
    (void)data;

    if (strcmp(interface, "ext_idle_notifier_v1") == 0)
    {
        wlIdleNotifier = wl_registry_bind(registry, id,
            &ext_idle_notifier_v1_interface,
            version < 1 ? version : 1);
        wlHasNotifier = true;
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        wlSeat = wl_registry_bind(registry, id,
            &wl_seat_interface,
            version < 5 ? version : 5);
    }
}

static void wlRegistryRemover(void *data, struct wl_registry *registry,
    uint32_t id)
{
    (void)data;
    (void)registry;
    (void)id;
}

static const struct wl_registry_listener wlRegistryListener = {
    .global = wlRegistryHandler,
    .global_remove = wlRegistryRemover,
};

static void wlIdleNotifIdled(void *data,
    struct ext_idle_notification_v1 *notif)
{
    (void)notif;
    bool *flag = data;
    *flag = true;
}

static void wlIdleNotifResumed(void *data,
    struct ext_idle_notification_v1 *notif)
{
    (void)notif;
    bool *flag = data;
    *flag = false;
}

static const struct ext_idle_notification_v1_listener wlIdleNotifListener = {
    .idled = wlIdleNotifIdled,
    .resumed = wlIdleNotifResumed,
};

static void wlShutdown(void);

static bool wlInit(void)
{
    wlDisplay = wl_display_connect(NULL);
    if (!wlDisplay)
    {
        fprintf(stderr, "idle/wayland: failed to connect to display\n");
        return false;
    }

    wlRegistry = wl_display_get_registry(wlDisplay);
    if (!wlRegistry)
    {
        fprintf(stderr,
            "idle/wayland: failed to get registry\n");
        wl_display_disconnect(wlDisplay);
        wlDisplay = NULL;
        return false;
    }

    wl_registry_add_listener(wlRegistry, &wlRegistryListener, NULL);

    wl_display_roundtrip(wlDisplay);

    if (!wlHasNotifier || !wlIdleNotifier)
    {
        fprintf(stderr,
            "idle/wayland: ext_idle_notifier_v1 not available\n");
        wlShutdown();
        return false;
    }

    wlIdleNotif = ext_idle_notifier_v1_get_idle_notification(
        wlIdleNotifier, 1000);
    if (!wlIdleNotif)
    {
        fprintf(stderr,
            "idle/wayland: failed to create idle notification\n");
        wlShutdown();
        return false;
    }

    ext_idle_notification_v1_add_listener(wlIdleNotif,
        &wlIdleNotifListener, &wlIsIdle);

    wl_display_roundtrip(wlDisplay);

    return true;
}

static uint64_t wlGetMs(void)
{
    if (!wlDisplay)
    {
        return UINT64_MAX;
    }

    wl_display_dispatch_pending(wlDisplay);

    if (wlIsIdle)
    {
        wlIdleTimeMs += 1000;
        return wlIdleTimeMs;
    }

    wlIdleTimeMs = 0;
    return 0;
}

static void wlShutdown(void)
{
    if (wlIdleNotif)
    {
        ext_idle_notification_v1_destroy(wlIdleNotif);
        wlIdleNotif = NULL;
    }
    if (wlIdleNotifier)
    {
        ext_idle_notifier_v1_destroy(wlIdleNotifier);
        wlIdleNotifier = NULL;
    }
    if (wlSeat)
    {
        wl_seat_destroy(wlSeat);
        wlSeat = NULL;
    }
    if (wlRegistry)
    {
        wl_registry_destroy(wlRegistry);
        wlRegistry = NULL;
    }
    if (wlDisplay)
    {
        wl_display_disconnect(wlDisplay);
        wlDisplay = NULL;
    }
    wlHasNotifier = false;
}
#endif /* HAVE_WAYLAND */

/* ---------- /dev/input backend ---------- */

/*
 * Check whether a device supports a given event type via EVIOCGBIT.
 * Returns true if the evbit includes the event type mask.
 */
static bool inputHasEvent(int fd, uint8_t evType)
{
    unsigned long evbits[(EV_MAX + sizeof(unsigned long) * 8 - 1)
        / (sizeof(unsigned long) * 8)];
    memset(evbits, 0, sizeof(evbits));

    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits) < 0)
    {
        return false;
    }

    return !!(evbits[evType / (sizeof(unsigned long) * 8)]
        & (1UL << (evType % (sizeof(unsigned long) * 8))));
}

/*
 * Open all /dev/input/event* devices that are keyboards or mice.
 * Keyboards: support EV_KEY
 * Mice/touchpads/trackpoints: support EV_REL or EV_ABS
 */
static void inputShutdown(void);

static bool inputInit(void)
{
    DIR *dir = opendir("/dev/input");
    if (!dir)
    {
        fprintf(stderr, "idle/input: cannot open /dev/input: %s\n",
            strerror(errno));
        return false;
    }

    /* First pass: count matching devices */
    int capacity = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "event", 5) != 0)
        {
            continue;
        }

        char path[PATH_MAX];
        int wrote = snprintf(path, sizeof(path),
            "/dev/input/%s", entry->d_name);
        if (wrote < 0 || (size_t)wrote >= sizeof(path))
        {
            /* Name too long — skip this device */
            continue;
        }

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
        {
            /* Permission denied — non-fatal, skip this device */
            continue;
        }

        bool isKbd = inputHasEvent(fd, EV_KEY);
        bool isMouse = inputHasEvent(fd, EV_REL)
                    || inputHasEvent(fd, EV_ABS);

        if (isKbd || isMouse)
        {
            if (inputFdCount >= capacity)
            {
                int newCap = capacity == 0 ? 16 : capacity * 2;
                int *newFds = realloc(inputFds,
                    (size_t)newCap * sizeof(int));
                if (!newFds)
                {
                    close(fd);
                    closedir(dir);
                    fprintf(stderr,
                        "idle/input: out of memory\n");
                    inputShutdown();
                    return false;
                }
                inputFds = newFds;
                capacity = newCap;
            }
            inputFds[inputFdCount++] = fd;
        }
        else
        {
            close(fd);
        }
    }
    closedir(dir);

    if (inputFdCount == 0)
    {
        fprintf(stderr,
            "idle/input: no keyboard/mouse devices found "
            "(check input group membership)\n");
        return false;
    }

    fprintf(stderr, "idle/input: opened %d input device(s)\n",
        inputFdCount);
    return true;
}

/*
 * Read pending events from each device.
 * We drain all readable events per device: if any activity is seen,
 * we record the current monotonic time as the last activity timestamp.
 * Returns idle time = now - lastActivityTime in milliseconds.
 */
static uint64_t inputGetMs(void)
{
    if (inputFdCount == 0 || !inputFds)
    {
        return UINT64_MAX;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t nowMs = (uint64_t)now.tv_sec * 1000
        + (uint64_t)now.tv_nsec / 1000000;

    /* Initialize baseline on first call so idle starts from zero. */
    static uint64_t sLastEventMs = 0;
    static bool     sHaveLastEvent = false;

    if (!sHaveLastEvent)
    {
        sLastEventMs = nowMs;
        sHaveLastEvent = true;
    }

    bool anyEvent = false;

    for (int i = 0; i < inputFdCount; i++)
    {
        /* Drain all pending events from this device.
         * We don't use the event's own timestamp (it's CLOCK_REALTIME);
         * we just note that activity happened and timestamp it with
         * our monotonic clock for consistency. */
        struct input_event ev;
        bool devHadEvent = false;

        while (read(inputFds[i], &ev, sizeof(ev)) == sizeof(ev))
        {
            devHadEvent = true;
        }

        if (devHadEvent)
        {
            anyEvent = true;
        }
    }

    if (anyEvent)
    {
        sLastEventMs = nowMs;
    }

    /* Idle = now minus last known event time */
    if (nowMs > sLastEventMs)
    {
        return nowMs - sLastEventMs;
    }
    return 0;
}

static void inputShutdown(void)
{
    for (int i = 0; i < inputFdCount; i++)
    {
        close(inputFds[i]);
    }
    free(inputFds);
    inputFds = NULL;
    inputFdCount = 0;
}

/* ---------- D-Bus logind backend ---------- */
static void dbusShutdown(void);

static bool dbusInit(void)
{
    DBusError err;

    dbus_error_init(&err);
    dbusConn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err))
    {
        fprintf(stderr, "idle/dbus: failed to connect to system bus: %s\n",
            err.message);
        dbus_error_free(&err);
        return false;
    }
    if (!dbusConn)
    {
        fprintf(stderr, "idle/dbus: no connection\n");
        return false;
    }

    const char *sessionId = getenv("XDG_SESSION_ID");
    if (!sessionId)
    {
        fprintf(stderr, "idle/dbus: XDG_SESSION_ID not set\n");
        dbusShutdown();
        return false;
    }

    size_t pathLen = strlen("/org/freedesktop/login1/session/")
        + strlen(sessionId) + 1;
    dbusSessionPath = malloc(pathLen);
    if (!dbusSessionPath)
    {
        fprintf(stderr, "idle/dbus: out of memory\n");
        dbusShutdown();
        return false;
    }
    snprintf(dbusSessionPath, pathLen,
        "/org/freedesktop/login1/session/%s", sessionId);

    /* Verify property is readable */
    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.login1",
        dbusSessionPath,
        "org.freedesktop.DBus.Properties",
        "Get");
    if (!msg)
    {
        fprintf(stderr, "idle/dbus: failed to create message\n");
        dbusShutdown();
        return false;
    }

    const char *iface = "org.freedesktop.login1.Session";
    const char *prop  = "IdleSinceHintMonotonic";
    dbus_message_append_args(msg,
        DBUS_TYPE_STRING, &iface,
        DBUS_TYPE_STRING, &prop,
        DBUS_TYPE_INVALID);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        dbusConn, msg, 2000, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err) || !reply)
    {
        fprintf(stderr,
            "idle/dbus: cannot read IdleSinceHintMonotonic: %s\n",
            err.message ? err.message : "no reply");
        dbus_error_free(&err);
        if (reply)
        {
            dbus_message_unref(reply);
        }
        dbusShutdown();
        return false;
    }
    dbus_message_unref(reply);

    return true;
}

static uint64_t dbusGetMs(void)
{
    if (!dbusConn || !dbusSessionPath)
    {
        return UINT64_MAX;
    }

    DBusError err;
    dbus_error_init(&err);

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.login1",
        dbusSessionPath,
        "org.freedesktop.DBus.Properties",
        "Get");
    if (!msg)
    {
        return UINT64_MAX;
    }

    const char *iface = "org.freedesktop.login1.Session";
    const char *prop  = "IdleSinceHintMonotonic";
    dbus_message_append_args(msg,
        DBUS_TYPE_STRING, &iface,
        DBUS_TYPE_STRING, &prop,
        DBUS_TYPE_INVALID);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        dbusConn, msg, 1000, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err) || !reply)
    {
        dbus_error_free(&err);
        if (reply)
        {
            dbus_message_unref(reply);
        }
        return UINT64_MAX;
    }

    uint64_t idleUs = 0;
    DBusMessageIter iter;
    dbus_message_iter_init(reply, &iter);

    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT)
    {
        DBusMessageIter variant;
        dbus_message_iter_recurse(&iter, &variant);
        if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_UINT64)
        {
            dbus_message_iter_get_basic(&variant, &idleUs);
        }
    }

    dbus_message_unref(reply);

    return idleUs / 1000;
}

static void dbusShutdown(void)
{
    free(dbusSessionPath);
    dbusSessionPath = NULL;

    if (dbusConn)
    {
        dbus_connection_unref(dbusConn);
        dbusConn = NULL;
    }
}

/* ---------- public API ---------- */
IdleBackend idleInit(void)
{
    gBackend = detectBackend();

    switch (gBackend)
    {
    case IDLE_BACKEND_X11:
        if (x11Init())
        {
            fprintf(stderr, "idle: using X11 backend\n");
            return gBackend;
        }
        fprintf(stderr, "idle: X11 init failed, trying /dev/input\n");
        if (inputInit())
        {
            gBackend = IDLE_BACKEND_INPUT;
            fprintf(stderr, "idle: fell back to /dev/input\n");
            return gBackend;
        }
        fprintf(stderr,
            "idle: /dev/input failed, trying D-Bus logind\n");
        if (dbusInit())
        {
            gBackend = IDLE_BACKEND_DBUS;
            fprintf(stderr, "idle: fell back to D-Bus logind\n");
            return gBackend;
        }
        gBackend = IDLE_BACKEND_NONE;
        return gBackend;

    case IDLE_BACKEND_WAYLAND:
        /* /dev/input is DE/compositor-agnostic — try it first */
        if (inputInit())
        {
            fprintf(stderr, "idle: using /dev/input backend\n");
            gBackend = IDLE_BACKEND_INPUT;
            return gBackend;
        }
        fprintf(stderr,
            "idle: /dev/input failed, trying D-Bus logind\n");
        if (dbusInit())
        {
            gBackend = IDLE_BACKEND_DBUS;
            fprintf(stderr, "idle: using D-Bus logind backend\n");
            return gBackend;
        }
#ifdef HAVE_WAYLAND
        fprintf(stderr,
            "idle: D-Bus logind failed, trying ext-idle-notify\n");
        if (wlInit())
        {
            fprintf(stderr,
                "idle: using Wayland ext-idle-notify backend\n");
            return gBackend;
        }
#endif
        fprintf(stderr, "idle: all Wayland backends failed\n");
        gBackend = IDLE_BACKEND_NONE;
        return gBackend;

    default:
        fprintf(stderr,
            "idle: no supported backend (need X11 or Wayland)\n");
        return IDLE_BACKEND_NONE;
    }
}

uint64_t idleGetMs(void)
{
    switch (gBackend)
    {
    case IDLE_BACKEND_X11:
        return x11GetMs();
    case IDLE_BACKEND_WAYLAND:
#ifdef HAVE_WAYLAND
        return wlGetMs();
#else
        return UINT64_MAX;
#endif
    case IDLE_BACKEND_INPUT:
        return inputGetMs();
    case IDLE_BACKEND_DBUS:
        return dbusGetMs();
    default:
        return UINT64_MAX;
    }
}

void idleShutdown(void)
{
    switch (gBackend)
    {
    case IDLE_BACKEND_X11:
        x11Shutdown();
        break;
    case IDLE_BACKEND_WAYLAND:
#ifdef HAVE_WAYLAND
        wlShutdown();
#endif
        break;
    case IDLE_BACKEND_INPUT:
        inputShutdown();
        break;
    case IDLE_BACKEND_DBUS:
        dbusShutdown();
        break;
    default:
        break;
    }
    gBackend = IDLE_BACKEND_NONE;
}

const char *idleBackendName(void)
{
    switch (gBackend)
    {
    case IDLE_BACKEND_X11:
        return "X11";
    case IDLE_BACKEND_WAYLAND:
        return "Wayland";
    case IDLE_BACKEND_INPUT:
        return "/dev/input";
    case IDLE_BACKEND_DBUS:
        return "D-Bus logind";
    default:
        return "none";
    }
}