/* Test with a header include from a custom directory. */
#include <stdio.h>
#include "test_header.h"

int main(void)
{
    printf("hello from t003: value=%d\n", TEST_VALUE);
    return 0;
}
