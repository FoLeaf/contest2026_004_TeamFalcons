/****************************************************************************
 * Minimal Modbus holding-register read for VelaGuard alarm monitor.
 ****************************************************************************/

#ifndef __APP_VELAGUARD_VG_MODBUS_READ_H
#define __APP_VELAGUARD_VG_MODBUS_READ_H

#include <stdint.h>

#ifdef CONFIG_VG_AGENT_OPS

int vg_modbus_read_holding(uint8_t slave, uint16_t start, uint16_t qty,
                           uint16_t *regs);

#else

static inline int vg_modbus_read_holding(uint8_t slave, uint16_t start,
                                         uint16_t qty, uint16_t *regs)
{
  (void)slave;
  (void)start;
  (void)qty;
  (void)regs;
  return -1;
}

#endif

#endif
