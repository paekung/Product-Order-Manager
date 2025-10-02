#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "helpers.h"

#define E2E_PRODUCTS_FILE "products.csv"

typedef struct {
    char ProductID[20];
    char ProductName[100];
    int Quantity;
    int UnitPrice;
} Product;

extern Product *products;
extern int product_count;
extern int product_capacity;

int ensure_csv_exists(const char *filename);
int load_csv(const char *filename);
int save_csv(const char *filename);
int add_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice);
int update_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice);
int remove_product(const char *ProductID);
int find_products_by_keyword(const char *keyword, int **out_matches);
void menu_product_manager(void);

typedef struct {
    MenuKey key;
    int digit;
    char typed;
} MenuScriptEvent;

static const MenuScriptEvent *g_script_events = NULL;
static size_t g_script_event_count = 0;
static size_t g_script_event_index = 0;
static int g_script_events_exhausted = 0;

static const char *const *g_script_lines = NULL;
static size_t g_script_line_count = 0;
static size_t g_script_line_index = 0;
static int g_script_lines_exhausted = 0;
static int g_script_step_mode = 0;
static size_t g_event_step_index = 0;
static size_t g_line_step_index = 0;

typedef struct {
    size_t event_index;
    const char *message;
} ScriptEventStep;

typedef struct {
    size_t line_index;
    const char *message;
} ScriptLineStep;

static const ScriptEventStep g_event_steps[] = {
    {0, "Selecting \"Add new product\" entry"},
    {1, "Re-selecting \"Add new product\" to add another item"},
    {2, "Opening form to add the second product"},
    {3, "Highlighting the newly added product"},
    {4, "Applying filter keyword 'mouse'"},
    {9, "Opening product actions for filtered result"},
    {10, "Choosing to update the product"},
    {11, "Clearing the search filter"},
    {16, "Opening actions for the first product"},
    {17, "Navigating to the remove option"},
    {18, "Confirming product removal"},
    {19, "Exiting the application"}
};

static const ScriptLineStep g_line_steps[] = {
    {0, "Entering Product ID: E2E001"},
    {1, "Entering Product Name: E2E Mechanical Keyboard"},
    {2, "Entering Quantity: 15"},
    {3, "Entering Unit Price: 2890"},
    {4, "Entering Product ID: E2E002"},
    {5, "Entering Product Name: E2E Precision Mouse"},
    {6, "Entering Quantity: 20"},
    {7, "Entering Unit Price: 1990"},
    {8, "Updating Product Name to: E2E Precision Mouse Pro"},
    {9, "Updating Quantity to: 25"},
    {10, "Updating Unit Price to: 2490"},
    {11, "Confirming removal with 'y'"}
};

static void scripted_step_pause(void) {
    if (!g_script_step_mode) {
        return;
    }
#ifdef _WIN32
    Sleep(600);
#else
    usleep(600000);
#endif
}

static void scripted_step_announce(const char *message) {
    if (!g_script_step_mode || !message) {
        return;
    }
    printf("  \033[1;36m[STEP]\033[0m %s\n", message);
    fflush(stdout);
    scripted_step_pause();
}

static MenuKey scripted_read_menu_key(int *out_digit, char *out_char) {
    if (out_digit) {
        *out_digit = -1;
    }
    if (out_char) {
        *out_char = '\0';
    }

    if (!g_script_events || g_script_event_index >= g_script_event_count) {
        g_script_events_exhausted = 1;
        return MENU_KEY_ESCAPE;
    }

    size_t current_index = g_script_event_index;
    while (g_script_step_mode &&
           g_event_step_index < (sizeof(g_event_steps) / sizeof(g_event_steps[0])) &&
           g_event_steps[g_event_step_index].event_index == current_index) {
        scripted_step_announce(g_event_steps[g_event_step_index].message);
        g_event_step_index++;
    }

    MenuScriptEvent ev = g_script_events[g_script_event_index++];
    if (out_digit) {
        *out_digit = ev.digit;
    }
    if (out_char) {
        *out_char = ev.typed;
    }
    return ev.key;
}

