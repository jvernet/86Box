/* TO 16 P>C*/
/* 8088     */
/* RAM 512 to 768k*/
/* if 768k, these extra 128k can be used as EMS RAM*/
/* */
/* SW1 1-4     */
/* SW2 1-8   Can be read thru port C of PPI8253 */
/* SW3 1-8   Can be read thru port OE0h */
/* SW4       Monitor TYPE*/
/* JP1       Internal card video mode*/
/* turbo mode 0C0h-0C3H*/



#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/io.h>
#include <86box/timer.h>
#include <86box/pit.h>
#include <86box/nmi.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/nvr.h>
#include <86box/keyboard.h>
#include <86box/lpt.h>
#include <86box/mem.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/gameport.h>
#include <86box/video.h>
#include <86box/vid_cga.h>
#include <86box/vid_colorplus.h>
#include <86box/vid_cga_comp.h>
#include <86box/plat.h>
#include <86box/machine.h>
#include <86box/m_xt_to16pc.h>

#define PLANTRONICS_MODE 1
#define TO16PC_MODE 0

typedef struct {
	/* SWitch registers */
	uint8_t	SW1;
	uint8_t	SW2;
	uint8_t	turbo;

	
	/* EMS data */
	uint8_t		ems_reg[4];
	mem_mapping_t	mapping[4];
	uint32_t	page_exec[4];
	uint8_t		ems_port_index;
	uint16_t	ems_port;
	uint8_t		is_640k;
	uint32_t	ems_base;
	int32_t		ems_pages;

	fdc_t* fdc;

	
} to16PC_t;

typedef struct {

	colorplus_t colorplus;
	int mode;
} to16pc_vid_t;

static to16PC_t	to16PC;
video_timings_t timing_to16pc_vid = { VIDEO_ISA, 8, 16, 32,   8, 16, 32 };

static void
to16pc_vid_close(void* priv)
{
	to16pc_vid_t* vid = (to16pc_vid_t*)priv;

	free(vid->colorplus.cga.vram);
	free(vid);
}


static void
to16pc_vid_speed_changed(void* priv)
{
	to16pc_vid_t* vid = (to16pc_vid_t*)priv;

	colorplus_recalctimings(&vid->colorplus);
	// ogc_recalctimings(&vid->ogc);
}


const device_t to16pc_vid_device = {
    "TO16PC graphics card",
    0, 0,
    NULL, to16pc_vid_close, NULL,
    { NULL },
    to16pc_vid_speed_changed,
    NULL,
    NULL
};

static uint8_t
to16pc_read(uint16_t addr, void* priv)
{
	to16PC_t* sys = (to16PC_t*)priv;
	uint8_t tmp = 0xff;

	switch (addr) {
	case 0xe0:
	case 0xe1:
	case 0xe3:
/* 1-4: ems page frame*/
/* all OFF desactivate */
/* 5 memory type always set to OFF*/
/* 6 OFF defaut deactivate page frame */
/* 7 BIOS actif always ON*/
/* 8 OFF 4.77 Mhz ON 9.54*/
		return 192;
		break;

	case 0xc3: 
		break;
	}

	return(tmp);
}


static void
to16pc_write(uint16_t addr, uint8_t val, void* priv)
{
	to16PC_t* sys = (to16PC_t*)priv;

	
	switch (addr) {
	case 0xc0:
		break;

	case 0xc1:	/* Write next byte to NVRAM */
		break;

	case 0xc2:
		break;

	case 0xc3:
		break;
	}
}


static void
to16pc_turbo_set(uint8_t value)
{
	if (value == to16PC.turbo) return;

	to16PC.turbo = value;
	if (!value)
		cpu_dynamic_switch(0);
	else
		cpu_dynamic_switch(cpu);
}

const device_t*
to16pc_get_device(void)
{
	return(&to16pc_vid_device);
}



static void
to16pc_vid_init(to16pc_vid_t* vid)
{
	/* int display_type; */
	vid->mode = PLANTRONICS_MODE;

	colorplus_standalone_init(vid);
}

int
machine_xt_to16pc_init(const machine_t* model)
{
	FILE* f;
	int pg;

	int ret;

	ret = bios_load_linear(L"roms/machines/to16pc/TO16_103.bin",
				0x000f8000, 32768, 0);


	if (bios_only || !ret)
		return ret;


	to16pc_vid_t* vid;

	/* Do not move memory allocation elsewhere. */
	vid = (to16pc_vid_t*)malloc(sizeof(to16pc_vid_t));
	memset(vid, 0x00, sizeof(to16pc_vid_t));
	memset(&to16PC, 0x00, sizeof(to16PC));
	to16PC.turbo = 0x80;
	
		/* System control functions, and add-on memory board */
	io_sethandler(0xe0, 16,
		to16pc_read, NULL, NULL, to16pc_write, NULL, NULL, &to16PC);
	io_sethandler(0xe1, 16,
		to16pc_read, NULL, NULL, to16pc_write, NULL, NULL, &to16PC);
	io_sethandler(0xe2, 16,
		to16pc_read, NULL, NULL, to16pc_write, NULL, NULL, &to16PC);
	io_sethandler(0xe3, 16,
		to16pc_read, NULL, NULL, to16pc_write, NULL, NULL, &to16PC);

	machine_common_init(model);
	pit_ctr_set_out_func(&pit->counters[1], pit_refresh_timer_xt);

	device_add(&keyboard_xt_device);
	to16PC.fdc = device_add(&fdc_xt_device);
	
	/*if (gfxcard == VID_INTERNAL)
		device_add(&to16PC_video_device);
	*/

	to16pc_vid_init(vid);
	device_add_ex(&to16pc_vid_device, vid);

	nmi_init();

	
	return ret;
}
