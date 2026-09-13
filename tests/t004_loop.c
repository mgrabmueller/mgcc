/* Test with a loop and a computed return code. */
#include <stdio.h>

int main(void)
{
    int total = 0;
    for (int i = 1; i <= 10; i++)
        total += i;
    printf("hello from t004: total=%d\n", total);
    return total == 55 ? 0 : 1;
}