static int scripted_read_line_allow_ctrl(char *buffer, size_t size) {
    if (!buffer || size < 2) {
        return 0;
    }

    if (!g_script_lines || g_script_line_index >= g_script_line_count) {
        g_script_lines_exhausted = 1;
        buffer[0] = '\0';
        return 0;
    }

    size_t current_index = g_script_line_index;
    while (g_script_step_mode &&
           g_line_step_index < (sizeof(g_line_steps) / sizeof(g_line_steps[0])) &&
           g_line_steps[g_line_step_index].line_index == current_index) {
        scripted_step_announce(g_line_steps[g_line_step_index].message);
        g_line_step_index++;
    }

    const char *src = g_script_lines[g_script_line_index++];
    size_t len = strlen(src);
    if (len >= size - 1) {
        len = size - 2;
    }
    memcpy(buffer, src, len);
    buffer[len] = '\n';
    buffer[len + 1] = '\0';
    return 1;
}

static void scripted_wait_for_enter(void) {
    /* Skip pauses during scripted sessions */
}

static void scripted_clear_screen(void) {
    /* Suppress terminal clear in scripted tests */
}

static int prompt_e2e_display_mode(void) {
    char buffer[32];

    while (1) {
        printf("\nChoose E2E display mode:\n");
        printf("  [1] Step-by-step (with live replay)\n");
        printf("  [2] Summary results only\n");
        printf("Select option (default 2): ");
        fflush(stdout);

        if (!read_line_allow_ctrl(buffer, sizeof(buffer))) {
            printf("\n\033[1;33mNo input detected, defaulting to summary mode.\033[0m\n");
            return 2;
        }

        buffer[strcspn(buffer, "\r\n")] = '\0';
        trim_whitespace(buffer);

        if (buffer[0] == '\0') {
            return 2;
        }

        if (input_is_ctrl_x(buffer)) {
            printf("\n\033[1;33mCancelled selection, using summary mode.\033[0m\n");
            return 2;
        }

        if (buffer[0] == '1' && buffer[1] == '\0') {
            return 1;
        }
        if (buffer[0] == '2' && buffer[1] == '\0') {
            return 2;
        }

        printf("\033[1;31mInvalid choice. Please enter 1 or 2.\033[0m\n\n");
    }
}

typedef struct {
    Product *original_products;
    Product *snapshot;
    int original_count;
    int original_capacity;
} ProductStateBackup;

typedef struct {
    char *data;
    size_t size;
    int existed;
} FileBackup;

static int backup_product_state(ProductStateBackup *backup) {
    if (!backup) {
        return -1;
    }

    backup->original_products = products;
    backup->original_count = product_count;
    backup->original_capacity = product_capacity;
    backup->snapshot = NULL;

    if (products && product_capacity > 0) {
        backup->snapshot = (Product *)malloc((size_t)product_capacity * sizeof(Product));
        if (!backup->snapshot) {
            return -1;
        }
        memcpy(backup->snapshot, products, (size_t)product_capacity * sizeof(Product));
    }

    products = NULL;
    product_count = 0;
    product_capacity = 0;

    return 0;
}

static void restore_product_state(ProductStateBackup *backup) {
    if (!backup) {
        return;
    }

    free(products);
    products = backup->original_products;
    product_count = backup->original_count;
    product_capacity = backup->original_capacity;

    if (backup->snapshot && backup->original_products && backup->original_capacity > 0) {
        memcpy(backup->original_products,
               backup->snapshot,
               (size_t)backup->original_capacity * sizeof(Product));
    }

    free(backup->snapshot);
    backup->snapshot = NULL;
}

static void reset_e2e_environment(void) {
    free(products);
    products = NULL;
    product_count = 0;
    product_capacity = 0;
}

