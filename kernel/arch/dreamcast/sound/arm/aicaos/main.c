#include <stddef.h>

extern int main(int argc, char **argv);

/* Initialize the OS */
void arm_main(void)
{
    main(0, NULL);
}
