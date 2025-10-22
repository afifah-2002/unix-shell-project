#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 80
#define HISTORY_SIZE 10

/* History storage */
char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;

/* Function to add command to history */
void add_to_history(char *cmd) {
    if (history_count < HISTORY_SIZE) {
        strcpy(history[history_count], cmd);
        history_count++;
    } else {
        /* Shift history and add new command */
        for (int i = 0; i < HISTORY_SIZE - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }
        strcpy(history[HISTORY_SIZE - 1], cmd);
    }
}

/* Function to display history */
void display_history() {
    printf("\n");
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    char *args[MAX_LINE/2 + 1];
    int should_run = 1;
    char input[MAX_LINE];
    FILE *input_file = stdin;
    int batch_mode = 0;
    
    /* Check for batch mode */
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
        
        /* Add to history (except history command itself) */
        if (strcmp(input, "history") != 0) {
            add_to_history(input);
        }
        
        /* Check for exit */
        if (strcmp(input, "exit") == 0) {
            should_run = 0;
            continue;
        }
        
        /* Check for history command */
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
        
        /* Fork and execute */
        pid_t pid = fork();
        
        if (pid < 0) {
            fprintf(stderr, "Fork failed\n");
            return 1;
        } else if (pid == 0) {
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