static int backup_products_file(FileBackup *backup, const char *path) {
    if (!backup || !path) {
        return -1;
    }

    backup->data = NULL;
    backup->size = 0;
    backup->existed = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }

    backup->existed = 1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    backup->size = (size_t)len;
    if (backup->size > 0) {
        backup->data = (char *)malloc(backup->size);
        if (!backup->data) {
            fclose(fp);
            return -1;
        }
        size_t read_bytes = fread(backup->data, 1, backup->size, fp);
        if (read_bytes != backup->size) {
            free(backup->data);
            backup->data = NULL;
            backup->size = 0;
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

static int restore_products_file(FileBackup *backup, const char *path) {
    if (!backup || !path) {
        return -1;
    }

    int result = 0;

    if (backup->existed) {
        FILE *fp = fopen(path, "wb");
        if (!fp) {
            result = -1;
            goto cleanup;
        }
        if (backup->size > 0) {
            size_t written = fwrite(backup->data, 1, backup->size, fp);
            if (written != backup->size) {
                fclose(fp);
                result = -1;
                goto cleanup;
            }
        }
        if (fclose(fp) != 0) {
            result = -1;
        }
    } else {
        if (remove(path) != 0 && errno != ENOENT) {
            result = -1;
        }
    }

cleanup:
    free(backup->data);
    backup->data = NULL;
    backup->size = 0;
    backup->existed = 0;
    return result;
}

static int scenario_full_user_journey(void) {
    static const MenuScriptEvent script_events[] = {
        {MENU_KEY_ENTER, -1, '\0'},            /* select add product */
        {MENU_KEY_UP, -1, '\0'},               /* focus add product again */
        {MENU_KEY_ENTER, -1, '\0'},            /* add second product */
        {MENU_KEY_DOWN, -1, '\0'},             /* highlight second product */
        {MENU_KEY_CHAR, -1, 'm'},              /* start filter */
        {MENU_KEY_CHAR, -1, 'o'},
        {MENU_KEY_CHAR, -1, 'u'},
        {MENU_KEY_CHAR, -1, 's'},
        {MENU_KEY_CHAR, -1, 'e'},
        {MENU_KEY_ENTER, -1, '\0'},            /* open filtered product actions */
        {MENU_KEY_ENTER, -1, '\0'},            /* choose update */
        {MENU_KEY_BACKSPACE, -1, '\0'},        /* clear filter char 1 */
        {MENU_KEY_BACKSPACE, -1, '\0'},        /* clear filter char 2 */
        {MENU_KEY_BACKSPACE, -1, '\0'},        /* clear filter char 3 */
        {MENU_KEY_BACKSPACE, -1, '\0'},        /* clear filter char 4 */
        {MENU_KEY_BACKSPACE, -1, '\0'},        /* clear filter char 5 */
        {MENU_KEY_ENTER, -1, '\0'},            /* open first product actions */
        {MENU_KEY_DOWN, -1, '\0'},             /* select remove */
        {MENU_KEY_ENTER, -1, '\0'},            /* confirm remove dialog */
        {MENU_KEY_SHORTCUT_EXIT, -1, '\0'}     /* exit application */
    };

    static const char *const script_lines[] = {
        "E2E001",
        "E2E Mechanical Keyboard",
        "15",
        "2890",
        "E2E002",
        "E2E Precision Mouse",
        "20",
        "1990",
        "E2E Precision Mouse Pro",
        "25",
        "2490",
        "y"
    };

    reset_e2e_environment();

    if (remove(E2E_PRODUCTS_FILE) != 0 && errno != ENOENT) {
        printf("    Failed to reset %s\n", E2E_PRODUCTS_FILE);
        return 1;
    }

    if (ensure_csv_exists(E2E_PRODUCTS_FILE) != 0) {
        printf("    ensure_csv_exists failed\n");
        return 1;
    }

    if (load_csv(E2E_PRODUCTS_FILE) != 0) {
        printf("    load_csv failed on empty catalog\n");
        return 1;
    }

    if (product_count != 0) {
        printf("    Expected empty catalog after load, got %d items\n", product_count);
        return 1;
    }

    g_script_events = script_events;
    g_script_event_count = sizeof(script_events) / sizeof(script_events[0]);
    g_script_event_index = 0;
    g_script_events_exhausted = 0;

    g_script_lines = script_lines;
    g_script_line_count = sizeof(script_lines) / sizeof(script_lines[0]);
    g_script_line_index = 0;
    g_script_lines_exhausted = 0;
    g_event_step_index = 0;
    g_line_step_index = 0;

    HelpersHooks hooks = {
        scripted_read_menu_key,
        scripted_read_line_allow_ctrl,
        scripted_wait_for_enter,
        scripted_clear_screen
    };

    helpers_set_hooks(&hooks);

    menu_product_manager();

    helpers_set_hooks(NULL);

    size_t expected_events = g_script_event_count;
    size_t expected_lines = g_script_line_count;

    g_script_events = NULL;
    g_script_lines = NULL;

    if (g_script_events_exhausted || g_script_event_index != expected_events) {
        printf("    Menu script did not complete cleanly (used %zu/%zu events).\n",
               g_script_event_index,
               expected_events);
        return 1;
    }

    if (g_script_lines_exhausted || g_script_line_index != expected_lines) {
        printf("    Line script did not complete cleanly (used %zu/%zu lines).\n",
               g_script_line_index,
               expected_lines);
        return 1;
    }

    if (product_count != 1) {
        printf("    Expected catalog to contain 1 product after scripted session, got %d.\n",
               product_count);
        return 1;
    }

    if (strcmp(products[0].ProductID, "E2E002") != 0 ||
        strcmp(products[0].ProductName, "E2E Precision Mouse Pro") != 0 ||
        products[0].Quantity != 25 ||
        products[0].UnitPrice != 2490) {
        printf("    Final product state did not match expectations.\n");
        return 1;
    }

    reset_e2e_environment();
    if (load_csv(E2E_PRODUCTS_FILE) != 0) {
        printf("    load_csv failed to reload persisted catalog\n");
        return 1;
    }

    if (product_count != 1) {
        printf("    Expected 1 product after reload, got %d\n", product_count);
        return 1;
    }

    if (strcmp(products[0].ProductID, "E2E002") != 0 ||
        strcmp(products[0].ProductName, "E2E Precision Mouse Pro") != 0 ||
        products[0].Quantity != 25 ||
        products[0].UnitPrice != 2490) {
        printf("    Persisted data did not match expected values\n");
        return 1;
    }

    int *matches = NULL;
    int match_count = find_products_by_keyword("pro", &matches);
    if (match_count != 1) {
        printf("    Expected keyword search on persisted data to return 1 item, got %d\n", match_count);
        free(matches);
        return 1;
    }
    free(matches);

    return 0;
}

typedef int (*E2EScenario)(void);

typedef struct {
    const char *name;
    E2EScenario func;
} E2ETestCase;

int run_e2e_tests(void) {
    int display_mode = prompt_e2e_display_mode();
    g_script_step_mode = (display_mode == 1);

    ProductStateBackup state_backup;
    FileBackup file_backup;

    if (backup_product_state(&state_backup) != 0) {
        printf("Failed to back up product state.\n");
        return 1;
    }

    if (backup_products_file(&file_backup, E2E_PRODUCTS_FILE) != 0) {
        printf("Failed to back up %s.\n", E2E_PRODUCTS_FILE);
        restore_product_state(&state_backup);
        return 1;
    }

    const E2ETestCase scenarios[] = {
        {"Complete user journey", scenario_full_user_journey}
    };

    const size_t scenario_count = sizeof(scenarios) / sizeof(scenarios[0]);
    size_t passed = 0;
    int *results = (int *)calloc(scenario_count, sizeof(int));
    if (!results) {
        printf("Failed to allocate memory for results.\n");
        restore_products_file(&file_backup, E2E_PRODUCTS_FILE);
        restore_product_state(&state_backup);
        g_script_step_mode = 0;
        return 1;
    }

    if (g_script_step_mode) {
        printf("\n\033[1mStarting step-by-step replay...\033[0m\n\n");
    } else {
        printf("\nRunning end-to-end tests...\n\n");
    }

    for (size_t i = 0; i < scenario_count; i++) {
        reset_e2e_environment();
        if (g_script_step_mode) {
            printf("\033[1;34mScenario:\033[0m %s\n", scenarios[i].name);
        }

        int rc = scenarios[i].func();
        results[i] = rc;

        if (rc == 0) {
            passed++;
        }

        if (g_script_step_mode) {
            if (rc == 0) {
                printf("\033[1;32m✓ Scenario completed successfully.\033[0m\n\n");
            } else {
                printf("\033[1;31m✗ Scenario failed.\033[0m\n\n");
            }
        } else {
            if (rc == 0) {
                printf("[PASS] %s\n", scenarios[i].name);
            } else {
                printf("[FAIL] %s\n", scenarios[i].name);
            }
        }
    }

    reset_e2e_environment();

    if (restore_products_file(&file_backup, E2E_PRODUCTS_FILE) != 0) {
        printf("Warning: Failed to restore %s.\n", E2E_PRODUCTS_FILE);
    }

    restore_product_state(&state_backup);

    printf("\n\033[1mResult breakdown:\033[0m\n");
    for (size_t i = 0; i < scenario_count; i++) {
        const char *status = (results[i] == 0) ? "PASS" : "FAIL";
        const char *color = (results[i] == 0) ? "\033[1;32m" : "\033[1;31m";
        printf("  %s%s\033[0m %s\n", color, status, scenarios[i].name);
    }

    printf("\nE2E summary: %zu/%zu passed.\n", passed, scenario_count);

    free(results);
    g_script_step_mode = 0;

    return (passed == scenario_count) ? 0 : 1;
}
