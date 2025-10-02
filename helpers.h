#ifndef HELPERS_H
#define HELPERS_H

#include <stddef.h>

typedef enum {
    MENU_KEY_NONE = 0,
    MENU_KEY_UP,
    MENU_KEY_DOWN,
    MENU_KEY_ENTER,
    MENU_KEY_DIGIT,
    MENU_KEY_ESCAPE,
    MENU_KEY_BACKSPACE,
    MENU_KEY_CHAR,
    MENU_KEY_SHORTCUT_RUN_TESTS,
    MENU_KEY_SHORTCUT_EXIT,
    MENU_KEY_SHORTCUT_ADD_PRODUCT
} MenuKey;

void clear_screen(void);
void wait_for_enter(void);
void trim_whitespace(char *str);
int read_line_allow_ctrl(char *buffer, size_t size);
char lowercase_ascii_char(char c);
int input_is_ctrl_x(const char *input);
int input_is_ctrl_z(const char *input);
MenuKey read_menu_key(int *out_digit, char *out_char);
int get_terminal_rows(void);

#endif // HELPERS_H
