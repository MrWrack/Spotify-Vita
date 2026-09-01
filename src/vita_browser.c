#include "vita_browser.h"

#include <psp2/appmgr.h>

#include <stdio.h>
#include <string.h>

int vita_browser_open_url(
    const char *url
)
{
    if (!url || !url[0])
        return -1;

    /*
     * On Vita, HTTPS links from homebrew are reliably handed to the
     * system browser through the "webmodal:" URI handler.
     *
     * Example used by Vita homebrew:
     *   sceAppMgrLaunchAppByUri(0x20000, "webmodal: https://...");
     *
     * Keep the auth URL unchanged after the prefix.
     */
    char uri[4096];

    int n = snprintf(
        uri,
        sizeof(uri),
        "webmodal: %s",
        url
    );

    if (n < 0 || n >= (int)sizeof(uri))
        return -2;

    return sceAppMgrLaunchAppByUri(
        0x20000,
        uri
    );
}
