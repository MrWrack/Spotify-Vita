#include "vita_browser.h"

#include <psp2/apputil.h>
#include <psp2/sysmodule.h>

#include <string.h>

static int g_apputil_ready = 0;

static int vita_browser_init_apputil(void)
{
    if (g_apputil_ready)
        return 0;

    /*
     * AppUtil is the Vita SDK library that exposes the system web browser.
     * Load the sysmodule first, then initialize AppUtil with zeroed params.
     */
    int rc = sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);
    if (rc < 0)
        return rc;

    SceAppUtilInitParam init_param;
    SceAppUtilBootParam boot_param;

    memset(&init_param, 0, sizeof(init_param));
    memset(&boot_param, 0, sizeof(boot_param));

    rc = sceAppUtilInit(&init_param, &boot_param);
    if (rc < 0)
        return rc;

    g_apputil_ready = 1;
    return 0;
}

int vita_browser_open_url(const char *url)
{
    if (!url || !url[0])
        return -1;

    int rc = vita_browser_init_apputil();
    if (rc < 0)
        return rc;

    SceAppUtilWebBrowserParam param;
    memset(&param, 0, sizeof(param));

    /*
     * launchMode = 0 is the normal URL-open mode used by Vita homebrew.
     * Pass the raw HTTPS Spotify authorization URL here; do NOT prepend
     * "webmodal:" when using sceAppUtilLaunchWebBrowser().
     */
    param.str = url;
    param.strlen = (SceSize)strlen(url);
    param.launchMode = 0;
    param.reserved = 0;

    return sceAppUtilLaunchWebBrowser(&param);
}
