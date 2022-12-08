#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef __APPLE__
#include <sys/uio.h>
#else

#include <sys/io.h>

#endif

#define MAX_CMD_LINE    600
#define MAX_WRITE_LEN   600
#define PROMPT          "mysh> "
#define err_arg         "Usage: mysh [batch-file]\n"
#define file_err        "Error: Cannot open file "
#define cmd_err         ": Command not found.\n"
#define format_err      "Redirection misformatted.\n"
#define unalias_err     "unalias: Incorrect number of arguments.\n"
#define danger_err      "alias: Too dangerous to alias that.\n"
#define noname_err      "unalias: no such alias name.\n"

#define EXIT            0
#define ALIAS           1
#define UNALIAS         2
#define BUILT_IN        3

char *command_path;
char *commands[MAX_CMD_LINE];
char *cmd_line;
char *built_in[] = {"exit", "alias", "unalias"};
char *redirect_path = NULL;
char full_cmd[MAX_CMD_LINE];

FILE *fp = NULL;
FILE *output_file = NULL;

typedef struct cmd_name {
    char *pre_name;
    char *new_name;
    struct cmd_name *next;
} Name_Node;

typedef struct name_list {
    Name_Node *head;

    int (*init)();

    void (*add)(char *pre, char *new);

    int (*delete)(char *name);

    Name_Node *(*contain)(char *name);
} NameList;

int init();

int delete(char *name);

Name_Node *contain(char *name);

void add(char *pre, char *new);

NameList nameList = {NULL, init, add, delete, contain};

int init() {
    nameList.head = NULL;
    return 0;
}

void add(char *pre, char *new) {
    Name_Node *node = malloc(sizeof(Name_Node));
    node->pre_name = malloc(sizeof(char) * strlen(pre) + 1);
    node->new_name = malloc(sizeof(char) * strlen(new) + 1);
    strcpy(node->pre_name, pre);
    strcpy(node->new_name, new);
    node->next = NULL;
    if (nameList.head == NULL) {
        nameList.head = node;
        return;
    }
    Name_Node *cur_node = nameList.head;
    while (cur_node->next != NULL) {
        cur_node = cur_node->next;
    }
    cur_node->next = node;
    return;
}

int delete(char *name) {
    if (nameList.head == NULL) {
        return -1;
    }
    Name_Node *cur = nameList.head;
    Name_Node *pre = NULL;
    while (cur != NULL) {
        if (strcmp(name, cur->new_name) == 0) {
            if (cur == nameList.head && cur->next != NULL) {
                nameList.head = cur->next;
            } else if (cur == nameList.head && cur->next == NULL) {
                nameList.head = NULL;
            } else {
                pre->next = cur->next;
            }
            free(cur->new_name);
            free(cur->pre_name);
            free(cur);
            return 0;
        }
        pre = cur;
        cur = cur->next;
    }
    return -1;
}

