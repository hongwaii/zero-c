/**
 * @file debug_console.h
 * @brief Optional debug console for the GUI app.
 *
 * By default the EXE is a Windows GUI subsystem app — no console is opened.
 * If the user wants to see printf/ImGui log output in a console, they can:
 *   - set the environment variable AGENT_DEBUG_CONSOLE=1 before launching
 *   - pass --debug-console on the command line
 *
 * Both of those will call agent_open_debug_console() at startup, which uses
 * AllocConsole() to create a console window and reopens stdout/stderr to it.
 */
#ifndef APP_SHELL_DEBUG_CONSOLE_H
#define APP_SHELL_DEBUG_CONSOLE_H

/* Inspects the environment and argv; calls agent_open_debug_console() if either
 * AGENT_DEBUG_CONSOLE=1 is set in the environment or --debug-console appears
 * in argv. Returns true if a console was opened.
 */
bool agent_maybe_open_debug_console(int argc, char **argv);

#endif
