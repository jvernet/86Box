/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Implementation of the Intel PIC chip emulation.
 *
 *
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2016-2020 Miran Grca.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/machine.h>
#include <86box/io.h>
#include <86box/pci.h>
#include <86box/pic.h>
#include <86box/timer.h>
#include <86box/pit.h>


int		output;
int		intclear;
int		keywaiting = 0;
int		pic_intpending;
PIC		pic, pic2;
uint16_t	pic_current;


static int	shadow = 0;


#ifdef ENABLE_PIC_LOG
int pic_do_log = ENABLE_PIC_LOG;


static void
pic_log(const char *fmt, ...)
{
    va_list ap;

    if (pic_do_log) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
}
#else
#define pic_log(fmt, ...)
#endif


int	picinterrupt_poll(int is_pic2);


void
pic_updatepending()
{
    uint16_t temp_pending = 0;
    if (AT) {
	if ((pic2.pend & ~pic2.mask) & ~pic2.mask2)
		pic.pend |= pic.icw3;
	else
		pic.pend &= ~pic.icw3;
    }
    pic_intpending = (pic.pend & ~pic.mask) & ~pic.mask2;
    if (AT) {
	if (!((pic.mask | pic.mask2) & pic.icw3)) {
		temp_pending = ((pic2.pend&~pic2.mask)&~pic2.mask2);
		temp_pending <<= 8;
		pic_intpending |= temp_pending;
	}
    }
    pic_log("pic_intpending = %i  %02X %02X %02X %02X\n", pic_intpending, pic.ins, pic.pend, pic.mask, pic.mask2);
    pic_log("                    %02X %02X %02X %02X %i %i\n", pic2.ins, pic2.pend, pic2.mask, pic2.mask2, ((pic.mask | pic.mask2) & (1 << 2)), ((pic2.pend&~pic2.mask)&~pic2.mask2));
}


void
pic_reset()
{
    pic.icw=0;
    pic.mask=0xFF;
    pic.mask2=0;
    pic.pend=pic.ins=0;
    pic.vector=8;
    pic.read=1;
    pic2.icw=0;
    pic2.mask=0xFF;
    pic.mask2=0;
    pic2.pend=pic2.ins=0;
    pic_intpending = 0;
}


void
pic_set_shadow(int sh)
{
    shadow = sh;
}


void
pic_update_mask(uint8_t *mask, uint8_t ins)
{
    int c;
    *mask = 0;
    for (c = 0; c < 8; c++) {
	if (ins & (1 << c)) {
		*mask = 0xff << c;
		return;
	}
    }
}


static int
picint_is_level(uint16_t irq)
{
    if (PCI)
	return pci_irq_is_level(irq);
    else {
	if (irq < 8)
		return (pic.icw1 & 8) ? 1 : 0;
	else
		return (pic2.icw1 & 8) ? 1 : 0;
    }
}


/* Should this really EOI *ALL* IRQ's at once? */
static void
pic_autoeoi()
{
    int c;

    for (c = 0; c < 8; c++) {
	if (pic.ins & ( 1 << c)) {
		pic.ins &= ~(1 << c);
		pic_update_mask(&pic.mask2, pic.ins);

		if (AT) {
			if (((1 << c) == pic.icw3) && (pic2.pend & ~pic2.mask) & ~pic2.mask2)
				pic.pend |= pic.icw3;
		}

		pic_updatepending();
		return;
	}
    }
}


