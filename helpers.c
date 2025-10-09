#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

char lowercase_ascii_char(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc >= 'A' && uc <= 'Z') {
        return (char)(uc + ('a' - 'A'));
    }
    return (char)uc;
}

static HelpersHooks g_test_hooks = {0};

void helpers_set_hooks(const HelpersHooks *hooks) {
    if (hooks) {
        g_test_hooks = *hooks;
    } else {
        memset(&g_test_hooks, 0, sizeof(g_test_hooks));
    }
}

void clear_screen(void) {
    if (g_test_hooks.clear_screen) {
        g_test_hooks.clear_screen();
        return;
    }
    printf("\033[2J\033[H");
}

void wait_for_enter(void) {
    if (g_test_hooks.wait_for_enter) {
        g_test_hooks.wait_for_enter();
        return;
    }
    printf("\033[1;32mPress Enter to back to menu...\033[0m");
    fflush(stdout);
    getchar();
}

void trim_whitespace(char *str) {
    if (!str) {
        return;
    }

    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    char *end = str + strlen(str);
    while (end > str && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
}

int read_line_allow_ctrl(char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    if (g_test_hooks.read_line_allow_ctrl) {
        return g_test_hooks.read_line_allow_ctrl(buffer, size);
    }

#ifndef _WIN32
    struct termios oldt;
    int have_old = 0;
    if (tcgetattr(STDIN_FILENO, &oldt) == 0) {
        have_old = 1;
        struct termios newt = oldt;
        newt.c_lflag &= (tcflag_t)(~ISIG);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }
#endif

    char *res = fgets(buffer, (int)size, stdin);

#ifndef _WIN32
    if (have_old) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
#endif

    return res != NULL;
}

static int input_matches_ctrl(const char *input, unsigned char control_value, char letter) {
    if (!input) {
        return 0;
    }

    while (isspace((unsigned char)*input)) {
        input++;
    }

    const char *end = input + strlen(input);
    while (end > input && isspace((unsigned char)end[-1])) {
        end--;
    }

    size_t len = (size_t)(end - input);
    if (len == 1 && (unsigned char)input[0] == control_value) {
        return 1;
    }
    if (len == 2 && input[0] == '^' && (input[1] == letter || input[1] == lowercase_ascii_char(letter))) {
        return 1;
    }

    return 0;
}

int input_is_ctrl_x(const char *input) {
    return input_matches_ctrl(input, 0x18, 'X');
}

int input_is_ctrl_z(const char *input) {
    return input_matches_ctrl(input, 0x1A, 'Z');
}

MenuKey read_menu_key(int *out_digit, char *out_char) {
    if (out_digit) {
        *out_digit = -1;
    }
    if (out_char) {
        *out_char = '\0';
    }

    if (g_test_hooks.read_menu_key) {
        return g_test_hooks.read_menu_key(out_digit, out_char);
    }

#ifdef _WIN32
    int ch = _getch();
    if (ch == 0 || ch == 0xE0) {
        int ch2 = _getch();
        if (ch2 == 72) {
            return MENU_KEY_UP;
        }
        if (ch2 == 80) {
            return MENU_KEY_DOWN;
        }
        return MENU_KEY_NONE;
    }
    unsigned char uch = (unsigned char)ch;
    if (ch == 0x14) {
        return MENU_KEY_SHORTCUT_RUN_TESTS;
    }
    if (ch == 0x05) {
        return MENU_KEY_SHORTCUT_RUN_E2E;
    }
    if (ch == 0x11) {
        return MENU_KEY_SHORTCUT_EXIT;
    }
    if (ch == 0x0E) {
        return MENU_KEY_SHORTCUT_ADD_PRODUCT;
    }
    if (ch == '\r') {
        return MENU_KEY_ENTER;
    }
    if (ch == 8) {
        return MENU_KEY_BACKSPACE;
    }
    if (uch >= '0' && uch <= '9') {
        if (out_digit) {
            *out_digit = (int)(uch - '0');
        }
        return MENU_KEY_DIGIT;
    }
    if (uch == 27) {
        return MENU_KEY_ESCAPE;
    }
    if (uch >= 0x20 && uch != 0x7F) {
        if (out_char) {
            *out_char = (char)uch;
        }
        return MENU_KEY_CHAR;
    }
    return MENU_KEY_NONE;
#else
    struct termios oldt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
        return MENU_KEY_NONE;
    }

    struct termios newt = oldt;
    newt.c_lflag &= (tcflag_t)(~(ICANON | ECHO | ISIG));
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) {
        return MENU_KEY_NONE;
    }

    MenuKey result = MENU_KEY_NONE;
    int ch = getchar();
    if (ch == EOF) {
        result = MENU_KEY_NONE;
        goto restore_termios;
    }

    unsigned char uch = (unsigned char)ch;

    if (ch == '\n' || ch == '\r') {
        result = MENU_KEY_ENTER;
        goto restore_termios;
    }

    if (ch == 127 || ch == 8) {
        result = MENU_KEY_BACKSPACE;
        goto restore_termios;
    }

    if (ch == 0x14) {
        result = MENU_KEY_SHORTCUT_RUN_TESTS;
        goto restore_termios;
    }
    if (ch == 0x05) {
        result = MENU_KEY_SHORTCUT_RUN_E2E;
        goto restore_termios;
    }
    if (ch == 0x11) {
        result = MENU_KEY_SHORTCUT_EXIT;
        goto restore_termios;
    }
    if (ch == 0x0E) {
        result = MENU_KEY_SHORTCUT_ADD_PRODUCT;
        goto restore_termios;
    }

    if (uch >= '0' && uch <= '9') {
        if (out_digit) {
            *out_digit = (int)(uch - '0');
        }
        result = MENU_KEY_DIGIT;
        goto restore_termios;
    }

    if (uch == 27) {
        int ch1 = getchar();
        if (ch1 == '[') {
            int ch2 = getchar();
            if (ch2 == 'A') {
                result = MENU_KEY_UP;
            } else if (ch2 == 'B') {
                result = MENU_KEY_DOWN;
            }
        } else if (ch1 == EOF) {
            result = MENU_KEY_ESCAPE;
        } else {
            result = MENU_KEY_ESCAPE;
        }
        goto restore_termios;
    }

    if (uch >= 0x20 && uch != 0x7F) {
        if (out_char) {
            *out_char = (char)uch;
        }
        result = MENU_KEY_CHAR;
        goto restore_termios;
    }

    if (ch == 3) {
        result = MENU_KEY_SHORTCUT_EXIT;
        goto restore_termios;
    }

restore_termios:
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return result;
#endif
}

int get_terminal_rows(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(handle, &info)) {
        int rows = (int)(info.srWindow.Bottom - info.srWindow.Top + 1);
        if (rows > 0) {
            return rows;
        }
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return (int)ws.ws_row;
    }

    const char *env_rows = getenv("LINES");
    if (env_rows) {
        char *endptr = NULL;
        long parsed = strtol(env_rows, &endptr, 10);
        if (endptr != env_rows && parsed > 0 && parsed <= INT_MAX) {
            return (int)parsed;
        }
    }
#endif
    return 24;
}
