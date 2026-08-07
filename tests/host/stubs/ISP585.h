/* Host-test stub for the CH585 Data-Flash (EEPROM) API: backs the
 * EEPROM macros with a RAM array owned by the test binary. */
#ifndef HOST_STUB_ISP585_H
#define HOST_STUB_ISP585_H

#include <stdint.h>

#define EEPROM_PAGE_SIZE  256
#define EEPROM_BLOCK_SIZE 4096
#define EEPROM_MAX_SIZE   0x8000

uint32_t host_fake_eeprom_read(uint32_t addr, void *buf, uint32_t len);
uint32_t host_fake_eeprom_write(uint32_t addr, const void *buf, uint32_t len);
uint32_t host_fake_eeprom_erase(uint32_t addr, uint32_t len);

#define EEPROM_READ(StartAddr, Buffer, Length) \
    host_fake_eeprom_read((StartAddr), (Buffer), (Length))
#define EEPROM_WRITE(StartAddr, Buffer, Length) \
    host_fake_eeprom_write((StartAddr), (Buffer), (Length))
#define EEPROM_ERASE(StartAddr, Length) \
    host_fake_eeprom_erase((StartAddr), (Length))

#endif
