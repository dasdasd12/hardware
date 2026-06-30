#include "CH58x_common.h"
#include "ch585_local_periph_test.h"

int main(void)
{
#if (defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    ch585_local_periph_test_run();

    while (1)
    {
    }
}
