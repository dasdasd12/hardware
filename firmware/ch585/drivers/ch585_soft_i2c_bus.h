#ifndef CH585_SOFT_I2C_BUS_H
#define CH585_SOFT_I2C_BUS_H

#include "ch585_i2c_bus.h"

/* PB20=SDA and PB21=SCL on both CH585 halves. The right half uses this bus
 * for MAX17048; the left-half EEPROM remains on its existing proven driver. */
int ch585_soft_i2c_bus_init(ch585_i2c_bus_t *bus);

#endif /* CH585_SOFT_I2C_BUS_H */
