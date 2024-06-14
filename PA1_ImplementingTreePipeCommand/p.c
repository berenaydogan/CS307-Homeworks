#include <stdio.h>
#include <stdlib.h>

#define OPERATION 0

typedef int (*operationFunc)(int, int);

int addSubtract(int num1, int num2) {
    return (num1 + num2) - 5;
}

int multiply(int num1, int num2) {
    return num1 * num2;
}

int add(int num1, int num2) {
    return num1 + num2;
}

int subtract(int num1, int num2) {
    return num2 - num1;
}

int minimum(int num1, int num2) {
    return (num1 < num2) ? num1 : num2;
}

int maximum(int num1, int num2) {
    return (num1 > num2) ? num1 : num2;
}

int bitwiseAND(int num1, int num2) {
    return num1 & num2;
}

int divideByTwo(int num1, int num2) {
    return num1 / 2;
}

int main(int argc, char *argv[]) {
    int num1, num2;
     if (argc != 1) {
        printf("Usage: %s \n", argv[0]);
        return 1; // Error code for incorrect usage
    }
    scanf("%d", &num1);
    scanf("%d", &num2);


    operationFunc operations[] = {
        add,          // 0
        multiply,     // 1
        subtract,     // 2
        addSubtract,  // 3
        minimum,      // 4
        maximum,      // 5
        bitwiseAND,   // 6
        divideByTwo   // 7
        };

    if (OPERATION < 0 || OPERATION >= sizeof(operations) / sizeof(operations[0])) {
        printf("Invalid OPERATION index.\n");
        return 1;
    }

    int result = operations[OPERATION](num1, num2);
    printf("%d\n", result);

    return 0;
}
