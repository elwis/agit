#define _XOPEN_SOURCE 700
#include <stdlib.h>
int main(void) {
    char buf[1024];
    char *r = realpath(".", buf);
    return r ? 0 : 1;
}

