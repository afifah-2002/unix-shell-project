#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80
#define HISTORY_SIZE 10

/* History storage */
char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;

void add_to_history(char *cmd) {
    if (history_count < HISTORY_SIZE) {
        strcpy(history[history_count], cmd);
        history_count++;
    } else {
        for (int i = 0; i < HISTORY_SIZE - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }
        strcpy(history[HISTORY_SIZE - 1], cmd);
    }
}

void display_history() {
    printf("\n");
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
    printf("\n");
}

int has_output_redirection(char **args, char **output_file) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] != NULL) {
                *output_file = args[i + 1];
                args[i] = NULL;
                return 1;
            }
        }
    }
    return 0;
}

/* Function to check for input redirection */
int has_input_redirection(char **args, char **input_file) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] != NULL) {
                *input_file = args[i + 1];
                args[i] = NULL;  /* Remove < and filename from args */
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char *args[MAX_LINE/2 + 1];
    int should_run = 1;
    char input[MAX_LINE];
    char input_copy[MAX_LINE];
    FILE *input_file = stdin;
    int batch_mode = 0;
    
    if (argc == 2) {
        batch_mode = 1;
        input_file = fopen(argv[1], "r");
        if (input_file == NULL) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", argv[1]);
            return 1;
        }
    } else if (argc > 2) {
        fprintf(stderr, "Usage: %s [batch_file]\n", argv[0]);
        return 1;
    }
    
    while (should_run) {
        if (!batch_mode) {
            printf("osh> ");
            fflush(stdout);
        }
        
        if (fgets(input, MAX_LINE, input_file) == NULL) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;
        }
        
        strcpy(input_copy, input);
        
        if (strcmp(input, "history") != 0) {
            add_to_history(input_copy);
        }
        
        if (strcmp(input, "exit") == 0) {
            should_run = 0;
            continue;
        }
        
        if (strcmp(input, "history") == 0) {
            display_history();
            continue;
        }
        
        /* Parse command */
        char *token = strtok(input, " ");
        int i = 0;
        while (token != NULL && i < MAX_LINE/2) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;
        
        if (args[0] == NULL) {
            continue;
        }
        
        /* Check for redirections */
        char *output_file = NULL;
        char *input_redir_file = NULL;
        int redirect_output = has_output_redirection(args, &output_file);
        int redirect_input = has_input_redirection(args, &input_redir_file);
        
        /* Fork and execute */
        pid_t pid = fork();
        
        if (pid < 0) {
            fprintf(stderr, "Fork failed\n");
            return 1;
        } else if (pid == 0) {
            /* Child process */
            
            /* Handle input redirection */
            if (redirect_input) {
                int fd = open(input_redir_file, O_RDONLY);
                if (fd < 0) {
                    fprintf(stderr, "Error: Cannot open input file '%s'\n", input_redir_file);
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            
            /* Handle output redirection */
            if (redirect_output) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            
            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "Command not found: %s\n", args[0]);
            }
            exit(1);
        } else {
            wait(NULL);
        }
    }
    
    if (batch_mode) {
        fclose(input_file);
    }
    
    return 0;
}