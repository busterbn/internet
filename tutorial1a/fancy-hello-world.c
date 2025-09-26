#include <stdio.h>
#include <string.h>
#include "hello-world.h"

int main(void) {
    char name[100];
    if (fgets(name, sizeof(name), stdin)) {
        name[strcspn(name, "\n")] = 0;
        printf("Hello World, hello %s\n", name);
    }
    return 0;
}