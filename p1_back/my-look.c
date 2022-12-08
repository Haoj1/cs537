#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>

#define     MAX_LEN     1000

int is_invalid = 0;
int is_readFile = 0;
char *argument = NULL;
char *filename = NULL;
char *buffer = NULL;
char *output = NULL;
FILE *fp = NULL;


void init_buffer(){
    buffer = (char *)malloc(MAX_LEN*sizeof(char));
    output = (char *)malloc(MAX_LEN*sizeof(char));
}

void free_buffer(){
    free(buffer);
    free(output);
    buffer = NULL;
    output = NULL;
}

int regular(char *line){
    char temp[MAX_LEN];
    int temp_length = 0;
    if(line == NULL){
        return 1;
    }
    for(int i = 0; i < strlen(line); i++){
        if(('a' <= line[i] && line[i] <= 'z') || ('A' <= line[i] && line[i] <= 'Z') || \
        ('0' <= line[i] && line[i] <= '9'))
        {
            temp[temp_length] = line[i];
            temp_length++;
        }
    }
    temp[temp_length] = '\0';
    memcpy(line, temp, MAX_LEN);
    for(int i = 0; i < strlen(line); i++){
        if('A' <= line[i] && line[i] <= 'Z'){
            line[i] = line[i] + 32;
        }
    }
    return 0;
}

void check_file(){
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("my-look: cannot open file\n");
        exit(1);
    }
    while(fgets(buffer, MAX_LEN, fp) != NULL){
        buffer[strlen(buffer)-1] = '\0';
        memcpy(output, buffer, strlen(buffer));
        regular(buffer);
        regular(argument);
        if(strstr(buffer, argument) != NULL){
            printf("%s\n", output);
        }
        memset(output, 0, MAX_LEN*sizeof(char));
    }
    fclose(fp);
}

void check_stdin(){
    while(fgets(buffer, MAX_LEN, stdin) != NULL){
        buffer[strlen(buffer)-1] = '\0';
        memcpy(output, buffer, strlen(buffer));
        regular(buffer);
        regular(argument);
        if(strstr(buffer, argument) != NULL){
            printf("%s\n", output);
        }
        memset(output, 0, MAX_LEN*sizeof(char));
    }
}

int main(int argc, char *argv[]){
    char option;
    opterr = 0;
    while((option = getopt(argc, argv, "Vhf:")) != -1){
        switch (option) {
            case 'V': {
                printf("my-look from CS537 Spring 2022\n");
                return 0;
            }
            case 'h': {
                printf("my-look [-f <filename> <String you looking for>]\n");
                return 0;
            }
            case 'f': {
                if(optarg[0] == '-'){
                    printf("my-look: invalid command line\n");
                    return 1;
                }
                is_readFile = 1;
                filename = optarg;
                break;
            }
            case '?': {
                printf("my-look: invalid command line\n");
                return 1;
            }
        }
    }
    if(optind == argc-1){
        argument = argv[optind];
    }
    else{
        printf("my-look: invalid command line\n");
        return 1;
    }
    /*
    printf("%s\n", filename);
    printf("%s\n", argument);
    for(int i = 1; i < argc; i++){
        if(argv[i][0] == '-'){
            if(strcmp("-V", argv[i]) == 0){
                printf("my-look from CS537 Spring 2022\n");
                return 0;
            }
            else if(strcmp("-h", argv[i]) == 0){
                printf("my-look [-f <filename> <String you looking for>]\n");
                return 0;
            }
            else if(strcmp("-f", argv[i]) == 0){
                if(i+3 != argc || argv[i+1][0] == '-'){
                    is_invalid = 1;
                    break;
                }
                else{
                    is_readFile = 1;
                }
            }
            else {
                printf("my-look: invalid command line\n");
                return 1;
                is_invalid = 1;
                break;
            }
        }
        else{
            if(argc == 2 || i == argc-1) {
                argument = argv[i];
            }
            else if(is_readFile && i == argc-2 && argv[i+1][0] != '-'){
                filename = argv[i];
            }
            else{
                is_invalid = 1;
                break;
            }
        }
    }
    if(is_invalid){
        printf("my-look: invalid command line\n");
        return 1;
    }
     */
    init_buffer();
    if(is_readFile){
        check_file();
    }
    else{
        check_stdin();
    }
    free_buffer();
    return 0;
}
