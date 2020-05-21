/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Common functions for hardware monitoring chips.
 *
 *
 *
 * Author:	RichardG, <richardg867@gmail.com>
 *
 *		Copyright 2020 RichardG.
 */

#include <stdint.h>
#include <86box/device.h>
#include <86box/hwm.h>


hwm_values_t hwm_values;


void
hwm_set_values(hwm_values_t new_values)
{
    hwm_values = new_values;
}


hwm_values_t*
hwm_get_values()
{
    return &hwm_values;
}
