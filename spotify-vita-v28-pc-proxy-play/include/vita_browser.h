#ifndef VITA_BROWSER_H
#define VITA_BROWSER_H

/*
 * Opens a URI using Vita AppMgr.
 *
 * Returns 0 on success, < 0 on AppMgr error.
 */
int vita_browser_open_url(
    const char *url
);

#endif
