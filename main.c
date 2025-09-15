#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

void clear_screen() {
    printf("\033[2J\033[H");
}

void wait_for_enter() {
    printf("\033[1;32mPress Enter to back to menu...\033[0m");
    fflush(stdout);
    getchar();
}

// Main function
int main(){
    // Load products from CSV file
    if(load_csv("products.csv")){
        printf("Failed to load CSV file.\n");
        return 1;
    };

    menu();
    
    // Free allocated memory
    free(products);
    return 0;
}

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
                    products[product_count].Quantity = atoi(token);
                    break;
                case 3:
                    products[product_count].UnitPrice = atoi(token);
                    break;
                default:
                    break;
            }

            token = strtok(NULL, ",");
            index++;
        }

        product_count++;
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
        printf("\033[1;32m [2] \033[0mAdd a new product\n");
        printf("\033[1;32m [3] \033[0mRemove a product\n");
        printf("\033[1;32m [4] \033[0mUpdate a product\n");
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
            case 1:
                clear_screen();
                menu_list_products();
                wait_for_enter();
                break;

            case 2:
                menu_add_product();
                wait_for_enter();
                break;

            case 3:
                printf("Removing a product...\n");
                wait_for_enter();
                break;

            case 4:
                printf("Updating a product...\n");
                wait_for_enter();
                break;

            case 0:
                printf("Exiting the program...\n");
                break;

            default:
                printf("Invalid choice! Please try again.\n");
                break;
        }
    } while (choice != 0);
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
        product_capacity = product_capacity == 0 ? 10 : product_capacity * 2;
        products = realloc(products, product_capacity * sizeof(Product));
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

    if(save_csv("products.csv")){
        printf("Failed to save CSV file.\n");
        return 1;
    };

    return 0;
}

// remove product by ProductID
int remove_product(const char *ProductID){
    // Find product by ProductID then remove it
    for(int i=0; i<product_count; i++){
        if(strcmp(products[i].ProductID, ProductID) == 0){
            for(int j=i; j<product_count-1; j++){
                products[j] = products[j+1];
            }
            product_count--;
            return 0;     
        }
    }
    return 1;
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

int save_csv(const char *filename){
    FILE *fp;

    if(!(fp = fopen(filename, "w"))){
        perror("fopen");
        return 1;
    }

    fprintf(fp, "ProductID,ProductName,Quantity,UnitPrice\n");

    for(int i=0; i<product_count; i++){
        fprintf(fp, "%s,%s,%d,%d\n", products[i].ProductID, products[i].ProductName, products[i].Quantity, products[i].UnitPrice);
    }

    fclose(fp);
    return 0;
}

// list all products
void menu_list_products(){   
    printf("\033[1m"); 
    printf("── Product Order Manager | Listing ───────────────────────────\n");
    printf("\033[0m");
    printf("\033[1;33m");
    printf("%-10s %-20s %-10s %-10s\n", "ProductID", "ProductName", "Quantity", "UnitPrice");
    printf("\033[0m");
    for(int i=0; i<product_count; i++){
        printf("%-10s %-20s %-10d %-10d\n", products[i].ProductID, products[i].ProductName, products[i].Quantity, products[i].UnitPrice);
    }
    printf("──────────────────────────────────────────────────────────────\n");
}

void menu_add_product(){
    char ProductID[20];
    char ProductName[100];
    int Quantity;
    int UnitPrice;

    clear_screen();
    printf("\033[1m");
    printf("── Product Order Manager | Add Product ───────────────────────\n\n");
    printf("\033[0m");


    printf("Enter Product ID: ");
    printf("\033[1;33m");
    scanf("%s", ProductID);
    while(getchar() != '\n');
    printf("\033[0m");

    printf("Enter Product Name: ");
    printf("\033[1;33m");
    fgets(ProductName, sizeof(ProductName), stdin);
    ProductName[strcspn(ProductName, "\r\n")] = '\0';
    printf("\033[0m");


    printf("Enter Quantity: ");
    printf("\033[1;33m");
    while (scanf("%d", &Quantity) != 1 || Quantity < 0) {
        printf("\033[0m");
        printf("Invalid input. Please enter a integer for Quantity: ");
        while(getchar() != '\n');
    }
    while(getchar() != '\n');
    printf("\033[0m");

    printf("Enter Unit Price: ");
    printf("\033[1;33m");
    while (scanf("%d", &UnitPrice) != 1 || UnitPrice < 0) {
        printf("\033[0m");
        printf("Invalid input. Please enter a integer for Unit Price: ");
        while(getchar() != '\n');
    }
    while(getchar() != '\n');
    printf("\033[0m");

    if(add_product(ProductID, ProductName, Quantity, UnitPrice) == 0){
        printf("\n\033[1;32mProduct added successfully!\033[0m\n");
    } else {
        printf("\033[1;31mFailed to add product. Duplicate Product ID\033[0m\n");
    }
}