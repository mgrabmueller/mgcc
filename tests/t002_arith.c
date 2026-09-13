/* Test with arithmetic and a return code. */
#include <stdio.h>

static int sum(int a, int b)
{
    return a + b;
}

int main(void)
{
    int r = sum(2, 3);
    printf("hello from t002: sum=%d\n", r);
    return r == 5 ? 0 : 1;
}