void
pic_write(uint16_t addr, uint8_t val, void *priv)
{
    int c;

    addr &= ~0x06;

    if (addr&1) {
	pic_log("%04X:%04X: write: %02X\n", CS, cpu_state.pc, val);
	switch (pic.icw) {
		case 0: /*OCW1*/
			pic.mask=val;
			pic_updatepending();
			break;
		case 1: /*ICW2*/
			pic.vector=val&0xF8;
			pic_log("ICW%i ->", pic.icw + 1);
			if (pic.icw1 & 2) pic.icw=3;
			else		  pic.icw=2;
			pic_log("ICW%i\n", pic.icw + 1);
			break;
		case 2: /*ICW3*/
			pic.icw3 = val;
			pic_log("PIC1 ICW3 now %02X\n", val);
			pic_log("ICW%i ->", pic.icw + 1);
			if (pic.icw1 & 1) pic.icw=3;
			else		  pic.icw=0;
			pic_log("ICW%i\n", pic.icw + 1);
			break;
		case 3: /*ICW4*/
			pic_log("ICW%i ->", pic.icw + 1);
			pic.icw4 = val;
			pic.icw=0;
			pic_log("ICW%i\n", pic.icw + 1);
			break;
	}
    } else {
	if (val & 16) { /*ICW1*/
		pic.mask = 0;
		pic.mask2 = 0;
		pic_log("ICW%i ->", pic.icw + 1);
		pic.icw = 1;
		pic.icw1 = val;
		pic_log("ICW%i\n", pic.icw + 1);
		pic.ins = 0;
		pic.pend = 0;	/* Pending IRQ's are cleared. */
		pic_updatepending();
	}
	else if (!(val & 8)) { /*OCW2*/
		pic.ocw2 = val;
		if ((val & 0xE0) == 0x60) {
			pic.ins &= ~(1 << (val & 7));
			pic_update_mask(&pic.mask2, pic.ins);
			if (AT) {
				if (((val&7) == pic2.icw3) && (pic2.pend&~pic2.mask)&~pic2.mask2)
					pic.pend |= pic.icw3;
			}

			pic_updatepending();
		} else {
			for (c = 0; c < 8; c++) {
				if (pic.ins & (1 << c)) {
					pic.ins &= ~(1 << c);
					pic_update_mask(&pic.mask2, pic.ins);

					if (AT) {
						if (((1 << c) == pic.icw3) && (pic2.pend & ~pic2.mask) & ~pic2.mask2)
							pic.pend |= pic.icw3;
					}

					if ((c == 1) && keywaiting)
						intclear &= ~1;
					pic_updatepending();
					return;
				}
			}
		}
	} else {               /*OCW3*/
		pic.ocw3 = val;
		if (val & 4)
			pic.read=4;
		if (val & 2)
			pic.read=(val & 1);
	}
    }
}


uint8_t
pic_read(uint16_t addr, void *priv)
{
    uint8_t ret = 0xff;
    int temp;

    if ((addr == 0x20) && shadow) {
	ret  = ((pic.ocw3   & 0x20) >> 5) << 4;
	ret |= ((pic.ocw2   & 0x80) >> 7) << 3;
	ret |= ((pic.icw4   & 0x10) >> 4) << 2;
	ret |= ((pic.icw4   & 0x02) >> 1) << 1;
	ret |= ((pic.icw4   & 0x08) >> 3) << 0;
    } else if ((addr == 0x21) && shadow)
	ret  = ((pic.vector & 0xf8) >> 3) << 0;
    else if (addr & 1)
	ret = pic.mask;
    else if (pic.read & 4) {
	temp = picinterrupt_poll(0);
	if (temp >= 0)
		ret = temp | 0x80;
	else
		ret = 0x00;
    } else if (pic.read) {
	if (AT)
		ret =  pic.ins | (pic2.ins ? 4 : 0);
	else
		ret = pic.ins;
    } else
	ret = pic.pend;

    pic_log("%04X:%04X: Read PIC 1 port %04X, value %02X\n", CS, cpu_state.pc, addr, val);

    return ret;
}


void
pic_init()
{
    shadow = 0;
    io_sethandler(0x0020, 0x0002, pic_read, NULL, NULL, pic_write, NULL, NULL, NULL);
}


void
pic_init_pcjr()
{
    shadow = 0;
    io_sethandler(0x0020, 0x0008, pic_read, NULL, NULL, pic_write, NULL, NULL, NULL);
}


static void
pic2_autoeoi()
{
    int c;

    for (c = 0; c < 8; c++) {
	if (pic2.ins & (1 << c)) {
		pic2.ins &= ~(1 << c);
		pic_update_mask(&pic2.mask2, pic2.ins);

		pic_updatepending();
		return;
	}
    }
}


