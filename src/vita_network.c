#include "vita_network.h"

#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>

#include <stdlib.h>
#include <string.h>

#define VITA_NET_MEMORY_SIZE (1024 * 1024)

static void *g_net_memory = NULL;
static int g_net_initialized = 0;
static int g_netctl_initialized = 0;
static int g_net_module_loaded_here = 0;

int vita_network_init(void)
{
    if (g_net_initialized)
        return 0;

    /*
     * VitaSDK's net_http sample loads SCE_SYSMODULE_NET before sceNetInit().
     * Track whether this module was already present so shutdown does not
     * unload a module owned by another component.
     */
    if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) != SCE_SYSMODULE_LOADED) {
        int rc = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
        if (rc < 0)
            return rc;

        g_net_module_loaded_here = 1;
    }

    g_net_memory = malloc(VITA_NET_MEMORY_SIZE);
    if (!g_net_memory) {
        if (g_net_module_loaded_here) {
            sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
            g_net_module_loaded_here = 0;
        }
        return -1;
    }

    SceNetInitParam param;
    memset(&param, 0, sizeof(param));

    param.memory = g_net_memory;
    param.size = VITA_NET_MEMORY_SIZE;
    param.flags = 0;

    int rc = sceNetInit(&param);
    if (rc < 0) {
        free(g_net_memory);
        g_net_memory = NULL;

        if (g_net_module_loaded_here) {
            sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
            g_net_module_loaded_here = 0;
        }

        return rc;
    }

    g_net_initialized = 1;

    rc = sceNetCtlInit();
    if (rc < 0) {
        sceNetTerm();
        g_net_initialized = 0;

        free(g_net_memory);
        g_net_memory = NULL;

        if (g_net_module_loaded_here) {
            sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
            g_net_module_loaded_here = 0;
        }

        return rc;
    }

    g_netctl_initialized = 1;
    return 0;
}

void vita_network_shutdown(void)
{
    if (g_netctl_initialized) {
        sceNetCtlTerm();
        g_netctl_initialized = 0;
    }

    if (g_net_initialized) {
        sceNetTerm();
        g_net_initialized = 0;
    }

    free(g_net_memory);
    g_net_memory = NULL;

    if (g_net_module_loaded_here) {
        sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
        g_net_module_loaded_here = 0;
    }
}


int vita_network_get_state(int *state)
{
    if (!state)
        return -1;

    *state = 0;

    if (!g_net_initialized || !g_netctl_initialized)
        return -2;

    return sceNetCtlInetGetState(state);
}

int vita_network_is_connected(void)
{
    int state = 0;
    int rc = vita_network_get_state(&state);

    if (rc < 0)
        return 0;

    /*
     * Vita NetCtl uses 3 for the fully connected internet state.
     * Keep the comparison numeric so this code stays compatible with
     * VitaSDK header revisions that used different enum spellings.
     */
    return state == 3;
}
