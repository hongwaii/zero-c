/**
 * @file diag_state.c
 */
#include "diag_state.h"
#include <string.h>

void diag_state_reset(diag_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    strncpy(s->last_update, "-", sizeof(s->last_update) - 1);
}