void
pic2_write(uint16_t addr, uint8_t val, void *priv)
{
    int c;
    if (addr & 1) {
	switch (pic2.icw) {
		case 0: /*OCW1*/
			pic2.mask=val;
			pic_updatepending();
			break;
		case 1: /*ICW2*/
			pic2.vector=val & 0xF8;
			pic_log("PIC2 vector now: %02X\n", pic2.vector);
			if (pic2.icw1 & 2) pic2.icw=3;
			else		   pic2.icw=2;
			break;
		case 2: /*ICW3*/
			pic2.icw3 = val;
			pic_log("PIC2 ICW3 now %02X\n", val);
			if (pic2.icw1 & 1) pic2.icw=3;
			else		   pic2.icw=0;
			break;
		case 3: /*ICW4*/
			pic2.icw4 = val;
			pic2.icw=0;
			break;
	}
    } else {
	if (val & 16) { /*ICW1*/
		pic2.mask = 0;
		pic2.mask2 = 0;
		pic2.icw = 1;
		pic2.icw1 = val;
		pic2.ins = 0;
		pic2.pend = 0;	/* Pending IRQ's are cleared. */
		pic.pend &= ~4;
		pic_updatepending();
	} else if (!(val & 8)) { /*OCW2*/
#ifdef ENABLE_PIC_LOG
		switch ((val >> 5) & 0x07) {
			case 0x00:
				pic_log("Rotate in automatic EOI mode (clear)\n");
				break;
			case 0x01:
				pic_log("Non-specific EOI command\n");
				break;
			case 0x02:
				pic_log("No operation\n");
				break;
			case 0x03:
				pic_log("Specific EOI command\n");
				break;
			case 0x04:
				pic_log("Rotate in automatic EOI mode (set)\n");
				break;
			case 0x05:
				pic_log("Rotate on on-specific EOI command\n");
				break;
			case 0x06:
				pic_log("Set priority command\n");
				break;
			case 0x07:
				pic_log("Rotate on specific EOI command\n");
				break;
		}
#endif

		pic2.ocw2 = val;
		if ((val & 0xE0) == 0x60) {
			pic2.ins &= ~(1 << (val & 7));
			pic_update_mask(&pic2.mask2, pic2.ins);

			pic_updatepending();
		} else {
			for (c = 0; c < 8; c++) {
				if (pic2.ins&(1<<c)) {
					pic2.ins &= ~(1<<c);
					pic_update_mask(&pic2.mask2, pic2.ins);
					pic_updatepending();
					return;
				}
			}
		}
	} else {               /*OCW3*/
		pic2.ocw3 = val;
		if (val & 4)
			pic2.read=4;
		if (val & 2)
			pic2.read=(val & 3);
	}
    }
}


uint8_t
pic2_read(uint16_t addr, void *priv)
{
    uint8_t ret = 0xff;
    int temp;

    if ((addr == 0xa0) && shadow) {
	ret  = ((pic2.ocw3   & 0x20) >> 5) << 4;
	ret |= ((pic2.ocw2   & 0x80) >> 7) << 3;
	ret |= ((pic2.icw4   & 0x10) >> 4) << 2;
	ret |= ((pic2.icw4   & 0x02) >> 1) << 1;
	ret |= ((pic2.icw4   & 0x08) >> 3) << 0;
    } else if ((addr == 0xa1) && shadow)
	ret  = ((pic2.vector & 0xf8) >> 3) << 0;
    else if (addr & 1)
	ret = pic2.mask;
    else if (pic2.read & 4) {
	temp = picinterrupt_poll(1);
	if (temp >= 0)
		ret = (temp | 0x80);
	else
		ret = 0x00;
    } else if (pic2.read)
	ret = pic2.ins;
    else
	ret = pic2.pend;

    pic_log("%04X:%04X: Read PIC 2 port %04X, value %02X\n", CS, cpu_state.pc, addr, val);

    return ret;
}


void
pic2_init()
{
    io_sethandler(0x00a0, 0x0002, pic2_read, NULL, NULL, pic2_write, NULL, NULL, NULL);
}


void
clearpic()
{
    pic.pend=pic.ins=pic_current=0;
    pic_updatepending();
}


void
picint_common(uint16_t num, int level)
{
    int c = 0;

    if (!num) {
	pic_log("Attempting to raise null IRQ\n");
	return;
    }

    if (AT && (num == pic.icw3) && (pic.icw3 == 4))
	num = 1 << 9;

    while (!(num & (1 << c)))
	c++;

    if (AT && (num == pic.icw3) && (pic.icw3 != 4)) {
	pic_log("Attempting to raise cascaded IRQ %i\n");
	return;
    }

    if (!(pic_current & num) || !level) {
	pic_log("Raising IRQ %i\n", c);

	if (level)
                pic_current |= num;

	if (AT && (cpu_fast_off_flags & num))
		cpu_fast_off_count = cpu_fast_off_val + 1;

        if (num>0xFF) {
		if (!AT)
			return;

		pic2.pend|=(num>>8);
		if ((pic2.pend&~pic2.mask)&~pic2.mask2)
			pic.pend |= (1 << pic2.icw3);
        } else
                pic.pend|=num;

        pic_updatepending();
    }
}


void
picint(uint16_t num)
{
    picint_common(num, 0);
}


void
picintlevel(uint16_t num)
{
    picint_common(num, 1);
}


