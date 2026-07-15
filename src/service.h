#ifndef SERVICE_H
#define SERVICE_H

/*
 * Installs the systemd user unit on first run.
 *
 * - Writes ~/.config/systemd/user/digital-wellbeing.service
 * - Runs systemctl --user enable digital-wellbeing.service
 *   (if systemd is reachable)
 *
 * Safe to call on every startup — skips if the service file already exists.
 * Returns 0 on success (or already installed), -1 on error.
 */
int serviceInstall(void);

#endif