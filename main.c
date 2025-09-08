#include <stdio.h>
#include <string.h>

void load_csv(const char *filename);

int main(){
    load_csv("products.csv");
    return 0;
}

void load_csv(const char *filename){
    FILE *fp;
    char line[1024];

    fp = fopen(filename, "r");

    while (fgets(line, sizeof(line), fp)){
        char *token = strtok(line, ",");
        while (token){
            printf("%s ", token);
            token = strtok(NULL, ",");
        }
        printf("\n");
    }

    fclose(fp);
}