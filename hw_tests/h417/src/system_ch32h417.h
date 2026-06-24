#ifndef SYSTEM_CH32H417_H
#define SYSTEM_CH32H417_H

#include <stdint.h>

extern uint32_t SystemCoreClock;
extern uint32_t SystemClock;
extern uint32_t HCLKClock;

void SystemInit(void);
void SystemAndCoreClockUpdate(void);

#endif
