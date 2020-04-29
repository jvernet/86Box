/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Definitions for the ACPI emulation.
 *
 *
 *
 * Authors:	Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020 Miran Grca.
 */
#ifndef ACPI_H
# define ACPI_H


#ifdef __cplusplus
extern "C" {
#endif

#define ACPI_TIMER_FREQ	3579545
#define PM_FREQ		ACPI_TIMER_FREQ

#define RSM_STS		(1 << 15)
#define PWRBTN_STS	(1 << 8)

#define RTC_EN		(1 << 10)
#define PWRBTN_EN	(1 << 8)
#define GBL_EN		(1 << 5)
#define TMROF_EN	(1 << 0)

#define SCI_EN		(1 << 0)
#define SUS_EN		(1 << 13)

#define ACPI_ENABLE	0xf1
#define	ACPI_DISABLE	0xf0

#define VEN_INTEL	0x8086
#define VEN_VIA		0x1106


typedef struct
{
    uint8_t		plvl2, plvl3,
			smicmd, gpio_dir,
			gpio_val, extsmi_val,
			timer32,
			gpireg[3], gporeg[4];
    uint16_t		pmsts, pmen,
			pmcntrl, gpsts,
			gpen, io_base,
			gpscien, gpsmien,
			pscntrl, gpo_val,
			gpi_val;
    int			slot, irq_mode,
			irq_pin, smi_lock,
			smi_active;
    uint32_t		pcntrl, glbsts,
			devsts, glben,
			glbctl, devctl,
			padsts, paden,
			gptren, timer_val;
    uint64_t		tmr_overflow_time;
} acpi_regs_t;


typedef struct
{
    acpi_regs_t		regs;
    uint8_t		gporeg_default[4];
    int			vendor;
    pc_timer_t		timer;
    nvr_t		*nvr;
    apm_t		*apm;
} acpi_t;


/* Global variables. */
extern const device_t	acpi_intel_device;
extern const device_t	acpi_via_device;


/* Functions. */
extern void		acpi_update_io_mapping(acpi_t *dev, uint32_t base, int chipset_en);
extern void		acpi_init_gporeg(acpi_t *dev, uint8_t val0, uint8_t val1, uint8_t val2, uint8_t val3);
extern void		acpi_set_timer32(acpi_t *dev, uint8_t timer32);
extern void		acpi_set_slot(acpi_t *dev, int slot);
extern void		acpi_set_irq_mode(acpi_t *dev, int irq_mode);
extern void		acpi_set_irq_pin(acpi_t *dev, int irq_pin);
extern void		acpi_set_nvr(acpi_t *dev, nvr_t *nvr);

#ifdef __cplusplus
}
#endif


#endif	/*ACPI_H*/
