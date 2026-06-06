/**
 * @file debug_console.cpp
 */
#include "debug_console.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

static bool g_console_open = false;

bool agent_maybe_open_debug_console(int argc, char **argv)
{
    if (g_console_open) return true;

    /* Check env var */
    char buf[8] = {0};
    DWORD n = GetEnvironmentVariableA("AGENT_DEBUG_CONSOLE", buf, sizeof(buf));
    bool want = (n > 0 && strcmp(buf, "1") == 0);

    /* Check argv */
    if (!want && argv) {
        for (int i = 1; i < argc; i++) {
            if (argv[i] && strcmp(argv[i], "--debug-console") == 0) {
                want = true;
                break;
            }
        }
    }

    if (!want) return false;

    if (!AllocConsole()) {
        /* Already had a console (e.g. launched from cmd); just redirect. */
    }

    /* Reopen stdio to the new console's CONOUT$ */
    FILE *f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    g_console_open = true;
    return true;
}
