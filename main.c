#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

// For Windows console UTF-8 support
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

// Define Product structure
typedef struct {
    char ProductID[20];
    char ProductName[100];
    int Quantity;
    int UnitPrice;
} Product;

// Global variables
Product *products = NULL;
int product_count = 0;
int product_capacity = 0;

// Function prototypes
int load_csv(const char *filename);
int add_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice);
int remove_product(const char *ProductID);
int update_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice);
int save_csv(const char *filename);
void menu_add_product();
void menu_product_manager();
int run_unit_tests(void);
int find_products_by_keyword(const char *keyword, int **out_matches);
int input_is_ctrl_x(const char *input);
int input_is_ctrl_z(const char *input);
void trim_whitespace(char *str);
int read_line_allow_ctrl(char *buffer, size_t size);
typedef enum {
    MENU_KEY_NONE = 0,
    MENU_KEY_UP,
    MENU_KEY_DOWN,
    MENU_KEY_ENTER,
    MENU_KEY_DIGIT,
    MENU_KEY_ESCAPE,
    MENU_KEY_BACKSPACE,
    MENU_KEY_CHAR
} MenuKey;
static MenuKey read_menu_key(int *out_digit, char *out_char);
static int get_terminal_rows(void);
/////////////////////////

// Utility functions
void clear_screen() {
    printf("\033[2J\033[H");
}

void wait_for_enter() {
    printf("\033[1;32mPress Enter to back to menu...\033[0m");
   fflush(stdout);
   getchar();
}
////////////////////////

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
////////////////////////

