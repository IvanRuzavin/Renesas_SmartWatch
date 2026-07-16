/**
 * @file main.c
 * @brief Application entry point for the R7FA4M2 smartwatch example.
 */

#ifdef PREINIT_SUPPORTED
#include "preinit.h"
#endif

#include "app_controller.h"

/**
 * @brief Initialize the smartwatch application and enter its foreground loop.
 * @return Zero if initialization returns; normally not reached.
 */
int main(void)
{
    #ifdef PREINIT_SUPPORTED
    preinit();
    #endif

    (void)app_controller_init();
    app_controller_run();
    return 0;
}
