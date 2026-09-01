/****************************************************************************
 * Minimal Modbus read (nanoMODBUS) for alarm monitor — read-only.
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_VG_AGENT_OPS

#include "vg_modbus_read.h"
#include "modbus_port_openvela.h"
#include "nanomodbus.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifndef CONFIG_VG_RS485_DEVPATH
#  define CONFIG_VG_RS485_DEVPATH "/dev/rs485"
#endif

#define VG_ALARM_READ_TO_MS  500
#define VG_ALARM_BYTE_TO_MS  50
#define VG_ALARM_MAX_REGS    8

int vg_modbus_read_holding(uint8_t slave, uint16_t start, uint16_t qty,
                           uint16_t *regs)
{
  struct vg_modbus_port_s port;
  nmbs_platform_conf pc;
  nmbs_t nmbs;
  nmbs_error err;

  if (slave == 0 || qty == 0 || qty > VG_ALARM_MAX_REGS || regs == NULL)
    {
      return -EINVAL;
    }

  port.fd = -1;
  if (vg_modbus_port_open(&port, CONFIG_VG_RS485_DEVPATH) < 0)
    {
      return -errno;
    }

  vg_modbus_port_bind(&port, &pc);
  err = nmbs_client_create(&nmbs, &pc);
  if (err != NMBS_ERROR_NONE)
    {
      vg_modbus_port_close(&port);
      return -EIO;
    }

  nmbs_set_read_timeout(&nmbs, VG_ALARM_READ_TO_MS);
  nmbs_set_byte_timeout(&nmbs, VG_ALARM_BYTE_TO_MS);
  nmbs_set_destination_rtu_address(&nmbs, slave);

  memset(regs, 0, sizeof(uint16_t) * qty);
  err = nmbs_read_holding_registers(&nmbs, start, qty, regs);
  vg_modbus_port_close(&port);

  return (err == NMBS_ERROR_NONE) ? 0 : -EIO;
}

#endif /* CONFIG_VG_AGENT_OPS */