int read_line_allow_ctrl(char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
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
////////////////////////

static MenuKey read_menu_key(int *out_digit, char *out_char) {
    if (out_digit) {
        *out_digit = -1;
    }
    if (out_char) {
        *out_char = '\0';
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
    if (ch == '\r') {
        return MENU_KEY_ENTER;
    }
    if (ch == 8) {
        return MENU_KEY_BACKSPACE;
    }
    if (ch >= '0' && ch <= '9') {
        if (out_digit) {
            *out_digit = ch - '0';
        }
        return MENU_KEY_DIGIT;
    }
    if (ch == 27) {
        return MENU_KEY_ESCAPE;
    }
    if (isprint(ch)) {
        if (out_char) {
            *out_char = (char)ch;
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
    newt.c_lflag &= (tcflag_t)(~(ICANON | ECHO));
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

    if (ch == '\n' || ch == '\r') {
        result = MENU_KEY_ENTER;
        goto restore_termios;
    }

    if (ch == 127 || ch == 8) {
        result = MENU_KEY_BACKSPACE;
        goto restore_termios;
    }

    if (ch >= '0' && ch <= '9') {
        if (out_digit) {
            *out_digit = ch - '0';
        }
        result = MENU_KEY_DIGIT;
        goto restore_termios;
    }

    if (ch == 27) {
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

    if (isprint(ch)) {
        if (out_char) {
            *out_char = (char)ch;
        }
        result = MENU_KEY_CHAR;
        goto restore_termios;
    }

    if (ch == 3) {
        result = MENU_KEY_ESCAPE;
    }

restore_termios:
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return result;
#endif
}
////////////////////////

static int get_terminal_rows(void) {
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
    return 24; // sensible default when terminal size is unknown
}
////////////////////////

typedef enum {
    INPUT_RESULT_OK = 0,
    INPUT_RESULT_CANCEL,
    INPUT_RESULT_BACK
} InputResult;

typedef enum {
    PRODUCT_ACTION_NONE = 0,
    PRODUCT_ACTION_UPDATED,
    PRODUCT_ACTION_REMOVED
} ProductActionResult;

typedef enum {
    EDIT_PRODUCT_CANCELLED = 0,
    EDIT_PRODUCT_UPDATED,
    EDIT_PRODUCT_FAILED
} EditProductResult;

static int product_id_exists(const char *ProductID);
static InputResult prompt_product_id(char *ProductID, size_t size, int *hasProductID);
static InputResult prompt_product_name(char *ProductName, size_t size, int *hasProductName);
static InputResult prompt_integer_input(const char *prompt, const char *field_name, int *value, int *hasValue);
static EditProductResult edit_product_prompt(Product *prod);
static ProductActionResult product_manager_handle_action(int product_index, char *status_buf, size_t status_len);
////////////////////////

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
    if (len == 2 && input[0] == '^' && (input[1] == letter || input[1] == (char)tolower((unsigned char)letter))) {
        return 1;
    }

    return 0;
}

// Detect if the user pressed Ctrl+X (ASCII 24) or typed ^X manually
int input_is_ctrl_x(const char *input) {
    return input_matches_ctrl(input, 0x18, 'X');
}

// Detect if the user pressed Ctrl+Z (ASCII 26) or typed ^Z manually
int input_is_ctrl_z(const char *input) {
    return input_matches_ctrl(input, 0x1A, 'Z');
}
////////////////////////

static int product_id_exists(const char *ProductID) {
    if (!ProductID) {
        return 0;
    }

    for (int i = 0; i < product_count; i++) {
        if (strcmp(products[i].ProductID, ProductID) == 0) {
            return 1;
        }
    }

    return 0;
}

static InputResult prompt_product_id(char *ProductID, size_t size, int *hasProductID) {
    char buf[256];

    while (1) {
        printf("Enter Product ID or press \033[1;31mCtrl+X\033[0m to cancel");
        if (hasProductID && *hasProductID) {
            printf(" [current: %s]", ProductID);
        }
        printf(": ");

        printf("\033[1;33m");
        if (!read_line_allow_ctrl(buf, sizeof(buf))) {
            printf("\033[0m");
            return INPUT_RESULT_CANCEL;
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("\033[0m");

        if (input_is_ctrl_x(buf)) {
            return INPUT_RESULT_CANCEL;
        }

        if (input_is_ctrl_z(buf)) {
            printf("\033[1;33mAlready at the first input.\033[0m\n");
            continue;
        }

        trim_whitespace(buf);

        if (buf[0] == '\0') {
            if (hasProductID && *hasProductID) {
                return INPUT_RESULT_OK;
            }
            printf("\033[1;31mProduct ID cannot be empty.\033[0m\n");
            continue;
        }

        if (strlen(buf) >= size) {
            printf("\033[1;31mProduct ID is too long (max %zu characters).\033[0m\n", size - 1);
            continue;
        }

        if (!hasProductID || !*hasProductID || strcmp(buf, ProductID) != 0) {
            if (product_id_exists(buf)) {
                printf("\033[1;31mDuplicate Product ID. Please enter a different ID.\033[0m\n");
                continue;
            }
            strcpy(ProductID, buf);
        }

        if (hasProductID) {
            *hasProductID = 1;
        }

        return INPUT_RESULT_OK;
    }
}

static InputResult prompt_product_name(char *ProductName, size_t size, int *hasProductName) {
    char buf[256];

    while (1) {
        printf("Enter Product Name (Ctrl+Z to go back, Ctrl+X to cancel)");
        if (hasProductName && *hasProductName && ProductName[0] != '\0') {
            printf(" [current: %s]", ProductName);
        }
        printf(": ");

        printf("\033[1;33m");
        if (!read_line_allow_ctrl(buf, sizeof(buf))) {
            printf("\033[0m");
            return INPUT_RESULT_CANCEL;
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("\033[0m");

        if (input_is_ctrl_x(buf)) {
            return INPUT_RESULT_CANCEL;
        }

        if (input_is_ctrl_z(buf)) {
            return INPUT_RESULT_BACK;
        }

        trim_whitespace(buf);

        if (buf[0] == '\0') {
            if (hasProductName && *hasProductName && ProductName[0] != '\0') {
                return INPUT_RESULT_OK;
            }
            printf("\033[1;31mProduct name cannot be empty.\033[0m\n");
            continue;
        }

        strncpy(ProductName, buf, size - 1);
        ProductName[size - 1] = '\0';
        if (hasProductName) {
            *hasProductName = 1;
        }

        return INPUT_RESULT_OK;
    }
}

static InputResult prompt_integer_input(const char *prompt, const char *field_name, int *value, int *hasValue) {
    char buf[256];

    while (1) {
        printf("%s (Ctrl+Z to go back, Ctrl+X to cancel)", prompt);
        if (hasValue && *hasValue) {
            printf(" [current: %d]", *value);
        }
        printf(": ");

        printf("\033[1;33m");
        if (!read_line_allow_ctrl(buf, sizeof(buf))) {
            printf("\033[0m");
            return INPUT_RESULT_CANCEL;
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("\033[0m");

        if (input_is_ctrl_x(buf)) {
            return INPUT_RESULT_CANCEL;
        }

        if (input_is_ctrl_z(buf)) {
            return INPUT_RESULT_BACK;
        }

        trim_whitespace(buf);

        if (buf[0] == '\0') {
            if (hasValue && *hasValue) {
                return INPUT_RESULT_OK;
            }
            printf("\033[1;31mInvalid input. Please enter a non-negative integer for %s.\033[0m\n", field_name);
            continue;
        }

        char *endp = NULL;
        long parsed = strtol(buf, &endp, 10);
        if (endp == buf || *endp != '\0' || parsed < 0 || parsed > INT_MAX) {
            printf("\033[1;31mInvalid input. Please enter a non-negative integer for %s.\033[0m\n", field_name);
            continue;
        }

        if (value) {
            *value = (int)parsed;
        }
        if (hasValue) {
            *hasValue = 1;
        }

        return INPUT_RESULT_OK;
    }
}
////////////////////////


// Main function
int main(){
    // Enable UTF-8 support for Windows console
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // Windows-specific
    #endif

    #ifdef SIGTSTP
    signal(SIGTSTP, SIG_IGN); // Ignore Ctrl+Z suspend to handle it manually
    #endif

    // Load products from CSV file
    if(load_csv("products.csv")){
        printf("Failed to load CSV file.\n");
        return 1;
    };

    // Launch Product Order Manager as the main interface
    menu_product_manager();

    // Free allocated memory
    free(products);
    return 0;
}

// Load products from CSV file then store in products struct
int load_csv(const char *filename){
    FILE *fp;
    char line[1024];
    
    // Check if file opens successfully
    if(!(fp = fopen(filename, "r"))){
        perror("fopen");
        return 1;
    }

    // Skip the header line
    fgets(line, sizeof(line), fp);

    // Read each line from the CSV file
    while(fgets(line, sizeof(line), fp)){

        // Dynamically allocate or reallocate memory for products
        if (product_count == product_capacity) {
            product_capacity = product_capacity == 0 ? 10 : product_capacity * 2;
            products = realloc(products, product_capacity * sizeof(Product));
            if (!products) {
                perror("realloc");
                fclose(fp);
                return 1;
            }
        }

        line[strcspn(line, "\r\n")] = '\0'; // Remove newline characters
        char *token = strtok(line, ","); // Tokenize by comma
        
        // Parse each token and store in the struct
        int index = 0;
        while (token){
            switch(index){
                case 0:
                    strcpy(products[product_count].ProductID, token);
                    break;
                case 1:
                    strcpy(products[product_count].ProductName, token);
                    break;
                case 2:
                    products[product_count].Quantity = atoi(token); // Convert string to integer using atoi
                    break;
                case 3:
                    products[product_count].UnitPrice = atoi(token);
                    break;
                default:
                    break;
            }

            token = strtok(NULL, ","); // Get next token
            index++; // Increment index
        }

        product_count++; // Increment product count
    }

    fclose(fp);
    return 0;
}

// add product
int add_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice){
    if (!ProductName) {
        return 1;
    }

    int has_non_whitespace = 0;
    for (const char *p = ProductName; *p; ++p) {
        if (!isspace((unsigned char)*p)) {
            has_non_whitespace = 1;
            break;
        }
    }
    if (!has_non_whitespace) {
        return 1;
    }

    // Check for duplicate ProductID
    for(int i=0; i<product_count; i++){
        if(strcmp(products[i].ProductID, ProductID) == 0){
            return 1; // Duplicate found
        }
    }

    // Dynamically allocate or reallocate memory for products
    if (product_count == product_capacity) {
        product_capacity = product_capacity == 0 ? 10 : product_capacity * 2; // Double the capacity if needed or set to 10 if it's the first allocation
        products = realloc(products, product_capacity * sizeof(Product)); // Reallocate memory
        if (!products) {
            perror("realloc");
            return 1;
        }
    }

    // Add new product
    strcpy(products[product_count].ProductID, ProductID);
    strcpy(products[product_count].ProductName, ProductName);
    products[product_count].Quantity = Quantity;
    products[product_count].UnitPrice = UnitPrice;
    product_count++;

    // Save to CSV file
    if(save_csv("products.csv")){
        printf("Failed to save CSV file.\n");
        return 1;
    };

    return 0;
}

// remove product by ProductID
int remove_product(const char *ProductID){
    if (!ProductID){
        return 1;
    }

    for (int i = 0; i < product_count; i++){
        if (strcmp(products[i].ProductID, ProductID) == 0){
            for (int j = i; j < product_count - 1; j++){
                products[j] = products[j + 1];
            }
            product_count--;
            return 0;
        }
    }

    return 1;
}

// find matching products by keyword (case-insensitive)
int find_products_by_keyword(const char *keyword, int **out_matches){
    if (!keyword || !out_matches){
        return -1;
    }

    *out_matches = NULL;

    size_t keyword_len = strlen(keyword);
    char *keyword_lower = (char*)malloc(keyword_len + 1);
    if (!keyword_lower){
        return -1;
    }

    for (size_t i = 0; i < keyword_len; i++){
        keyword_lower[i] = (char)tolower((unsigned char)keyword[i]);
    }
    keyword_lower[keyword_len] = '\0';

    int capacity = product_count > 0 ? product_count : 1;
    int *matches = (int*)malloc(sizeof(int) * capacity);
    if (!matches){
        free(keyword_lower);
        return -1;
    }

    int count = 0;
    for (int i = 0; i < product_count; i++){
        char id_lower[sizeof(products[i].ProductID)];
        char name_lower[sizeof(products[i].ProductName)];

        strcpy(id_lower, products[i].ProductID);
        strcpy(name_lower, products[i].ProductName);

        for (int j = 0; id_lower[j]; j++){
            id_lower[j] = (char)tolower((unsigned char)id_lower[j]);
        }
        for (int j = 0; name_lower[j]; j++){
            name_lower[j] = (char)tolower((unsigned char)name_lower[j]);
        }

        if (strstr(id_lower, keyword_lower) != NULL || strstr(name_lower, keyword_lower) != NULL){
            matches[count++] = i;
        }
    }

    free(keyword_lower);

    if (count == 0){
        free(matches);
        matches = NULL;
    }

    *out_matches = matches;
    return count;
}

// update product by ProductID
int update_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice){
    // Find product by ProductID then update it
    for(int i=0; i<product_count; i++){
        if(strcmp(products[i].ProductID, ProductID) == 0){
            if(ProductName != NULL){
                int has_non_whitespace = 0;
                for (const char *p = ProductName; *p; ++p) {
                    if (!isspace((unsigned char)*p)) {
                        has_non_whitespace = 1;
                        break;
                    }
                }
                if (!has_non_whitespace) {
                    return 1;
                }
                strcpy(products[i].ProductName, ProductName);
            }
            if(Quantity >= 0){
                products[i].Quantity = Quantity;
            }
            if(UnitPrice >= 0){
                products[i].UnitPrice = UnitPrice;
            }
            return 0;
        }
    }
    return 1;
}

// save products to CSV file
int save_csv(const char *filename){
    FILE *fp;

    // Check if file opens successfully
    if(!(fp = fopen(filename, "w"))){
        perror("fopen");
        return 1;
    }

    // Write header
    fprintf(fp, "ProductID,ProductName,Quantity,UnitPrice\n");

    // Write each product
    for(int i=0; i<product_count; i++){
        fprintf(fp, "%s,%s,%d,%d\n", products[i].ProductID, products[i].ProductName, products[i].Quantity, products[i].UnitPrice);
    }

    fclose(fp);
    return 0;
}

// add new product
void menu_add_product(){
    char ProductID[20] = "";
    char ProductName[100] = "";
    int Quantity = 0;
    int UnitPrice = 0;
    int hasProductID = 0;
    int hasProductName = 0;
    int hasQuantity = 0;
    int hasUnitPrice = 0;
    int stage = 0;

    clear_screen();
    printf("\033[1m");
    printf("── Product Order Manager | Add Product ───────────────────────\n\n");
    printf("\033[0m");

    while (stage >= 0 && stage < 4) {
        InputResult result;

        clear_screen();
        printf("\033[1m── Product Order Manager | Add Product ───────────────────────\033[0m\n\n");
        printf("Enter Product ID: %s\n", hasProductID ? ProductID : "");
        printf("Enter Product Name: %s\n", hasProductName ? ProductName : "");
        if (hasQuantity) {
            printf("Enter Quantity: %d\n", Quantity);
        } else {
            printf("Enter Quantity: \n");
        }
        if (hasUnitPrice) {
            printf("Enter Unit Price: %d\n", UnitPrice);
        } else {
            printf("Enter Unit Price: \n");
        }
        printf("\n");

        switch (stage) {
            case 0:
                result = prompt_product_id(ProductID, sizeof(ProductID), &hasProductID);
                if (result == INPUT_RESULT_CANCEL) {
                    return;
                }
                if (result == INPUT_RESULT_BACK) {
                    continue;
                }
                stage = 1;
                break;

            case 1:
                result = prompt_product_name(ProductName, sizeof(ProductName), &hasProductName);
                if (result == INPUT_RESULT_CANCEL) {
                    return;
                }
                if (result == INPUT_RESULT_BACK) {
                    stage = 0;
                    continue;
                }
                stage = 2;
                break;

            case 2:
                result = prompt_integer_input("Enter Quantity", "Quantity", &Quantity, &hasQuantity);
                if (result == INPUT_RESULT_CANCEL) {
                    return;
                }
                if (result == INPUT_RESULT_BACK) {
                    stage = 1;
                    continue;
                }
                stage = 3;
                break;

            case 3:
                result = prompt_integer_input("Enter Unit Price", "Unit Price", &UnitPrice, &hasUnitPrice);
                if (result == INPUT_RESULT_CANCEL) {
                    return;
                }
                if (result == INPUT_RESULT_BACK) {
                    stage = 2;
                    continue;
                }
                stage = 4;
                break;

            default:
                return;
        }
    }

    if (stage != 4) {
        return;
    }

    if(add_product(ProductID, ProductName, Quantity, UnitPrice) == 0){
        printf("\n\033[1;32mProduct added successfully!\033[0m\n");
    } else {
        printf("\033[1;31mFailed to add product. Ensure the Product ID is unique and the name is not empty.\033[0m\n");
    }
}

static EditProductResult edit_product_prompt(Product *prod) {
    if (!prod) {
        return EDIT_PRODUCT_CANCELLED;
    }

    char ProductName[100];
    strncpy(ProductName, prod->ProductName, sizeof(ProductName) - 1);
    ProductName[sizeof(ProductName) - 1] = '\0';
    int Quantity = prod->Quantity;
    int UnitPrice = prod->UnitPrice;
    int hasProductName = 1;
    int hasQuantity = 1;
    int hasUnitPrice = 1;
    int stage = 0;
    const char *status_msg = NULL;

    while (stage >= 0 && stage < 3) {
        clear_screen();
        printf("\033[1m── Product Order Manager | Update Product ────────────────────\033[0m\n\n");
        printf("\033[1;33mProduct ID:\033[0m %s\n", prod->ProductID);

        const char *name_marker = stage == 0 ? "\033[1;33m>\033[0m" : "  ";
        const char *qty_marker  = stage == 1 ? "\033[1;33m>\033[0m" : "  ";
        const char *price_marker = stage == 2 ? "\033[1;33m>\033[0m" : "  ";

        printf("%s \033[1;32mProduct Name:\033[0m %s\n",
               name_marker,
               hasProductName ? ProductName : "");
        if (hasQuantity) {
            printf("%s \033[1;32mQuantity:\033[0m %d\n", qty_marker, Quantity);
        } else {
            printf("%s \033[1;32mQuantity:\033[0m \n", qty_marker);
        }
        if (hasUnitPrice) {
            printf("%s \033[1;32mUnit Price:\033[0m %d\n", price_marker, UnitPrice);
        } else {
            printf("%s \033[1;32mUnit Price:\033[0m \n", price_marker);
        }

        printf("\n\033[1;32mTip:\033[0m Use Ctrl+Z to go back, Ctrl+X to cancel. Highlighted field shows the current step.\n\n");
        if (status_msg) {
            printf("%s\n\n", status_msg);
        }
        status_msg = NULL;

        InputResult result;
        switch (stage) {
            case 0:
                result = prompt_product_name(ProductName, sizeof(ProductName), &hasProductName);
                if (result == INPUT_RESULT_CANCEL) {
                    return EDIT_PRODUCT_CANCELLED;
                }
                if (result == INPUT_RESULT_BACK) {
                    status_msg = "\033[1;33mAlready at the first input.\033[0m";
                    continue;
                }
                stage = 1;
                break;

            case 1:
                result = prompt_integer_input("Enter Quantity", "Quantity", &Quantity, &hasQuantity);
                if (result == INPUT_RESULT_CANCEL) {
                    return EDIT_PRODUCT_CANCELLED;
                }
                if (result == INPUT_RESULT_BACK) {
                    stage = 0;
                    continue;
                }
                stage = 2;
                break;

            case 2:
                result = prompt_integer_input("Enter Unit Price", "Unit Price", &UnitPrice, &hasUnitPrice);
                if (result == INPUT_RESULT_CANCEL) {
                    return EDIT_PRODUCT_CANCELLED;
                }
                if (result == INPUT_RESULT_BACK) {
                    stage = 1;
                    continue;
                }
                stage = 3;
                break;

            default:
                return EDIT_PRODUCT_CANCELLED;
        }
    }

    if (stage != 3) {
        return EDIT_PRODUCT_CANCELLED;
    }

    EditProductResult result = EDIT_PRODUCT_FAILED;
    if (update_product(prod->ProductID, ProductName, Quantity, UnitPrice) == 0) {
        result = EDIT_PRODUCT_UPDATED;
        if (save_csv("products.csv") == 0) {
            printf("\n\033[1;32mProduct updated successfully!\033[0m\n");
        } else {
            printf("\n\033[1;33mUpdated in memory, but failed to save CSV.\033[0m\n");
        }
    } else {
        printf("\n\033[1;31mFailed to update product.\033[0m\n");
        result = EDIT_PRODUCT_FAILED;
    }

    printf("──────────────────────────────────────────────────────────────\n");
    return result;
}

static ProductActionResult product_manager_handle_action(int product_index, char *status_buf, size_t status_len) {
    if (status_buf && status_len > 0) {
        status_buf[0] = '\0';
    }

    if (product_index < 0 || product_index >= product_count) {
        if (status_buf && status_len > 0) {
            snprintf(status_buf, status_len, "\033[1;31mProduct not found.\033[0m");
        }
        return PRODUCT_ACTION_NONE;
    }

    const int action_values[] = {1, 2, 0};
    const char *action_numbers[] = {"[1]", "[2]", "[0]"};
    const char *action_labels[] = {
        "Update product",
        "Remove product",
        "Back"
    };
    const int action_count = (int)(sizeof(action_values) / sizeof(action_values[0]));

    int selected = 0;
    const char *local_msg = NULL;

    while (1) {
        if (product_index < 0 || product_index >= product_count) {
            if (status_buf && status_len > 0) {
                snprintf(status_buf, status_len, "\033[1;31mProduct no longer available.\033[0m");
            }
            return PRODUCT_ACTION_NONE;
        }

        Product *prod = &products[product_index];

        clear_screen();
        printf("\033[1m── Product Order Manager | Actions ────────────────────────────────\033[0m\n\n");
        printf("\033[1;33mProduct ID:\033[0m %s\n", prod->ProductID);
        printf("\033[1;33mName:\033[0m %s\n", prod->ProductName);
        printf("\033[1;33mQuantity:\033[0m %d\n", prod->Quantity);
        printf("\033[1;33mUnit Price:\033[0m %d\n\n", prod->UnitPrice);

        printf("Choose an action:\n");
        for (int i = 0; i < action_count; i++) {
            if (i == selected) {
                printf("\033[1;32m> %s %s\033[0m\n", action_numbers[i], action_labels[i]);
            } else {
                printf("  \033[1;32m%s\033[0m %s\n", action_numbers[i], action_labels[i]);
            }
        }

        printf("\nUse arrows or number keys. Enter to confirm.\n");
        if (local_msg) {
            printf("%s\n", local_msg);
            local_msg = NULL;
        }

        int digit = -1;
        char typed = '\0';
        MenuKey key = read_menu_key(&digit, &typed);

        switch (key) {
            case MENU_KEY_UP:
                selected = (selected - 1 + action_count) % action_count;
                break;
            case MENU_KEY_DOWN:
                selected = (selected + 1) % action_count;
                break;
            case MENU_KEY_DIGIT: {
                int found = 0;
                for (int i = 0; i < action_count; i++) {
                    if (action_values[i] == digit) {
                        selected = i;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    local_msg = "\033[1;31mInvalid choice.\033[0m";
                }
                break;
            }
            case MENU_KEY_ENTER: {
                int choice = action_values[selected];
                switch (choice) {
                    case 1: {
                        EditProductResult edit_res = edit_product_prompt(prod);
                        if (edit_res == EDIT_PRODUCT_UPDATED) {
                            wait_for_enter();
                            if (status_buf && status_len > 0) {
                                snprintf(status_buf, status_len, "\033[1;32mProduct updated.\033[0m");
                            }
                            return PRODUCT_ACTION_UPDATED;
                        } else if (edit_res == EDIT_PRODUCT_FAILED) {
                            wait_for_enter();
                            local_msg = "\033[1;31mUpdate failed.\033[0m";
                        } else {
                            local_msg = "\033[1;33mUpdate cancelled.\033[0m";
                        }
                        break;
                    }
                    case 2: {
                        char id_copy[sizeof(prod->ProductID)];
                        char name_copy[sizeof(prod->ProductName)];
                        strncpy(id_copy, prod->ProductID, sizeof(id_copy) - 1);
                        id_copy[sizeof(id_copy) - 1] = '\0';
                        strncpy(name_copy, prod->ProductName, sizeof(name_copy) - 1);
                        name_copy[sizeof(name_copy) - 1] = '\0';
                        int qty = prod->Quantity;
                        int price = prod->UnitPrice;

                        clear_screen();
                        printf("\033[1m── Product Order Manager | Remove ────────────────────────────────\033[0m\n\n");
                        printf("Removing: %s | %s (Qty %d, Price %d)\n", id_copy, name_copy, qty, price);
                        printf("Type y to confirm, n to cancel, or press \033[1;31mCtrl+X\033[0m to abort: ");
                        printf("\033[1;33m");
                        char confirm[32];
                        if (!fgets(confirm, sizeof(confirm), stdin)) {
                            printf("\033[0m\n");
                            local_msg = "\033[1;33mRemoval cancelled.\033[0m";
                            break;
                        }
                        printf("\033[0m\n");
                        confirm[strcspn(confirm, "\r\n")] = '\0';

                        if (input_is_ctrl_x(confirm) || confirm[0] == '\0') {
                            local_msg = "\033[1;33mRemoval cancelled.\033[0m";
                            break;
                        }

                        if (!(confirm[0] == 'y' || confirm[0] == 'Y')) {
                            local_msg = "\033[1;33mRemoval cancelled.\033[0m";
                            break;
                        }

                        if (remove_product(id_copy) == 0) {
                            if (save_csv("products.csv") == 0) {
                                printf("\033[1;32mRemoved successfully.\033[0m\n");
                                if (status_buf && status_len > 0) {
                                    snprintf(status_buf, status_len, "\033[1;32mProduct removed.\033[0m");
                                }
                            } else {
                                printf("\033[1;33mRemoved in memory, failed to save CSV.\033[0m\n");
                                if (status_buf && status_len > 0) {
                                    snprintf(status_buf, status_len, "\033[1;33mProduct removed, but CSV save failed.\033[0m");
                                }
                            }
                            wait_for_enter();
                            return PRODUCT_ACTION_REMOVED;
                        } else {
                            printf("\033[1;31mFailed to remove product.\033[0m\n");
                            wait_for_enter();
                            local_msg = "\033[1;31mRemoval failed.\033[0m";
                        }
                        break;
                    }
                    default:
                        if (status_buf && status_len > 0) {
                            status_buf[0] = '\0';
                        }
                        return PRODUCT_ACTION_NONE;
                }
                break;
            }
            default:
                break;
        }
    }
}

void menu_product_manager(){
    const int run_tests_index = 0;
    const int exit_index = 1;
    const int add_product_index = 2;
    const int product_start_index = 3;

    int selected = (product_count > 0) ? product_start_index : add_product_index;
    char filter[128];
    filter[0] = '\0';
    char status_msg[256];
    status_msg[0] = '\0';
    int running = 1;
    int product_offset = 0;

    while (running) {
        int *matches = NULL;
        int mcount = find_products_by_keyword(filter, &matches);
        if (mcount < 0) {
            clear_screen();
            printf("\033[1m── Product Order Manager ───────────────────────────────────────────\n\n\033[0m");
            printf("\033[1;31mMemory allocation failed.\033[0m\n");
            wait_for_enter();
            return;
        }

        int display_count = mcount + 3; // Run tests, Exit, Add product, and product rows
        if (display_count == 0) {
            display_count = 1;
        }
        if (selected >= display_count) {
            selected = display_count - 1;
        }
        if (selected < 0) {
            selected = 0;
        }

        if (mcount == 0) {
            product_offset = 0;
        }

        int terminal_rows = get_terminal_rows();
        int status_lines = (status_msg[0] != '\0') ? 1 : 0;
        int reserved_lines = 9 + status_lines; // header + filter info + instructions + blank + run tests + exit + spacer + add + table header (+status)
        int available_rows = terminal_rows - reserved_lines;
        if (available_rows < 1) {
            available_rows = 1;
        }

        int product_rows_capacity = available_rows - 1; // keep headroom so title stays visible on cramped terminals
        if (product_rows_capacity < 1) {
            product_rows_capacity = 1;
        }

        int items_per_page = (mcount > 0) ? product_rows_capacity : 1;
        if (mcount > 0 && items_per_page > mcount) {
            items_per_page = mcount;
        }
        if (mcount > 0 && product_offset >= mcount) {
            product_offset = mcount - items_per_page;
            if (product_offset < 0) {
                product_offset = 0;
            }
        }

        if (mcount > 0 && selected >= product_start_index && selected < product_start_index + mcount) {
            int relative_index = selected - product_start_index;
            if (relative_index < product_offset) {
                product_offset = relative_index;
            } else if (relative_index >= product_offset + items_per_page) {
                product_offset = relative_index - items_per_page + 1;
            }
        }

        int visible_count = 0;
        int has_more_above = 0;
        int has_more_below = 0;
        if (mcount > 0) {
            int max_visible = items_per_page;
            if (max_visible < 1) {
                max_visible = 1;
            }

            for (;;) {
                if (product_offset < 0) {
                    product_offset = 0;
                }

                if (product_offset > mcount - 1) {
                    product_offset = mcount - 1;
                }

                visible_count = mcount - product_offset;
                if (visible_count > max_visible) {
                    visible_count = max_visible;
                }

                has_more_above = (product_offset > 0);
                has_more_below = (product_offset + visible_count < mcount);

                int total_lines = visible_count + (has_more_above ? 1 : 0) + (has_more_below ? 1 : 0);
                while (total_lines > max_visible && visible_count > 0) {
                    if (has_more_below) {
                        visible_count--;
                        has_more_below = (product_offset + visible_count < mcount);
                    } else if (has_more_above) {
                        visible_count--;
                        if (visible_count == 0 && mcount > 0) {
                            visible_count = 1;
                            has_more_above = 0;
                        }
                    } else {
                        break;
                    }
                    total_lines = visible_count + (has_more_above ? 1 : 0) + (has_more_below ? 1 : 0);
                }

                if (visible_count < 1) {
                    visible_count = 1;
                    has_more_above = (product_offset > 0);
                    has_more_below = (product_offset + visible_count < mcount);
                }

                if (selected >= product_start_index && selected < product_start_index + mcount) {
                    int relative_index = selected - product_start_index;
                    if (relative_index < product_offset) {
                        product_offset = relative_index;
                        continue;
                    }
                    if (relative_index >= product_offset + visible_count) {
                        product_offset = relative_index - visible_count + 1;
                        if (product_offset < 0) {
                            product_offset = 0;
                        }
                        continue;
                    }
                }

                if (visible_count + (has_more_above ? 1 : 0) + (has_more_below ? 1 : 0) > max_visible) {
                    if (has_more_below) {
                        has_more_below = 0;
                    } else if (has_more_above) {
                        has_more_above = 0;
                    }
                }

                break;
            }
        }

        int total_pages = 1;
        int current_page = 1;
        int start_display = 0;
        int end_display = 0;
        if (mcount > 0 && items_per_page > 0) {
            total_pages = (mcount + items_per_page - 1) / items_per_page;
            if (total_pages < 1) {
                total_pages = 1;
            }
            current_page = (product_offset / items_per_page) + 1;
            if (current_page > total_pages) {
                current_page = total_pages;
            }
            start_display = product_offset + 1;
            end_display = product_offset + visible_count;
        }

        clear_screen();
        printf("\033[1;33m── Product Order Manager ───────────────────────────────────────────\033[0m\n");
        const char *filter_display = filter[0] ? filter : "<none>";
        if (mcount > 0) {
            printf("Filter: \033[1;32m%s\033[0m | Matches: %d | Page %d/%d (%d-%d of %d)\n",
                   filter_display,
                   mcount,
                   current_page,
                   total_pages,
                   start_display,
                   end_display,
                   mcount);
        } else {
            printf("Filter: \033[1;31m%s\033[0m | Matches: 0\n", filter_display);
        }
        printf("Use arrows to navigate. Type to filter, Backspace to erase, Enter selects.\n");
        printf("\n");

        if (selected == run_tests_index) {
            printf("\033[1;34m> [*] Run unit tests\033[0m\n");
        } else {
            printf("  [*] Run unit tests\n");
        }

        if (selected == exit_index) {
            printf("\033[1;31m> [0] Exit application\033[0m\n");
        } else {
            printf("  [0] Exit application\n");
        }

        printf("\n");

        if (selected == add_product_index) {
            printf("\033[1;32m> [+] Add new product\033[0m\n");
        } else {
            printf("  [+] Add new product\n");
        }

        printf("\033[1;33m  #  %-10s %-20s %10s %10s\033[0m\n", "ProductID", "ProductName", "Quantity", "UnitPrice");

        if (mcount == 0) {
            printf("  (no products to display)\n");
        } else {
            if (has_more_above) {
                printf("  ↑  more products above\n");
            }
            for (int i = 0; i < visible_count; i++) {
                int match_index = product_offset + i;
                int idx = matches[match_index];
                int display_index = match_index + 1;
                if (selected == product_start_index + match_index) {
                    printf("\033[1;32m> %2d %-10.10s %-20.20s %10d %10d\033[0m\n",
                           display_index,
                           products[idx].ProductID,
                           products[idx].ProductName,
                           products[idx].Quantity,
                           products[idx].UnitPrice);
                } else {
                    printf("  %2d %-10.10s %-20.20s %10d %10d\n",
                           display_index,
                           products[idx].ProductID,
                           products[idx].ProductName,
                           products[idx].Quantity,
                           products[idx].UnitPrice);
                }
            }
            if (has_more_below) {
                printf("  ↓  more products below\n");
            }
        }

        if (status_msg[0] != '\0') {
            printf("%s\n", status_msg);
            status_msg[0] = '\0';
        }

        int digit = -1;
        char typed = '\0';
        MenuKey key = read_menu_key(&digit, &typed);

        int chosen_index = -1;
        size_t filter_len = strlen(filter);

        switch (key) {
            case MENU_KEY_UP:
                if (display_count > 0) {
                    selected = (selected - 1 + display_count) % display_count;
                }
                break;
            case MENU_KEY_DOWN:
                if (display_count > 0) {
                    selected = (selected + 1) % display_count;
                }
                break;
            case MENU_KEY_ENTER:
                if (selected == add_product_index) {
                    int before_count = product_count;
                    free(matches);
                    matches = NULL;
                    menu_add_product();
                    wait_for_enter();
                    if (product_count > before_count) {
                        snprintf(status_msg, sizeof(status_msg), "\033[1;32mProduct added.\033[0m");
                        selected = (product_count > 0) ? product_start_index : add_product_index;
                        filter[0] = '\0';
                    } else {
                        snprintf(status_msg, sizeof(status_msg), "\033[1;33mNo product added.\033[0m");
                    }
                    product_offset = 0;
                    continue;
                } else if (selected == run_tests_index) {
                    free(matches);
                    matches = NULL;
                    clear_screen();
                    int tests_result = run_unit_tests();
                    wait_for_enter();
                    if (tests_result == 0) {
                        snprintf(status_msg, sizeof(status_msg), "\033[1;32mUnit tests passed.\033[0m");
                    } else {
                        snprintf(status_msg, sizeof(status_msg), "\033[1;31mUnit tests failed.\033[0m");
                    }
                    selected = (product_count > 0) ? product_start_index : add_product_index;
                    filter[0] = '\0';
                    product_offset = 0;
                    continue;
                } else if (selected == exit_index) {
                    free(matches);
                    matches = NULL;
                    running = 0;
                    continue;
                }
                if (mcount > 0 && selected >= product_start_index && selected < product_start_index + mcount) {
                    chosen_index = matches[selected - product_start_index];
                }
                break;
            case MENU_KEY_DIGIT:
                if (filter_len < sizeof(filter) - 1) {
                    filter[filter_len] = (char)('0' + digit);
                    filter[filter_len + 1] = '\0';
                    selected = (mcount > 0) ? product_start_index : add_product_index;
                    product_offset = 0;
                }
                break;
            case MENU_KEY_CHAR:
                if (filter_len < sizeof(filter) - 1 && typed != '\0') {
                    filter[filter_len] = typed;
                    filter[filter_len + 1] = '\0';
                    selected = (mcount > 0) ? product_start_index : add_product_index;
                    product_offset = 0;
                }
                break;
            case MENU_KEY_BACKSPACE:
                if (filter_len > 0) {
                    filter[filter_len - 1] = '\0';
                    selected = (mcount > 0) ? product_start_index : add_product_index;
                    product_offset = 0;
                }
                break;
            default:
                break;
        }

        if (chosen_index >= 0) {
            ProductActionResult action = product_manager_handle_action(chosen_index, status_msg, sizeof(status_msg));
            if (action == PRODUCT_ACTION_REMOVED) {
                selected = (product_count > 0) ? product_start_index : add_product_index;
                product_offset = 0;
            }
        }

        free(matches);
    }
}
