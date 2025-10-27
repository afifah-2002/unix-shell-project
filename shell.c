#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

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
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

/* Trim leading and trailing whitespace */
char* trim_whitespace(char *str) {
    char *end;
    
    /* Trim leading space */
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0)  /* All spaces? */
        return str;
    
    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    /* Write new null terminator */
    end[1] = '\0';
    
    return str;
}

int has_output_redirection(char **args, char **output_file) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] != NULL) {
                *output_file = args[i + 1];
                args[i] = NULL;
                return 1;
            } else {
                fprintf(stderr, "Error: No output file specified after '>'\n");
                return -1;  /* Error: malformed command */
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
            } else {
                fprintf(stderr, "Error: No input file specified after '<'\n");
                return -1;  /* Error: malformed command */
            }
        }
    }
    return 0;
}

int has_pipe(char **args, char ***cmd1, char ***cmd2) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: No command specified after '|'\n");
                return -1;  /* Error: malformed command */
            }
            args[i] = NULL;
            *cmd1 = args;
            *cmd2 = &args[i + 1];
            
            /* Check if cmd1 is empty */
            if (*cmd1 == NULL || **cmd1 == NULL) {
                fprintf(stderr, "Error: No command specified before '|'\n");
                return -1;
            }
            
            return 1;
        }
    }
    return 0;
}

/* Check if command is a built-in (for history tracking purposes) */
int is_builtin_for_history(char *cmd) {
    return (strcmp(cmd, "history") == 0 || strcmp(cmd, "exit") == 0);
}

/* Execute built-in commands - returns 1 if it was a builtin, 0 otherwise */
int execute_builtin(char **args) {
    if (strcmp(args[0], "history") == 0) {
        display_history();
        return 1;
    }
    return 0;
}

void execute_command(char **args) {
    char *output_file = NULL;
    char *input_file = NULL;
    int redirect_output = has_output_redirection(args, &output_file);
    int redirect_input = has_input_redirection(args, &input_file);
    
    /* Check for malformed redirection */
    if (redirect_output == -1 || redirect_input == -1) {
        exit(1);
    }
    
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
    
    /* Check if it's a built-in command */
    if (execute_builtin(args)) {
        exit(0);
    }
    
    /* Not a built-in, execute as external command */
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
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        /* Check if first command is built-in */
        if (execute_builtin(cmd1)) {
            exit(0);
        }
        
        /* Not built-in, execute as external command */
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
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        /* NEW: Check for output redirection in cmd2 */
        char *output_file = NULL;
        int redirect_output = has_output_redirection(cmd2, &output_file);
        
        if (redirect_output == 1) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        /* Execute second command */
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

/* BONUS FEATURE: Execute command from history */
int execute_history_command(char *input) {
    /* Check for !! (repeat last command) */
    if (strcmp(input, "!!") == 0) {
        if (history_count == 0) {
            fprintf(stderr, "Error: No commands in history\n");
            return -1;
        }
        printf("%s\n", history[history_count - 1]);
        strcpy(input, history[history_count - 1]);
        return 1;
    }
    
    /* Check for !n (repeat command number n) */
    if (input[0] == '!' && isdigit(input[1])) {
        int num = atoi(&input[1]);
        if (num < 1 || num > history_count) {
            fprintf(stderr, "Error: No such command in history\n");
            return -1;
        }
        printf("%s\n", history[num - 1]);
        strcpy(input, history[num - 1]);
        return 1;
    }
    
    return 0;  /* Not a history command */
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
        
        /* Trim whitespace */
        char *trimmed = trim_whitespace(input);
        if (strlen(trimmed) == 0) {
            continue;
        }
        strcpy(input, trimmed);
        
        /* Save original command */
        strcpy(input_copy, input);
        
        /* Check for exit command before parsing */
        if (strcmp(input, "exit") == 0) {
            should_run = 0;
            continue;
        }
        
        /* BONUS: Check for history recall (!!, !n) */
        int history_recall = execute_history_command(input);
        if (history_recall == -1) {
            continue;  /* Error in history recall */
        }
        if (history_recall == 1) {
            /* Command was replaced with history command, update copy */
            strcpy(input_copy, input);
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
        
        /* Check for pipe */
        char **cmd1, **cmd2;
        int is_pipe = has_pipe(args, &cmd1, &cmd2);
        
        /* Check for malformed pipe */
        if (is_pipe == -1) {
            if (batch_mode) {
                fprintf(stderr, "Batch mode: Skipping malformed command\n");
            }
            continue;
        }
        
        /* Add to history ONLY if it's not a built-in command */
        if (!is_builtin_for_history(args[0])) {
            add_to_history(input_copy);
        }
        
        if (is_pipe) {
            /* Execute piped commands */
            execute_pipe(cmd1, cmd2);
        } else {
            /* Check if it's history command without pipe */
            if (strcmp(args[0], "history") == 0) {
                display_history();
                continue;
            }
            
            /* No pipe - regular execution */
            pid_t pid = fork();
            
            if (pid < 0) {
                fprintf(stderr, "Fork failed\n");
                if (batch_mode) {
                    fprintf(stderr, "Batch mode: Continuing with next command\n");
                }
                return 1;
            } else if (pid == 0) {
                execute_command(args);
            } else {
                int status;
                wait(&status);
                if (batch_mode && WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                    fprintf(stderr, "Batch mode: Command failed with exit code %d\n", WEXITSTATUS(status));
                }
            }
        }
    }
    
    if (batch_mode) {
        fclose(input_file);
    }
    
    return 0;
}