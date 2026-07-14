#ifndef IDLE_H
#define IDLE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    IDLE_BACKEND_NONE,
    IDLE_BACKEND_X11,
    IDLE_BACKEND_WAYLAND,
    IDLE_BACKEND_INPUT,
    IDLE_BACKEND_DBUS
} IdleBackend;

/*
 * Initialize idle detection subsystem.
 * Auto-detects X11 or Wayland via $XDG_SESSION_TYPE.
 * Returns the backend selected, or IDLE_BACKEND_NONE on failure.
 */
IdleBackend idleInit(void);

/*
 * Return the user idle time in milliseconds.
 * Returns UINT64_MAX on error / unsupported.
 */
uint64_t idleGetMs(void);

/*
 * Shut down idle detection and free resources.
 */
void idleShutdown(void);

/*
 * Human-readable backend name for logging.
 */
const char *idleBackendName(void);

#endif