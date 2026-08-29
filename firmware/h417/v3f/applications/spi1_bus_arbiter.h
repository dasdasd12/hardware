#ifndef V3F_SPI1_BUS_ARBITER_H
#define V3F_SPI1_BUS_ARBITER_H

#include <stdint.h>

/* Returns non-zero only while V3F owns HSEM31 and SPI1 may be used. */
uint8_t v3f_spi1_bus_arbiter_init(void);
uint8_t v3f_spi1_bus_arbiter_service(void);
uint8_t v3f_spi1_bus_arbiter_transfer_allowed(void);

#endif
