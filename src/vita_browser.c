#include "vita_browser.h"

#include <psp2/appmgr.h>

int vita_browser_open_url(
    const char *url
)
{
    if (!url || !url[0])
        return -1;

    /*
     * VitaSDK documents sceAppMgrLaunchAppByUri() as the user-mode API for
     * launching an application by URI. The documented flag value is 0x20000.
     *
     * Passing an HTTPS URL should hand the URI to the system handler/browser.
     *
     * Hardware note:
     * The exact suspend/resume behavior of the calling homebrew still needs
     * verification on a real Vita. The OAuth callback listener is started
     * before this call to minimize races.
     */
    return sceAppMgrLaunchAppByUri(
        0x20000,
        url
    );
}
