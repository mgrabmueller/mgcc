/* Test that writes to stderr and exits with a non-zero status. */
#include <stdio.h>

int main(void)
{
    fprintf(stderr, "error from t006\n");
    return 7;
}
