#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

// For Windows console UTF-8 support
#ifdef _WIN32
#include <windows.h>
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
void menu();
void menu_list_products();
void menu_add_product();
void menu_search_product();
void menu_remove_product();
void menu_update_product();
int find_products_by_keyword(const char *keyword, int **out_matches);
int input_is_ctrl_x(const char *input);
void trim_whitespace(char *str);
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

// Detect if the user pressed Ctrl+X (ASCII 24) or typed ^X manually
int input_is_ctrl_x(const char *input) {
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
    if (len == 1 && (unsigned char)input[0] == 0x18) {
        return 1;
    }
    if (len == 2 && input[0] == '^' && (input[1] == 'X' || input[1] == 'x')) {
        return 1;
    }

    return 0;
}
////////////////////////


// Main function
int main(){
    // Enable UTF-8 support for Windows console
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // Windows-specific
    #endif

    // Load products from CSV file
    if(load_csv("products.csv")){
        printf("Failed to load CSV file.\n");
        return 1;
    };

    // Show menu
    menu();
    
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

void menu() {
    int choice;

    do {
        clear_screen();
        printf("\033[0m");
        printf("\033[1m");
        printf("── Product Order Manager | Menu ───\n\n");
        printf("\033[0m");
        printf("\033[1;33mChoose an option:\033[0m\n");
        printf("\033[1;32m [1] \033[0mList all products\n");
        printf("\033[1;32m [2] \033[0mSearch products\n");
        printf("\033[1;32m [3] \033[0mAdd a new product\n");
        printf("\033[1;32m [4] \033[0mRemove a product\n");
        printf("\033[1;32m [5] \033[0mUpdate a product\n");
        printf("\033[1;32m [0] \033[0mExit\n\n");

        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            choice = -1;
        } else {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
        }

        switch (choice) {
            case 1: // List products
                clear_screen();
                printf("\033[1m"); 
                printf("── Product Order Manager | Listing ───────────────────────────\n");
                printf("\033[0m");
                menu_list_products();
                wait_for_enter();
                break;

            case 2: // Search products
                menu_search_product();
                wait_for_enter();
                break;

            case 3: // Add product
                menu_add_product();
                wait_for_enter();
                break;

            case 4: // Remove product
                menu_remove_product();
                wait_for_enter();
                break;
            case 5: // Update product
                menu_update_product();
                wait_for_enter();
                break;
            case 0: // Exit
                printf("Exiting the program...\n");
                break;
            default:
                printf("Invalid choice! Please try again.\n");
                break;
        }
    } while (choice != 0); // Loop until user chooses to exit
}

