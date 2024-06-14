#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int num1, num2;
     if (argc != 1) {
        printf("Usage: %s \n", argv[0]);
        return 1; // Error code for incorrect usage
    }
    scanf("%d", &num1);
    scanf("%d", &num2);
    // Calculate the addition
    int result = num1 + num2;
    //printf("Inputs: %d %d \n", num1, num2);
    // Print the result
    printf("%d\n", result);

    return 0; // Successful execution
}