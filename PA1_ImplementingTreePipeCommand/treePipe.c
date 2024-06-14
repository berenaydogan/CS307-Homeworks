#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>

// Function that prints dashes according to the given parameter to match the output
void printDashes(int times) {
    for (int i = 0; i < 3 * times; i++) {
        fprintf(stderr, "%s", "-");
    }
}

// Function that transforms the process to treePipe program by executing the specified command using execvp()
void transformNode(int curDepth, int maxDepth, int lr) {
    char curDepthStr[11];
    char maxDepthStr[11];
    char lrStr[11];

    // Convert integers to strings
    sprintf(curDepthStr, "%d", curDepth + 1);
    sprintf(maxDepthStr, "%d", maxDepth);
    sprintf(lrStr, "%d", lr);

    // Declare an array holding to be passed to the treePipe program and execvp()
    char *args[] = {"treePipe", curDepthStr, maxDepthStr, lrStr, NULL};
    execvp("./treePipe", args);
}

// Function that forks a worker process to compute results using another program
// It takes current depth and whether it is a left/right node as parameters
void createWorker(int curDepth, int lr) {

    // Declare resStr char array to hold the result
    char resStr[11] = {'\0'};

    // Declare worker_rc to hold the return of fork()
    int worker_rc = fork();
    // If worker_rc < 0, print a message informing the user that fork failed
    if (worker_rc < 0) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    
    // If worker_rc == 0 (it is the worker (child) process)
    else if (worker_rc == 0) {
        // If left node, execute ./left program
        if (lr == 0) {
            char *argsLr[] = {"./left", NULL};
            execvp("./left", argsLr);

        // Else execute ./right program
        } else {
            char *argsLr[] = {"./right", NULL};
            execvp("./right", argsLr);
        }
    }
    
    // If it is the parent process
    else {

        // Wait for the child process to finish executing
        wait(NULL);

        // Read the result printed by worker process
        scanf("%s", resStr);
        
        // Print result using stderr to the console
        printDashes(curDepth);
        fprintf(stderr, "> my result is: %s\n", resStr); 

        // Write the result to the pipe to be read by parent
        printf("%s\n", resStr);
        fflush(stdout);
    }
}


