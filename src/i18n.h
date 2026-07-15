#ifndef I18N_H
#define I18N_H

#include <libintl.h>

/* Convenience macro for both C and C++ */
#define _(String) gettext(String)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize gettext i18n.
 *
 * - Sets locale from environment (LANG/LC_ALL/LC_MESSAGES)
 * - Binds text domain "digital-wellbeing" to
 *   $HOME/.local/share/locale (fallback /usr/share/locale)
 * - Returns 0 on success, -1 if HOME is unset
 */
int i18nInit(void);

#ifdef __cplusplus
}
#endif

#endif