#include "h417_common.h"

#ifndef HW_TEST_FUNC
#define HW_TEST_FUNC h417_gpio_status_run
#endif

#ifndef HW_TEST_ID
#define HW_TEST_ID 1
#endif

int main(void)
{
    h417_board_clock_gpio_init();
    h417_status_begin(HW_TEST_ID);
    HW_TEST_FUNC();

    while(1)
    {
        g_h417_status.cycle++;
        h417_delay_cycles(200000u);
    }

    return 0;
}
