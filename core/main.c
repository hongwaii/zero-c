/**
 * @file main.c
 * @brief Application entry point
 */
#include <stdio.h>
#include "version.h"

int main(void)
{
    printf("=== %s ===\n", APP_FULL_TAG);
    printf("Version: %s\n", APP_VERSION_STRING);
    printf("Git:     %s\n", APP_GIT_HASH);
    printf("Hello, Windows-C!\n");
    return 0;
}