Name_Node *contain(char *name) {
    Name_Node *cur = nameList.head;
    while (cur != NULL) {
        if (strcmp(name, cur->new_name) == 0) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void print_alias() {
    Name_Node *cur = nameList.head;
    while (cur != NULL) {
        write(STDOUT_FILENO, cur->new_name, strlen(cur->new_name));
        write(STDOUT_FILENO, " ", strlen(" "));
        write(STDOUT_FILENO, cur->pre_name, strlen(cur->pre_name));
        write(STDOUT_FILENO, "\n", strlen("\n"));
        cur = cur->next;
    }
}

void free_mem() {
    free(cmd_line);
    if (fp != NULL) {
        fclose(fp);
    }
    while (nameList.head != NULL) {
        nameList.delete(nameList.head->new_name);
    }
}

void init_mem() {
    cmd_line = malloc(MAX_CMD_LINE * sizeof(char));
}


int line2cmd(char *line) {
    char *delim = " \t\n";
    int is_redirect = 0;
//    p = strtok(NULL, "");
//    write(STDOUT_FILENO, p, strlen(p));
//    write(STDOUT_FILENO, "\n", strlen("\n"));
    if (strstr(line, ">")) {
        is_redirect = 1;
    }
    char *cmd_part = strtok(line, ">");
    char *redirect_part = strtok(NULL, "");
    command_path = strtok(cmd_part, delim);
    if (command_path == NULL) {
        return -1;
    }
//    write(STDOUT_FILENO, command_path, strlen(command_path));
    memset(commands, 0, sizeof(char *) * MAX_CMD_LINE);
    char command_path_copy[MAX_CMD_LINE];
    char *exec_cmd;
    strcpy(command_path_copy, command_path);
    exec_cmd = command_path_copy;
//    for (int i = 0; i < strlen(command_path_copy); i++) {
//        if (command_path_copy[i] == '/') {
//            exec_cmd = &command_path_copy[i] + 1;
//        }
//    }
    commands[0] = exec_cmd;
    char *p;
    int last = 1;

    for (int i = 1; (p = strtok(NULL, delim)); i++) {
        commands[i] = p;
        last++;
    }
    if (is_redirect) {
        redirect_path = strtok(redirect_part, delim);
        if (redirect_path == NULL || strstr(redirect_path, ">") || strtok(NULL, delim) != NULL) {
            write(STDERR_FILENO, format_err, strlen(format_err));
            return -1;
        }
    }
    commands[last] = NULL;
    return 0;
}


//void batch_mode(char* filename) {
//    FILE *fp = fopen(filename, "r");
//    if (fp == NULL) {
////        printf("Error: Cannot open file %s.\n", filename);
//        char file_err_msg[512];
//        strcat(file_err_msg, file_err);
//        strcat(file_err_msg, filename);
//        strcat(file_err_msg, "\n");
//        write(STDERR_FILENO, file_err_msg, strlen(file_err_msg));
//        exit(1);
//    }
//
//    fclose(fp);
//}
void alias(char *alias_arg) {
    char *alias_name;
    char *replace_cmd;
    char *delim = " \t\n";
    Name_Node *node;
    if (alias_arg == NULL) {
        print_alias();
        return;
    }
    alias_name = strtok(alias_arg, delim);
    if ((replace_cmd = strtok(NULL, "\n")) == NULL) {
        if ((node = nameList.contain(alias_name))) {
            write(STDOUT_FILENO, node->new_name, strlen(node->new_name));
            write(STDOUT_FILENO, " ", strlen(" "));
            write(STDOUT_FILENO, node->pre_name, strlen(node->pre_name));
            write(STDOUT_FILENO, "\n", strlen("\n"));
        }
        return;
    } else {
        for (int i = 0; i < BUILT_IN; i++) {
            if (strcmp(alias_name, built_in[i]) == 0) {
                write(STDERR_FILENO, danger_err, strlen(danger_err));
                write(STDERR_FILENO, "\n", strlen("\n"));
                return;
            }
        }
        if (nameList.contain(alias_name)) {
            nameList.delete(alias_name);
        }
        nameList.add(replace_cmd, alias_name);
    }
    return;
}

int unalias(char *alias_name) {
    char *delim = " \t\n";
    char *name;
    if (alias_name == NULL || ((name = strtok(alias_name, delim)) && (strtok(NULL, delim)))) {
        write(STDOUT_FILENO, unalias_err, strlen(unalias_err));
        return -1;
    }
    if (nameList.delete(name) < 0) {
        write(STDOUT_FILENO, noname_err, strlen(noname_err));
        return -1;
    }
    return 0;
}

int exec_cmd(char *cmd) {
    char *delim = " \t\n";
    char *names[MAX_CMD_LINE];
    char after_cmd[MAX_CMD_LINE];
    memset(after_cmd, 0, sizeof(char) * MAX_CMD_LINE);
    strcpy(after_cmd, cmd);
    if (fp != NULL) {
        if (after_cmd[strlen(after_cmd) - 1] == '\n') {
            after_cmd[strlen(after_cmd) - 1] = '\0';
        }
        write(STDOUT_FILENO, after_cmd, strlen(after_cmd));
        write(STDOUT_FILENO, "\n", strlen("\n"));
    }
    if ((names[0] = strtok(cmd, delim)) == NULL) {
        return 0;
    }
    Name_Node *node;
    if ((node = nameList.contain(names[0]))) {
        names[0] = node->pre_name;
    }
    for (int i = 1; (names[i] = strtok(NULL, delim)); i++) {
        if ((node = nameList.contain(names[i]))) {
            names[i] = node->pre_name;
        }
    }
    memset(full_cmd, 0, sizeof(char) * MAX_CMD_LINE);
    for (int i = 0; i < MAX_CMD_LINE && names[i] != NULL; i++) {
//        write(STDOUT_FILENO, names[i], strlen(names[i]));
//        write(STDOUT_FILENO, "\n", strlen("\n"));
        strcat(full_cmd, names[i]);
        strcat(full_cmd, " ");
    }
    int built_in_cmd = -1;
    for (int i = 0; i < BUILT_IN; i++) {
        if (strcmp(built_in[i], names[0]) == 0) {
            built_in_cmd = i;
        }
    }
    char *alias_arg;
    switch (built_in_cmd) {
        case EXIT:
            exit(0);

        case ALIAS:
            strtok(after_cmd, delim);
            alias_arg = strtok(NULL, "\n");
            alias(alias_arg);
            return 0;

        case UNALIAS:
            strtok(after_cmd, delim);
            alias_arg = strtok(NULL, "\n");
            unalias(alias_arg);
            return 0;

    }
    if (line2cmd(full_cmd) < 0) {
        return 0;
    }
    int pid;
    if ((pid = fork()) == 0) {
        if (redirect_path != NULL) {
            output_file = freopen(redirect_path, "w", stdout);
            if (output_file == NULL) {
                write(STDOUT_FILENO, "Cannot write to file ", strlen("Cannot write to file "));
                write(STDOUT_FILENO, redirect_path, strlen(redirect_path));
                write(STDOUT_FILENO, "\n", strlen("\n"));
                exit(1);
            }
        }

        if (execv(command_path, (char *const *) commands) < 0) {
            write(STDERR_FILENO, cmd, strlen(cmd));
            write(STDERR_FILENO, cmd_err, strlen(cmd_err));
            exit(1);
        }
        fclose(output_file);
    }
    wait(NULL);
    if (redirect_path != NULL) {
        redirect_path = NULL;
    }
    return 0;
}

void shell_mode(FILE *file) {
    nameList.init();
    if (file == stdin)
        write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
    while (fgets(cmd_line, MAX_CMD_LINE, file) != NULL) {
        exec_cmd(cmd_line);
//        line2cmd(cmd_line);
        if (file == stdin)
            write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
    }
}

int main(int argc, char *argv[]) {
    if (argc > 2) {
//        printf("Usage: mysh [batch-file]\n");
        write(STDERR_FILENO, err_arg, strlen(err_arg));
        exit(1);
    }
    init_mem();
    //batch mode
    if (argc == 2) {
        char *filename;
        filename = argv[1];
        fp = fopen(filename, "r");
        if (fp == NULL) {
//        printf("Error: Cannot open file %s.\n", filename);
            char file_err_msg[MAX_WRITE_LEN];
            strcat(file_err_msg, file_err);
            strcat(file_err_msg, filename);
            strcat(file_err_msg, ".\n");
            write(STDERR_FILENO, file_err_msg, strlen(file_err_msg));
            exit(1);
        }
        shell_mode(fp);
        fclose(fp);
        return 0;
    }
        //interactive mode
    else {
        shell_mode(stdin);
    }
    free_mem();
//    nameList.init();
//    nameList.add("A", "1");
//    nameList.delete("1");
//    nameList.add("B", "2");
//    nameList.add("C", "3");
//    nameList.add("D", "4");
//    print_alias();
//    printf("%s\n", node1->pre_name);
    return 0;
}

