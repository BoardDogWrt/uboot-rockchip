/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * Copyright (c) 2022 FriendlyElec Computer Tech. Co., Ltd.
 * (http://www.friendlyarm.com)
 *
 * (C) Copyright 2020 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <adc.h>
#include <i2c.h>
#include <misc.h>
#include <asm/setup.h>
#include <usb.h>
#include <dwc3-uboot.h>

#include "hwrev.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_USB_DWC3
static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_HIGH,
	.base = 0xfcc00000,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.index = 0,
	.dis_u2_susphy_quirk = 1,
	.usb2_phyif_utmi_width = 16,
};

int usb_gadget_handle_interrupts(void)
{
	dwc3_uboot_handle_interrupt(0);
	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
{
	return dwc3_uboot_init(&dwc3_device_data);
}
#endif

/* Supported panels and dpi for nanopi5 series */
static char *panels[] = {
	"HD702E,213dpi",
	"HD703E,213dpi",
	"HDMI1024x768,165dpi",
	"HDMI1280x800,168dpi",
};

char *board_get_panel_name(void)
{
	char *name;
	int i;

	name = env_get("panel");
	if (!name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(panels); i++) {
		if (!strncmp(panels[i], name, strlen(name)))
			return panels[i];
	}

	return name;
}

int board_select_fdt_index(ulong dt_table_hdr, struct blk_desc *dev_desc)
{
	return (dev_desc ? dev_desc->devnum : 0);
}

static int board_check_supply(void)
{
	u32 adc_reading = 0;
	int mv = 5000;

	adc_channel_single_shot("saradc", 2, &adc_reading);
	debug("ADC reading %d\n", adc_reading);

	/* should be in 3v ~ 20v */
	if (adc_reading > 160 && adc_reading < 1024) {
		mv = adc_reading * 196 - 2130;
		mv /= 10;
	}

	printf("vdd_usbc %d mV\n", mv);

	return 0;
}

static int mac_read_from_generic_eeprom(u8 *addr)
{
	struct udevice *i2c_dev;
	int ret;

	/* Microchip 24AA02xxx EEPROMs with EUI-48 Node Identity */
	ret = i2c_get_chip_for_busnum(5, 0x53, 1, &i2c_dev);
	if (!ret)
		ret = dm_i2c_read(i2c_dev, 0xfa, addr, 6);

	return ret;
}

static void __maybe_unused setup_macaddr(void)
{
	u8 mac_addr[6] = { 0 };
	int lockdown = 0;
	int ret;

#ifndef CONFIG_ENV_IS_NOWHERE
	lockdown = env_get_yesno("lockdown") == 1;
#endif
	if (lockdown && env_get("ethaddr"))
		return;

	ret = mac_read_from_generic_eeprom(mac_addr);
	if (!ret && is_valid_ethaddr(mac_addr)) {
		debug("MAC: %pM\n", mac_addr);
		eth_env_set_enetaddr("ethaddr", mac_addr);
	}
}

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
	setup_macaddr();

	return 0;
}
#endif

#ifdef CONFIG_DISPLAY_BOARDINFO
int show_board_info(void)
{
	printf("Board: %s\n", get_board_name());

	return 0;
}
#endif

#ifdef CONFIG_REVISION_TAG
static void set_board_rev(void)
{
	char info[64] = {0, };

	snprintf(info, ARRAY_SIZE(info), "%02x", get_board_rev());
	env_set("board_rev", info);
}
#endif

void set_dtb_name(void)
{
	char info[64] = {0, };

#ifndef CONFIG_ENV_IS_NOWHERE
	if (env_get_yesno("lockdown") == 1 &&
		env_get("dtb_name"))
		return;
#endif

	snprintf(info, ARRAY_SIZE(info),
			"rk3568-nanopi5-rev%02x.dtb", get_board_rev());
	env_set("dtb_name", info);
}

#ifdef CONFIG_SERIAL_TAG
void get_board_serial(struct tag_serialnr *serialnr)
{
	char *serial_string;
	u64 serial = 0;

	serial_string = env_get("serial#");

	if (serial_string)
		serial = simple_strtoull(serial_string, NULL, 16);

	serialnr->high = (u32)(serial >> 32);
	serialnr->low = (u32)(serial & 0xffffffff);
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int rk_board_late_init(void)
{
	board_check_supply();

#ifdef CONFIG_REVISION_TAG
	set_board_rev();
#endif

#ifdef CONFIG_SILENT_CONSOLE
	gd->flags &= ~GD_FLG_SILENT;
#endif

	printf("\n");

	return 0;
}
#endif
