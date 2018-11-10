/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/gpio.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <dt-bindings/pinctrl/sun4i-a10.h>

void sunxi_gpio_set_cfgbank(struct sunxi_gpio *pio, int bank_offset, u32 val)
{
	u32 index = GPIO_CFG_INDEX(bank_offset);
	u32 offset = GPIO_CFG_OFFSET(bank_offset);

	clrsetbits_le32(&pio->cfg[0] + index, 0xf << offset, val << offset);
}

void sunxi_gpio_set_cfgpin(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	struct sunxi_gpio *pio = BANK_TO_GPIO(bank);

	sunxi_gpio_set_cfgbank(pio, pin, val);
}

int sunxi_gpio_get_cfgbank(struct sunxi_gpio *pio, int bank_offset)
{
	u32 index = GPIO_CFG_INDEX(bank_offset);
	u32 offset = GPIO_CFG_OFFSET(bank_offset);
	u32 cfg;

	cfg = readl(&pio->cfg[0] + index);
	cfg >>= offset;

	return cfg & 0xf;
}

int sunxi_gpio_get_cfgpin(u32 pin)
{
	u32 bank = GPIO_BANK(pin);
	struct sunxi_gpio *pio = BANK_TO_GPIO(bank);

	return sunxi_gpio_get_cfgbank(pio, pin);
}

int sunxi_gpio_set_drv(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	u32 index = GPIO_DRV_INDEX(pin);
	u32 offset = GPIO_DRV_OFFSET(pin);
	struct sunxi_gpio *pio = BANK_TO_GPIO(bank);

	clrsetbits_le32(&pio->drv[0] + index, 0x3 << offset, val << offset);

	return 0;
}

int sunxi_gpio_set_pull(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	u32 index = GPIO_PULL_INDEX(pin);
	u32 offset = GPIO_PULL_OFFSET(pin);
	struct sunxi_gpio *pio = BANK_TO_GPIO(bank);

	clrsetbits_le32(&pio->pull[0] + index, 0x3 << offset, val << offset);

	return 0;
}

int sunxi_gpio_parse_pin_name(const char *pin_name)
{
	int pin;

	if (pin_name[0] != 'P')
		return -1;

	if (pin_name[1] < 'A' || pin_name[1] > 'Z')
		return -1;

	pin = (pin_name[1] - 'A') << 5;
	pin += simple_strtol(&pin_name[2], NULL, 10);

	return pin;
}

int sunxi_gpio_setup_dt_pins(const void * volatile fdt_blob, int node,
			     const char * mux_name, int mux_sel)
{
	int drive, pull, pin, i;
	const char *pin_name;
	int offset;

	offset = fdtdec_lookup_phandle(fdt_blob, node, "pinctrl-0");
	if (offset < 0)
		return offset;

	drive = fdt_getprop_u32_default_node(fdt_blob, offset, 0,
					     "drive-strength", 0);
	if (drive) {
		if (drive <= 10)
			drive = SUN4I_PINCTRL_10_MA;
		else if (drive <= 20)
			drive = SUN4I_PINCTRL_20_MA;
		else if (drive <= 30)
			drive = SUN4I_PINCTRL_30_MA;
		else
			drive = SUN4I_PINCTRL_40_MA;
	} else {
		drive = fdt_getprop_u32_default_node(fdt_blob, offset, 0,
						     "allwinner,drive", 4);
	}

	if (fdt_get_property(fdt_blob, offset, "bias-pull-up", NULL))
		pull = SUN4I_PINCTRL_PULL_UP;
	else if (fdt_get_property(fdt_blob, offset, "bias-disable", NULL))
		pull = SUN4I_PINCTRL_NO_PULL;
	else if (fdt_get_property(fdt_blob, offset, "bias-pull-down", NULL))
		pull = SUN4I_PINCTRL_PULL_DOWN;
	else
		pull = fdt_getprop_u32_default_node(fdt_blob, offset, 0,
						    "allwinner,pull", 0);

	for (i = 0; ; i++) {
		pin_name = fdt_stringlist_get(fdt_blob, offset,
					      "allwinner,pins", i, NULL);
		if (!pin_name) {
			pin_name = fdt_stringlist_get(fdt_blob, offset,
						      "pins", i, NULL);
			if (!pin_name)
				break;
		}
		pin = sunxi_gpio_parse_pin_name(pin_name);
		if (pin < 0)
			continue;

		sunxi_gpio_set_cfgpin(pin, mux_sel);
		sunxi_gpio_set_drv(pin, drive);
		sunxi_gpio_set_pull(pin, pull);
	}

	return i;
}
