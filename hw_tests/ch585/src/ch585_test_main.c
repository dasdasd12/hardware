#include "ch585_common.h"

#ifndef CH585_TEST_FUNC
#define CH585_TEST_FUNC ch585_u2_eeprom_i2c_run
#endif

int main(void)
{
    ch585_board_init();
    ch585_log_start();
    CH585_TEST_FUNC();

    while(1)
    {
        ch585_delay_ms(1000);
    }
}
