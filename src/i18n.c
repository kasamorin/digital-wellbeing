#include "i18n.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_DOMAIN "digital-wellbeing"

int i18nInit(void)
{
    setlocale(LC_ALL, "");

    const char *home = getenv("HOME");
    const char *localeDir;

    if (home)
    {
        static char homeLocaleDir[1024];
        snprintf(homeLocaleDir, sizeof(homeLocaleDir),
            "%s/.local/share/locale", home);
        localeDir = homeLocaleDir;
    }
    else
    {
        localeDir = "/usr/share/locale";
    }

    bindtextdomain(TEXT_DOMAIN, localeDir);
    bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
    textdomain(TEXT_DOMAIN);

    return home ? 0 : -1;
}