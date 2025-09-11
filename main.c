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

    while(fgets(line, sizeof(line), fp)){
        // printf("%s",line);

        line[strcspn(line, "\r\n")] = '\0';

        char *token = strtok(line, ",");
        //printf("%s",token);
        
        while (token){
            printf("%s ", token);
            token = strtok(NULL, ","); // Continue
        }
    }

    printf("\n");
    fclose(fp);

}