int main(int argc, char *argv[]) {

    // If given number of arguments are not provided, print the usage of the command
    if (argc != 4) {
        printf("Usage: treePipe <current depth> <max depth> <left-right>");
        return 1;
    }

    // Obtain the given command line arguments from argv array and convert them to integer
    int curDepth = atoi(argv[1]);
    int maxDepth = atoi(argv[2]);
    int lr = atoi(argv[3]);

    // Declare flags to keep the return of system calls
    int left_rc = 0;
    int right_rc = 0;
    int pipeReturn = 0;
    int dup2Return = 0;

    // Declare char arrays to read the values from pipe to
    char num1Str[11] = {'\0'}; 
    char num2Str[11] = {'\0'}; 
    char resStr[11] = {'\0'};

    // Declare an array of two integers to hold file descriptors returned by the pipe() system call
    int fd[2];

    // If the process is at root node, do some operations as initial setup tasks
    if (curDepth == 0) {

        // Print current depth and whether it is a left/right node using stderr to the console
        fprintf(stderr, "> current depth: %d, lr: %d\n", curDepth, lr);

        // Take num1 from the user
        fprintf(stderr, "Please enter num1 for the root: ");
        scanf("%s", num1Str);

        // Print the num1 value of the root
        fprintf(stderr, "> my num1 is: %s\n", num1Str);

        // Create pipe and store its return value
        pipeReturn = pipe(fd);

        // If pipeReturn == -1, print a message informing the user that pipe failed
        if (pipeReturn == -1) {
            fprintf(stderr, "pipe failed\n");
            exit(1);
        }

        // Copy pipe to stdin of the process, store return of dup2 in a variable
        dup2Return = dup2(fd[0], STDIN_FILENO);

        // If dup2Return == -1, print a message informing the user that dup2 failed
        if (dup2Return == -1) {
        fprintf(stderr, "dup2 failed\n");
        exit(1);
        }
        
        // Copy pipe to stdout of the process, store return of dup2 in a variable
        dup2Return = dup2(fd[1], STDOUT_FILENO);

        // If dup2Return == -1, print a message informing the user that dup2 failed
        if (dup2Return == -1) {
        fprintf(stderr, "dup2 failed\n");
        exit(1);
        }

        // Close the original file descriptors
        close(fd[0]); 
        close(fd[1]); 

        // Print num1 value to pipe to be read by child process
        printf("%s\n", num1Str);
        fflush(stdout);

    }

    // Read num1 from pipe
    scanf("%s", num1Str);

    // If the process isn't a leaf node, fork a left process from it
    if (curDepth != maxDepth) {
        left_rc = fork();
    }

    // If left_rc < 0, print a message informing the user that fork failed
    if (left_rc < 0) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    
    // If left_rc == 0 (it is the child process)
    else if (left_rc == 0) {

        // If not the root node
        if (curDepth != 0) {
            // Print current depth and num1 using stderr to the console
            printDashes(curDepth);
            fprintf(stderr, "> current depth: %d, lr: %d\n", curDepth, lr);
            printDashes(curDepth);
            fprintf(stderr, "> my num1 is: %s\n", num1Str);
        }

        // If the child process isn't a leaf node, execute the program treePipe recursively as a left node on curDepth + 1
        if (curDepth < maxDepth) {
            
            // Print the current value of num1 to the pipe to be used by the new process to be executed
            printf("%s\n", num1Str);
            fflush(stdout);

            // Transform the process to treePipe program, the left node of the forked parent, by executing the specified command using execvp()
            transformNode(curDepth, maxDepth, 0);
        }

        // Format the initial num2 value into a string for leaf nodes
        sprintf(num2Str, "%d", 1);

        // Print num1 and num2 values to pipe to be read by worker process
        printf("%s\n", num1Str);
        printf("%s\n", num2Str);
        fflush(stdout);

        // Create a worker process to run left process and compute the result
        createWorker(curDepth, lr);

    }
    
    // If it is the parent process
    else {

        // Wait for the child process to finish executing
        wait(NULL);

        // Scan num2 received from child process through the pipe
        scanf("%s", num2Str);

        // Print current depth, num1 and num2 using stderr to the console
        printDashes(curDepth);
        fprintf(stderr, "> current depth: %d, lr: %d, my num1: %s, my num2: %s\n", curDepth, lr, num1Str, num2Str);

        // Print the current value of num1 and num2 to the pipe to be read by worker process
        printf("%s\n", num1Str);
        printf("%s\n", num2Str);
        fflush(stdout);

        // Create a worker process to run the process of the parent and compute the result
        createWorker(curDepth, lr);

        // Declare right_rc to hold the return of fork()
        right_rc = fork();
        
        // If right_rc < 0, print a message informing the user that fork failed
        if (right_rc < 0) {
            fprintf(stderr, "fork failed\n");
            exit(1);
        }

        // If right_rc == 0 (it is the child process)
        else if (right_rc == 0) {
            // Transform the process to treePipe program, the right node of the forked parent, by executing the specified command using execvp()
            transformNode(curDepth, maxDepth, 1);
        }
        
        else {
            // Wait for the child process to finish executing
            wait(NULL);

            // Read the result printed by worker process
            scanf("%s", resStr);

            // Write the result to the pipe to be read by parent
            printf("%s\n", resStr);
            fflush(stdout);
        }

    }

    // If the root process is finished, read and print final result of it
    if (curDepth == 0) {
        scanf("%s", resStr);
        fprintf(stderr, "The final result is: %s", resStr);
    }

    return 0;
}
