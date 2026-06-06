/**
 * @file agent_types.h
 * @brief Common types shared across app/ and lib/
 *
 * This header has zero implementation and zero non-stdlib dependencies
 * other than cJSON (forward-declared only). It is the single place
 * where cross-cutting enums and structs live.
 */
#ifndef AGENT_TYPES_H
#define AGENT_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward decl: cJSON — used by lib/util/json_*.c only. */
struct cJSON;

/* Theme */
typedef enum {
    AGENT_THEME_ENGINEERING_BLUE = 0,  /* dark, high-contrast (default) */
    AGENT_THEME_LIGHT,                 /* reserved for v1.1 */
    AGENT_THEME_COUNT_
} agent_theme_t;

/* Language */
typedef enum {
    AGENT_LANG_ZH_CN = 0,
    AGENT_LANG_EN_US,    /* reserved for v1.1 */
    AGENT_LANG_COUNT_
} agent_lang_t;

/* Panel identifiers — used by shell to dispatch render() */
typedef enum {
    AGENT_PANEL_DIAG = 0,
    AGENT_PANEL_DEVICES,
    AGENT_PANEL_PROD,
    AGENT_PANEL_OTA,
    AGENT_PANEL_SETTINGS,
    AGENT_PANEL_COUNT_
} agent_panel_id_t;

/* LLM provider — defined here so settings panel can list them. */
typedef struct agent_llm_provider {
    char name[64];          /* "DeepSeek" / "Qwen" / "GLM" / "Kimi" / "MiniMax" */
    char base_url[256];     /* user-supplied endpoint */
    char api_key[512];      /* DPAPI-encrypted blob in P6; plaintext in P1 */
    char default_model[64];
    struct agent_llm_provider *next;
} agent_llm_provider_t;

/* Top-level app context — held in main(), passed by pointer to panels. */
typedef struct agent_app {
    agent_theme_t          theme;
    agent_lang_t           lang;
    agent_panel_id_t       active_panel;
    bool                   llm_drawer_open;
    agent_llm_provider_t  *providers;   /* singly-linked list */
    /* Future: libuv loop, sqlite handle, etc. — added in P2/P6. */
} agent_app_t;

/* Render function signature every panel implements. */
typedef void (*agent_panel_render_fn)(agent_app_t *app);

/* Error code — single source of truth. */
#define AGENT_OK                0
#define AGENT_ERR              -1
#define AGENT_ERR_NOT_FOUND    -2
#define AGENT_ERR_BAD_ARG      -3
#define AGENT_ERR_IO           -4
#define AGENT_ERR_OOM          -5

const char *agent_errstr(int err);

#endif /* AGENT_TYPES_H */
