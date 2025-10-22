#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 80  /* Maximum command length */

int main(void) {
    char *args[MAX_LINE/2 + 1];  /* Command line arguments */
    int should_run = 1;
    char input[MAX_LINE];
    
    while (should_run) {
        printf("osh> ");
        fflush(stdout);
        
        /* Read user input */
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            break;
        }
        
        /* Remove newline character */
        input[strcspn(input, "\n")] = 0;
        
        /* Check for exit command */
        if (strcmp(input, "exit") == 0) {
            should_run = 0;
            continue;
        }
        
        /* Parse command - simplified version */
        char *token = strtok(input, " ");
        int i = 0;
        while (token != NULL && i < MAX_LINE/2) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;  /* Null-terminate the argument list */
        
        /* If empty command, continue */
        if (args[0] == NULL) {
            continue;
        }
        
        /* Fork a child process */
        pid_t pid = fork();
        
        if (pid < 0) {
            fprintf(stderr, "Fork failed\n");
            return 1;
        } else if (pid == 0) {
            /* Child process */
            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "Command not found: %s\n", args[0]);
            }
            exit(1);
        } else {
            /* Parent process */
            wait(NULL);  /* Wait for child to complete */
        }
    }
    
    return 0;
}
