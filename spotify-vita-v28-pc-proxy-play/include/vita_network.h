#ifndef VITA_NETWORK_H
#define VITA_NETWORK_H

int vita_network_init(void);
void vita_network_shutdown(void);

/* Returns 1 when Vita NetCtl reports an active internet connection. */
int vita_network_is_connected(void);

/* Returns 0 on success and writes the raw NetCtl state to *state. */
int vita_network_get_state(int *state);

#endif
