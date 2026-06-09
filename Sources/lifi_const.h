#ifndef LIFI_CONST_H
#define LIFI_CONST_H

#define LIFI_HALF_BIT_MS          1000U
#define LIFI_TIMER_PRESCALER      (160U - 1)
#define LIFI_TIMER_PERIOD         (LIFI_HALF_BIT_MS - 1)

#define T_SHORT_MIN               (LIFI_HALF_BIT_MS * 8U / 10U)
#define T_SHORT_MAX               (LIFI_HALF_BIT_MS * 12U / 10U)
#define T_LONG_MIN                (LIFI_HALF_BIT_MS * 18U / 10U)
#define T_LONG_MAX                (LIFI_HALF_BIT_MS * 22U / 10U)

#endif