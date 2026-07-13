#include "idle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef HAVE_WAYLAND
#include <wayland-client.h>
#include "ext-idle-notify-v1.h"
#endif

#include <X11/extensions/scrnsaver.h>
#include <X11/Xlib.h>

/* ---------- backend state ---------- */
static IdleBackend gBackend = IDLE_BACKEND_NONE;

/* X11 */
static Display *  xDisplay = NULL;
static XScreenSaverInfo *xSaverInfo = NULL;

/* Wayland */
#ifdef HAVE_WAYLAND
static struct wl_display *          wlDisplay   = NULL;
static struct wl_registry *         wlRegistry  = NULL;
static struct ext_idle_notifier_v1 *wlIdleNotifier = NULL;
static struct ext_idle_notification_v1 *wlIdleNotif = NULL;
static struct wl_seat *             wlSeat      = NULL;
static uint64_t                     wlIdleTimeMs = 0;
static bool                         wlIsIdle    = false;
static bool                         wlHasNotifier = false;
#endif

/* ---------- auto-detect ---------- */
static IdleBackend detectBackend(void)
{
    const char *sessionType = getenv("XDG_SESSION_TYPE");
    if (!sessionType)
    {
        /* Fallback: try DISPLAY */
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
    /* "tty" or "mir" — unsupported */
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

    /* Two roundtrips: first to discover globals, second to bind */
    wl_display_roundtrip(wlDisplay);

    if (!wlHasNotifier || !wlIdleNotifier)
    {
        fprintf(stderr,
            "idle/wayland: ext_idle_notifier_v1 not available\n");
        wlShutdown();
        return false;
    }

    /* Create a notification with minimal timeout (1 second)
     * so we can track idled/resumed state */
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

    /* Final roundtrip to get bound interfaces */
    wl_display_roundtrip(wlDisplay);

    return true;
}

static uint64_t wlGetMs(void)
{
    if (!wlDisplay)
    {
        return UINT64_MAX;
    }

    /* Dispatch pending events to update wlIsIdle */
    wl_display_dispatch_pending(wlDisplay);

    /* ext-idle-notify doesn't give exact ms, but we can track state.
     * We return 0 when active, and a large value when idled.
     * For sub-second precision, we'd need the compositor to provide it.
     * This is good enough for our 30-second threshold check. */
    if (wlIsIdle)
    {
        /* Return a value large enough to exceed any reasonable threshold */
        wlIdleTimeMs += 1000;
        return wlIdleTimeMs;
    }

    /* User is active — reset accumulator */
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
        fprintf(stderr, "idle: X11 init failed, trying Wayland\n");
#ifdef HAVE_WAYLAND
        if (wlInit())
        {
            gBackend = IDLE_BACKEND_WAYLAND;
            fprintf(stderr, "idle: fell back to Wayland\n");
            return gBackend;
        }
#endif
        gBackend = IDLE_BACKEND_NONE;
        return gBackend;

    case IDLE_BACKEND_WAYLAND:
#ifdef HAVE_WAYLAND
        if (wlInit())
        {
            fprintf(stderr, "idle: using Wayland backend\n");
            return gBackend;
        }
        fprintf(stderr, "idle: Wayland init failed, trying X11\n");
#endif
        if (x11Init())
        {
            gBackend = IDLE_BACKEND_X11;
            fprintf(stderr, "idle: fell back to X11\n");
            return gBackend;
        }
        gBackend = IDLE_BACKEND_NONE;
        return gBackend;

    default:
        fprintf(stderr, "idle: no supported backend (need X11 or Wayland)\n");
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
    default:
        return "none";
    }
}