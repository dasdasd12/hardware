#include "ch585_common.h"
#include "ch585_soft_i2c.h"

#ifndef ALLOW_EEPROM_WRITE
#define ALLOW_EEPROM_WRITE 0
#endif

void ch585_u2_eeprom_i2c_run(void)
{
    uint8_t addr;
    uint8_t found = 0;
    uint8_t value = 0;

    ch585_soft_i2c_init();

    for(addr = 0x50u; addr <= 0x57u; ++addr)
    {
        if(ch585_soft_i2c_probe(addr))
        {
            found = addr;
            break;
        }
    }

    if(found == 0u)
    {
        ch585_log_fail("eeprom", "no_ack");
        return;
    }

    ch585_log_kv_hex("addr7", found, 2);
    if(!ch585_soft_i2c_read_u8(found, 0x00u, &value))
    {
        ch585_log_fail("eeprom", "read0");
        return;
    }
    ch585_log_kv_hex("byte0", value, 2);

#if ALLOW_EEPROM_WRITE
    {
        uint8_t original = 0;
        uint8_t verify = 0;
        uint8_t test_value;

        if(!ch585_soft_i2c_read_u8(found, 0xFFu, &original))
        {
            ch585_log_fail("eeprom", "read_restore_slot");
            return;
        }

        test_value = (uint8_t)(original ^ 0x5Au);
        if(!ch585_soft_i2c_write_u8(found, 0xFFu, test_value))
        {
            ch585_log_fail("eeprom", "write_test");
            return;
        }
        ch585_delay_ms(8);

        if(!ch585_soft_i2c_read_u8(found, 0xFFu, &verify) || verify != test_value)
        {
            ch585_log_fail("eeprom", "verify_test");
            return;
        }

        if(!ch585_soft_i2c_write_u8(found, 0xFFu, original))
        {
            ch585_log_fail("eeprom", "restore");
            return;
        }
        ch585_delay_ms(8);
    }
#else
    ch585_log_skip("eeprom_write", "disabled");
#endif

    ch585_log_pass("ch585_u2_eeprom_i2c");
}
