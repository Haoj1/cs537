#include <stdio.h>

int fec_and_inc(int *var) {
    int old = *var;
    __asm__ volatile
    ("lock; addl $1, %0"
	:"+m" (*var) // input + output
	: // No input
	: "memory"
    );
    return old;
}

int main(int argc, char* args[]) {
    int val = 1;
    int rc = fec_and_inc(&val);
    printf("val = %d, rc = %d\n", val, rc);
    return 0;
}