#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char ProductID[20];
    char ProductName[100];
    int Quantity;
    int UnitPrice;
} Product;

Product *products = NULL;
int product_count = 0;
int product_capacity = 0;

int load_csv(const char *filename);
int add_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice);

int main(){
    if(load_csv("products.csv")){
        printf("Failed to load CSV file.\n");
        return 1;
    };

    for(int i=0; i<product_count; i++){
        printf("ProductID: %s, ProductName: %s, Quantity: %d, UnitPrice: %d\n", products[i].ProductID, products[i].ProductName, products[i].Quantity, products[i].UnitPrice);
    }
    
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

int add_product(const char *ProductID, const char *ProductName, int Quantity, int UnitPrice){

    if (product_count == product_capacity) {
        product_capacity = product_capacity == 0 ? 10 : product_capacity * 2;
        products = realloc(products, product_capacity * sizeof(Product));
        if (!products) {
            perror("realloc");
            return 1;
        }
    }

    strcpy(products[product_count].ProductID, ProductID);
    strcpy(products[product_count].ProductName, ProductName);
    products[product_count].Quantity = Quantity;
    products[product_count].UnitPrice = UnitPrice;
    product_count++;
    return 0;
}