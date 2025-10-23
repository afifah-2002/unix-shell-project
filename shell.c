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

int has_input_redirection(char **args, char **input_file) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] != NULL) {
                *input_file = args[i + 1];
                args[i] = NULL;
                return 1;
            }
        }
    }
    return 0;
}

/* Function to check for pipe */
int has_pipe(char **args, char ***cmd1, char ***cmd2) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            args[i] = NULL;  /* Split the command at pipe */
            *cmd1 = args;
            *cmd2 = &args[i + 1];
            return 1;
        }
    }
    return 0;
}

void execute_command(char **args) {
    char *output_file = NULL;
    char *input_file = NULL;
    int redirect_output = has_output_redirection(args, &output_file);
    int redirect_input = has_input_redirection(args, &input_file);
    
    /* Handle input redirection */
    if (redirect_input) {
        int fd = open(input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Error: Cannot open input file '%s'\n", input_file);
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
}

void execute_pipe(char **cmd1, char **cmd2) {
    int pipefd[2];
    pid_t pid1, pid2;
    
    if (pipe(pipefd) == -1) {
        fprintf(stderr, "Pipe failed\n");
        return;
    }
    
    /* First command */
    pid1 = fork();
    if (pid1 < 0) {
        fprintf(stderr, "Fork failed\n");
        return;
    }
    
    if (pid1 == 0) {
        /* Child 1: writes to pipe */
        close(pipefd[0]);  /* Close read end */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        if (execvp(cmd1[0], cmd1) == -1) {
            fprintf(stderr, "Command not found: %s\n", cmd1[0]);
        }
        exit(1);
    }
    
    /* Second command */
    pid2 = fork();
    if (pid2 < 0) {
        fprintf(stderr, "Fork failed\n");
        return;
    }
    
    if (pid2 == 0) {
        /* Child 2: reads from pipe */
        close(pipefd[1]);  /* Close write end */
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        if (execvp(cmd2[0], cmd2) == -1) {
            fprintf(stderr, "Command not found: %s\n", cmd2[0]);
        }
        exit(1);
    }
    
    /* Parent closes both ends and waits */
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
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
        
        /* Check for exit command early */
        if (strcmp(input, "exit") == 0) {
            should_run = 0;
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
        
        /* Check for history command AFTER parsing */
        if (strcmp(args[0], "history") == 0) {
            /* Check if history is part of a pipe */
            int is_piped = 0;
            for (int j = 0; args[j] != NULL; j++) {
                if (strcmp(args[j], "|") == 0) {
                    is_piped = 1;
                    break;
                }
            }
            
            if (is_piped) {
                fprintf(stderr, "Error: 'history' is a built-in command and cannot be used in pipes\n");
                continue;
            }
            
            display_history();
            continue;
        }
        
        /* Add to history (after checking it's not history command itself) */
        add_to_history(input_copy);
        
        /* Check for pipe */
        char **cmd1, **cmd2;
        if (has_pipe(args, &cmd1, &cmd2)) {
            execute_pipe(cmd1, cmd2);
        } else {
            /* No pipe - regular execution */
            pid_t pid = fork();
            
            if (pid < 0) {
                fprintf(stderr, "Fork failed\n");
                return 1;
            } else if (pid == 0) {
                execute_command(args);
            } else {
                wait(NULL);
            }
        }
    }
    
    if (batch_mode) {
        fclose(input_file);
    }
    
    return 0;
}