#include "i18n.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TEXT_DOMAIN "digital-wellbeing"
#define SYSTEM_LOCALE_DIR "/usr/share/locale"

/*
 * Pick the locale directory that actually contains translations.
 * gettext's bindtextdomain() accepts only one path and does NOT fall back
 * to /usr/share/locale on its own. Without this check, a PKGBUILD install
 * (which installs .mo files system-wide) would never show translations for
 * users with HOME set — i.e. virtually everyone.
 */
static const char *selectLocaleDir(void)
{
    const char *home = getenv("HOME");
    static char homeLocaleDir[1024];

    if (home)
    {
        snprintf(homeLocaleDir, sizeof(homeLocaleDir),
            "%s/.local/share/locale", home);
        struct stat st;
        if (stat(homeLocaleDir, &st) == 0 && S_ISDIR(st.st_mode))
        {
            return homeLocaleDir;
        }
    }

    return SYSTEM_LOCALE_DIR;
}

int i18nInit(void)
{
    setlocale(LC_ALL, "");

    const char *home = getenv("HOME");
    bindtextdomain(TEXT_DOMAIN, selectLocaleDir());
    bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
    textdomain(TEXT_DOMAIN);

    return home ? 0 : -1;
}