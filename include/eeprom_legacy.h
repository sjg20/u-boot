/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2019 Google LLC
 */

#ifndef __EEPROM_LEGACY_H
#define __EEPROM_LEGACY_H

#ifdef CONFIG_CMD_EEPROM
void eeprom_init(int bus);
int eeprom_read(uint dev_addr, uint offset, uchar *buffer, uint cnt);
int eeprom_write(uint dev_addr, uint offset, uchar *buffer, uint cnt);
#else
/*
 * Some EEPROM code is depecated because it used the legacy I2C interface. Add
 * some macros here so we don't have to touch every one of those uses
 */
#define eeprom_init(bus)
#define eeprom_read(dev_addr, offset, buffer, cnt) ((void)-ENOSYS)
#define eeprom_write(dev_addr, offset, buffer, cnt) ((void)-ENOSYS)
#endif

#endif
