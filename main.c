#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char ProductID[20];
    char ProductName[100];
    int Quantity;
    int UnitPrice;
} Product;

Product products[100];
int product_count = 0;

void load_csv(const char *filename);

int main(){
    load_csv("products.csv");
    for(int i=0; i<product_count; i++){
        printf("ProductID: %s, ProductName: %s, Quantity: %d, UnitPrice: %d\n", products[i].ProductID, products[i].ProductName, products[i].Quantity, products[i].UnitPrice);
    }
    return 0;
}

void load_csv(const char *filename){
    FILE *fp;
    char line[1024];
    

    // Check if file opens successfully
    if(!(fp = fopen(filename, "r"))){
        perror("fopen");
        return;
    }

    // Skip the header line
    fgets(line, sizeof(line), fp);

    while(fgets(line, sizeof(line), fp)){
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
                    products[product_count].UnitPrice = atoi(token);
                    break;
                case 3:
                    products[product_count].Quantity = atoi(token);
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
}