// add product
int add_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice){

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

// list all products
void menu_list_products(){   
    printf("\033[1;33m");
    printf("%-10s %-20s %-10s %-10s\n", "ProductID", "ProductName", "Quantity", "UnitPrice");
    printf("\033[0m");
    for(int i=0; i<product_count; i++){
        printf("%-10s %-20s %-10d %-10d\n", products[i].ProductID, products[i].ProductName, products[i].Quantity, products[i].UnitPrice);
    }
    printf("──────────────────────────────────────────────────────────────\n");
}

// search products by keyword
void menu_search_product(){
    clear_screen();
    char keyword[100];

    printf("\033[1m");
    printf("── Product Order Manager | Search ────────────────────────────\n\n");
    printf("\033[0m");

    printf("Enter Product ID or Name keyword or press \033[1;31mCtrl+X\033[0m to cancel: ");
    fgets(keyword, sizeof(keyword), stdin);
    keyword[strcspn(keyword, "\r\n")] = '\0'; // Remove newline characters

    // Check for exit command
    if(input_is_ctrl_x(keyword)){
        return;
    }

    int *matches = NULL;
    int mcount = find_products_by_keyword(keyword, &matches);
    if (mcount < 0){
        printf("\033[1;31mMemory allocation failed.\033[0m\n");
        return;
    }

    printf("\n\033[1;33m%-10s %-20s %-10s %-10s\033[0m\n",
           "ProductID","ProductName","Quantity","UnitPrice");

    if (mcount == 0) {
        printf("No product found.\n");
    } else {
        for (int i = 0; i < mcount; i++) {
            int idx = matches[i];
            printf("%-10s %-20s %-10d %-10d\n",
                products[idx].ProductID,
                products[idx].ProductName,
                products[idx].Quantity,
                products[idx].UnitPrice);
        }
    }

    printf("──────────────────────────────────────────────────────────────\n");
    free(matches);
}

// add new product
void menu_add_product(){
    char ProductID[20];
    char ProductName[100];
    int Quantity;
    int UnitPrice;
    char buf[256];

    clear_screen();
    printf("\033[1m");
    printf("── Product Order Manager | Add Product ───────────────────────\n\n");
    printf("\033[0m");

    while (1) {
        printf("Enter Product ID or press \033[1;31mCtrl+X\033[0m to cancel : ");
        printf("\033[1;33m");
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("\033[0m");
            return;
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("\033[0m");

        if (input_is_ctrl_x(buf)) {
            return;
        }

        trim_whitespace(buf);
        if (buf[0] == '\0') {
            printf("\033[1;31mProduct ID cannot be empty.\033[0m\n");
            continue;
        }
        if (strlen(buf) >= sizeof(ProductID)) {
            printf("\033[1;31mProduct ID is too long (max %zu characters).\033[0m\n", sizeof(ProductID) - 1);
            continue;
        }

        int duplicate = 0;
        for (int i = 0; i < product_count; i++) {
            if (strcmp(products[i].ProductID, buf) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            printf("\033[1;31mDuplicate Product ID. Please enter a different ID.\033[0m\n");
            continue;
        }

        strcpy(ProductID, buf);
        break;
    }

    printf("Enter Product Name: ");
    printf("\033[1;33m");
    if (!fgets(buf, sizeof(buf), stdin)) {
        printf("\033[0m");
        return;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
    printf("\033[0m");

    if (input_is_ctrl_x(buf)) {
        return;
    }

    trim_whitespace(buf);
    strncpy(ProductName, buf, sizeof(ProductName) - 1);
    ProductName[sizeof(ProductName) - 1] = '\0';

    while (1) {
        printf("Enter Quantity: ");
        printf("\033[1;33m");
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("\033[0m");
            return;
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("\033[0m");

        if (input_is_ctrl_x(buf)) {
            return;
        }

        trim_whitespace(buf);
        if (buf[0] == '\0') {
            printf("\033[1;31mInvalid input. Please enter a non-negative integer for Quantity.\033[0m\n");
            continue;
        }

        char *endp = NULL;
        long value = strtol(buf, &endp, 10);
        if (endp == buf || *endp != '\0' || value < 0 || value > INT_MAX) {
            printf("\033[1;31mInvalid input. Please enter a non-negative integer for Quantity.\033[0m\n");
            continue;
        }

        Quantity = (int)value;
        break;
    }

    while (1) {
        printf("Enter Unit Price: ");
        printf("\033[1;33m");
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("\033[0m");
            return;
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("\033[0m");

        if (input_is_ctrl_x(buf)) {
            return;
        }

        trim_whitespace(buf);
        if (buf[0] == '\0') {
            printf("\033[1;31mInvalid input. Please enter a non-negative integer for Unit Price.\033[0m\n");
            continue;
        }

        char *endp = NULL;
        long value = strtol(buf, &endp, 10);
        if (endp == buf || *endp != '\0' || value < 0 || value > INT_MAX) {
            printf("\033[1;31mInvalid input. Please enter a non-negative integer for Unit Price.\033[0m\n");
            continue;
        }

        UnitPrice = (int)value;
        break;
    }

    if(add_product(ProductID, ProductName, Quantity, UnitPrice) == 0){
        printf("\n\033[1;32mProduct added successfully!\033[0m\n");
    } else {
        printf("\033[1;31mFailed to add product. Duplicate Product ID\033[0m\n");
    }
}

// remove product by keyword search
void menu_remove_product(){
    char keyword[100];

    clear_screen();
    printf("\033[1m");
    printf("── Product Order Manager | Remove Product ────────────────────\n\n");
    printf("\033[0m");

    printf("Enter Product ID or Name keyword or press \033[1;31mCtrl+X\033[0m to cancel: ");
    printf("\033[1;33m");
    if (!fgets(keyword, sizeof(keyword), stdin)) {
        printf("\033[0m");
        return;
    }
    keyword[strcspn(keyword, "\r\n")] = '\0';
    printf("\033[0m");

    if(input_is_ctrl_x(keyword)){
        return;
    }

    int *matches = NULL;
    int mcount = find_products_by_keyword(keyword, &matches);
    if (mcount < 0) {
        printf("\033[1;31mMemory allocation failed.\033[0m\n");
        return;
    }

    printf("\n\033[1;33m#  %-10s %-20s %-10s %-10s\033[0m\n",
           "ProductID","ProductName","Quantity","UnitPrice");

    if (mcount == 0) {
        printf("No product found.\n");
        printf("──────────────────────────────────────────────────────────────\n");
        free(matches);
        return;
    }

    for (int i = 0; i < mcount; i++) {
        int idx = matches[i];
        printf("%-3d%-10s %-20s %-10d %-10d\n",
               i + 1,
               products[idx].ProductID,
               products[idx].ProductName,
               products[idx].Quantity,
               products[idx].UnitPrice);
    }

    char choice[256];
    printf("\nSelect item number(s) to remove (e.g. 1,3) or type \033[1;33mall\033[0m to remove all, or press \033[1;31mCtrl+X\033[0m to cancel: ");
    printf("\033[1;33m");
    if (!fgets(choice, sizeof(choice), stdin)) {
        printf("\033[0m");
        free(matches);
        return;
    }
    choice[strcspn(choice, "\r\n")] = '\0';
    printf("\033[0m");

    if (input_is_ctrl_x(choice)) {
        free(matches);
        printf("\n\033[1;33mOperation cancelled.\033[0m\n");
        return;
    }

    int *to_delete = (int*)malloc(sizeof(int) * mcount);
    if (!to_delete) {
        free(matches);
        printf("\033[1;31mMemory allocation failed.\033[0m\n");
        return;
    }
    int del_count = 0;

    if (strcasecmp(choice, "all") == 0) {
        for (int i = 0; i < mcount; i++) to_delete[del_count++] = matches[i];
    } else {
        char *token = strtok(choice, ",");
        while (token) {
            while (isspace((unsigned char)*token)) token++;
            int num = atoi(token);
            if (num >= 1 && num <= mcount) {
                int idx = matches[num - 1];
                int dup = 0;
                for (int k = 0; k < del_count; k++) if (to_delete[k] == idx) { dup = 1; break; }
                if (!dup) to_delete[del_count++] = idx;
            } else {
                printf("\033[1;31mIgnored invalid selection: %s\033[0m\n", token);
            }
            token = strtok(NULL, ",");
        }

        if (del_count == 0) {
            free(matches);
            free(to_delete);
            printf("\n\033[1;33mNothing selected. Operation cancelled.\033[0m\n");
            return;
        }
    }

    printf("\nYou are about to remove %d item(s):\n", del_count);
    for (int i = 0; i < del_count; i++) {
        int idx = to_delete[i];
        printf("\033[1;31m");
        printf(" - %s | %s\n", products[idx].ProductID, products[idx].ProductName);
        printf("\033[0m");
    }

    char confirm[8];
    printf("\nAre you sure? (y/n): ");
    printf("\033[1;33m");
    if (!fgets(confirm, sizeof(confirm), stdin)) {
        printf("\033[0m");
        free(matches); free(to_delete);
        return;
    }
    confirm[strcspn(confirm, "\r\n")] = '\0';
    printf("\033[0m");

    if (!(strcmp(confirm, "y") == 0 || strcmp(confirm, "Y") == 0)) {
        printf("\n\033[1;33mOperation cancelled.\033[0m\n");
        free(matches); free(to_delete);
        return;
    }

    for (int i = 0; i < del_count - 1; i++) {
        for (int j = i + 1; j < del_count; j++) {
            if (to_delete[i] < to_delete[j]) {
                int t = to_delete[i]; to_delete[i] = to_delete[j]; to_delete[j] = t;
            }
        }
    }

    int removed_ok = 0;
    for (int i = 0; i < del_count; i++) {
        int idx = to_delete[i];
        if (idx < 0 || idx >= product_count) {
            continue;
        }

        char id_copy[sizeof(products[0].ProductID)];
        strcpy(id_copy, products[idx].ProductID);

        if (remove_product(id_copy) == 0) {
            removed_ok++;
        } else {
            printf("\033[1;31mFailed to remove product ID %s.\033[0m\n", id_copy);
        }
    }

    if (removed_ok > 0) {
        if (save_csv("products.csv") == 0) {
            printf("\n\033[1;32mRemoved %d/%d item(s) successfully!\033[0m\n", removed_ok, del_count);
        } else {
            printf("\033[1;31mRemoved in memory, but failed to save CSV file.\033[0m\n");
        }
    } else {
        printf("\n\033[1;33mNo items were removed.\033[0m\n");
    }

    free(matches);
    free(to_delete);
    printf("──────────────────────────────────────────────────────────────\n");
}

// update product by keyword search
void menu_update_product(){
    clear_screen();
    printf("\033[1m");
    printf("── Product Order Manager | Update Product ────────────────────\n\n");
    printf("\033[0m");

    if (product_count == 0){
        printf("No products loaded.\n");
        return;
    }

    char keyword[128];
    printf("Enter Product ID or Name keyword or press \033[1;31mCtrl+X\033[0m to cancel: ");
    if (!fgets(keyword, sizeof(keyword), stdin)) return;
    keyword[strcspn(keyword, "\r\n")] = '\0';
    if (input_is_ctrl_x(keyword)) return;

    int *matches = NULL;
    int mcount = find_products_by_keyword(keyword, &matches);
    if (mcount < 0){
        printf("\033[1;31mMemory allocation failed.\033[0m\n");
        return;
    }

    printf("\n\033[1;33m#  %-10s %-20s %-10s %-10s\033[0m\n",
           "ProductID","ProductName","Quantity","UnitPrice");

    if (mcount == 0){
        printf("No product found.\n");
        printf("──────────────────────────────────────────────────────────────\n");
        free(matches);
        return;
    }

    for (int i = 0; i < mcount; i++){
        int idx = matches[i];
        printf("%-3d%-10s %-20s %-10d %-10d\n",
               i + 1,
               products[idx].ProductID,
               products[idx].ProductName,
               products[idx].Quantity,
               products[idx].UnitPrice);
    }

    char choice[32];
    int pick = -1;
    printf("\nSelect ONE item number to update or press \033[1;31mCtrl+X\033[0m to cancel: ");
    if (!fgets(choice, sizeof(choice), stdin)){ free(matches); return; }
    choice[strcspn(choice, "\r\n")] = '\0';
    if (input_is_ctrl_x(choice)){ free(matches); printf("\n\033[1;33mOperation cancelled.\033[0m\n"); return; }

    pick = atoi(choice);
    if (pick < 1 || pick > mcount){
        printf("\033[1;31mInvalid selection.\033[0m\n");
        free(matches);
        return;
    }

    int idx = matches[pick - 1];
    free(matches);

    printf("\nEditing: \033[1m%s\033[0m | %s (Qty %d, Price %d)\n",
           products[idx].ProductID,
           products[idx].ProductName,
           products[idx].Quantity,
           products[idx].UnitPrice);

    char buf[256];

    char *newNamePtr = NULL;
    char newName[100];
    printf("New Product Name (leave blank to keep): ");
    if (!fgets(buf, sizeof(buf), stdin)) return;
    buf[strcspn(buf, "\r\n")] = '\0';
    if (buf[0] != '\0'){
        strncpy(newName, buf, sizeof(newName)-1);
        newName[sizeof(newName)-1] = '\0';
        newNamePtr = newName;
    }

    int newQty = -1;
    while (1){
        printf("New Quantity (leave blank to keep): ");
        if (!fgets(buf, sizeof(buf), stdin)) return;
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0'){ // keep
            newQty = -1;
            break;
        }
        char *endp = NULL;
        long v = strtol(buf, &endp, 10);
        if (endp != buf && *endp == '\0' && v >= 0 && v <= INT_MAX){
            newQty = (int)v;
            break;
        }
        printf("\033[1;31mPlease enter a non-negative integer.\033[0m\n");
    }

    int newPrice = -1;
    while (1){
        printf("New Unit Price (leave blank to keep): ");
        if (!fgets(buf, sizeof(buf), stdin)) return;
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0'){ // keep
            newPrice = -1;
            break;
        }
        char *endp = NULL;
        long v = strtol(buf, &endp, 10);
        if (endp != buf && *endp == '\0' && v >= 0 && v <= INT_MAX){
            newPrice = (int)v;
            break;
        }
        printf("\033[1;31mPlease enter a non-negative integer.\033[0m\n");
    }

    if (update_product(products[idx].ProductID, newNamePtr, newQty, newPrice) == 0){
        if (save_csv("products.csv") == 0){
            printf("\n\033[1;32mProduct updated successfully!\033[0m\n");
        } else {
            printf("\n\033[1;33mUpdated in memory, but failed to save CSV.\033[0m\n");
        }
    } else {
        printf("\n\033[1;31mFailed to update product.\033[0m\n");
    }

    printf("──────────────────────────────────────────────────────────────\n");
}
