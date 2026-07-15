#ifndef SERVICE_H
#define SERVICE_H

/*
 * Installs the systemd user unit.
 *
 * - Writes ~/.config/systemd/user/digital-wellbeing.service
 * - Prints instructions to run 'systemctl --user enable
 *   digital-wellbeing.service' (does not run systemctl itself)
 *
 * Safe to call repeatedly — skips if the service file already exists.
 * Returns 0 on success (or already installed), -1 on error.
 */
int serviceInstall(void);

#endif