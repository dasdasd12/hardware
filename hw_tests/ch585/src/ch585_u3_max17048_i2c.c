#include "ch585_common.h"
#include "ch585_soft_i2c.h"

#define MAX17048_ADDR 0x36u

void ch585_u3_max17048_i2c_run(void)
{
    uint16_t vcell;
    uint16_t soc;
    uint16_t version;
    uint16_t config;

    GPIOB_ModeCfg(GPIO_Pin_5, GPIO_ModeIN_PU);
    ch585_soft_i2c_init();

    if(!ch585_soft_i2c_probe(MAX17048_ADDR))
    {
        ch585_log_fail("max17048", "no_ack");
        return;
    }

    if(!ch585_soft_i2c_read_u16_be(MAX17048_ADDR, 0x02u, &vcell))
    {
        ch585_log_fail("max17048", "vcell");
        return;
    }
    if(!ch585_soft_i2c_read_u16_be(MAX17048_ADDR, 0x04u, &soc))
    {
        ch585_log_fail("max17048", "soc");
        return;
    }
    if(!ch585_soft_i2c_read_u16_be(MAX17048_ADDR, 0x08u, &version))
    {
        ch585_log_fail("max17048", "version");
        return;
    }
    if(!ch585_soft_i2c_read_u16_be(MAX17048_ADDR, 0x0Cu, &config))
    {
        ch585_log_fail("max17048", "config");
        return;
    }

    ch585_log_kv_hex("vcell_raw", vcell, 4);
    ch585_log_kv_hex("soc_raw", soc, 4);
    ch585_log_kv_hex("version", version, 4);
    ch585_log_kv_hex("config", config, 4);
    ch585_log_kv_hex("alert_pin", GPIOB_ReadPortPin(GPIO_Pin_5) ? 1u : 0u, 1);

    if(version == 0x0000u || version == 0xFFFFu)
    {
        ch585_log_fail("max17048", "bad_version");
        return;
    }

    ch585_log_pass("ch585_u3_max17048_i2c");
}