void
picintc(uint16_t num)
{
    int c = 0;

    if (!num) {
	pic_log("Attempting to lower null IRQ\n");
	return;
    }

    if (AT && (num == pic.icw3) && (pic.icw3 == 4))
	num = 1 << 9;

    while (!(num & (1 << c)))
	c++;

    if (AT && (num == pic.icw3) && (pic.icw3 != 4)) {
	pic_log("Attempting to lower cascaded IRQ %i\n");
	return;
    }

    if (pic_current & num)
        pic_current &= ~num;

    pic_log("Lowering IRQ %i\n", c);

    if (num > 0xff) {
	if (!AT)
		return;

	pic2.pend &= ~(num >> 8);
	if (!((pic2.pend&~pic2.mask)&~pic2.mask2))
		pic.pend &= ~(1 << pic2.icw3);
    } else
	pic.pend&=~num;

    pic_updatepending();
}


static int
pic_process_interrupt(PIC* target_pic, int c)
{
    uint8_t pending = target_pic->pend & ~target_pic->mask;
    int ret = -1;
    /* TODO: On init, a PIC need to get a pointer to one of these, and rotate as needed
	     if in rotate mode. */
			/*   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 */
    int priority_xt[16] = {  7,  6,  5,  4,  3,  2,  1,  0, -1, -1, -1, -1, -1, -1, -1, -1 };
    int priority_at[16] = { 14, 13, -1,  4,  3,  2,  1,  0, 12, 11, 10,  9,  8,  7,  6,  5  };
    int i;

    int pic_int = c & 7;
    int pic_int_num = 1 << pic_int;

    int in_service = 0;

    if (AT) {
	for (i = 0; i < 16; i++) {
		if ((priority_at[i] != -1) && (priority_at[i] >= priority_at[c])) {
			if (i < 8)
				in_service |= (pic.ins & (1 << i));
			else
				in_service |= (pic2.ins & (1 << i));
		}
	}
    } else {
	for (i = 0; i < 16; i++) {
		if ((priority_xt[i] != -1) && (priority_xt[i] >= priority_xt[c]))
			in_service |= (pic.ins & (1 << i));
	}
    }

    if ((pending & pic_int_num) && !in_service) {
	if (!((pic_current & (1 << c)) && picint_is_level(c)))
		target_pic->pend &= ~pic_int_num;
	else if (!picint_is_level(c))
		target_pic->pend &= ~pic_int_num;
	target_pic->ins |= pic_int_num;
	pic_update_mask(&target_pic->mask2, target_pic->ins);

	if (AT && (c >= 8)) {
		if (!((target_pic->pend & ~target_pic->mask) & ~target_pic->mask2))
			pic.pend &= ~(1 << pic2.icw3);
		pic.ins |= (1 << pic2.icw3); /*Cascade IRQ*/
		pic_update_mask(&pic.mask2, pic.ins);
	}

	pic_updatepending();

	if (target_pic->icw4 & 0x02)
		(AT && (c >= 8)) ? pic2_autoeoi() : pic_autoeoi();

	if (!c && (pit2 != NULL))
		pit_ctr_set_gate(&pit2->counters[0], 0);

	ret = pic_int + target_pic->vector;
    }

    return ret;
}


int
picinterrupt()
{
    int c, d;
    int ret;

    for (c = 0; c <= 7; c++) {
	if (AT && ((1 << c) == pic.icw3)) {
		for (d = 8; d <= 15; d++) {
			ret = pic_process_interrupt(&pic2, d);
			if (ret != -1)  return ret;
		}
	} else {
		ret = pic_process_interrupt(&pic, c);
		if (ret != -1)  return ret;
	}
    }
    return -1;
}


int
picinterrupt_poll(int is_pic2)
{
    int c, d;
    int ret;

    if (is_pic2)
	pic2.read &= ~4;
    else
	pic.read &= ~4;

    for (c = 0; c <= 7; c++) {
	if (AT && ((1 << c) == pic.icw3)) {
		for (d = 8; d <= 15; d++) {
			ret = pic_process_interrupt(&pic2, d);
			if ((ret != -1) && is_pic2)  return c & 7;
		}
	} else {
		ret = pic_process_interrupt(&pic, c);
		if ((ret != -1) && !is_pic2)  return c;
	}
    }
    return -1;
}


void
dumppic()
{
    pic_log("PIC1 : MASK %02X PEND %02X INS %02X LEVEL %02X VECTOR %02X CASCADE %02X\n", pic.mask, pic.pend, pic.ins, (pic.icw1 & 8) ? 1 : 0, pic.vector, pic.icw3);
    if (AT)
	pic_log("PIC2 : MASK %02X PEND %02X INS %02X LEVEL %02X VECTOR %02X CASCADE %02X\n", pic2.mask, pic2.pend, pic2.ins, (pic2.icw1 & 8) ? 1 : 0, pic2.vector, pic2.icw3);
}
