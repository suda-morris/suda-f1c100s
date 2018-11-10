// SPDX-License-Identifier: GPL-2.0
/*
 * Device Tree support for Allwinner F series SoCs
 *
 * Copyright (C) 2017 Icenowy Zheng <icenowy@aosc.io>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <asm/mach/arch.h>

static const char * const suniv_board_dt_compat[] = {
	"allwinner,suniv",
	"allwinner,suniv-f1c100s",
	NULL,
};

DT_MACHINE_START(SUNXI_DT, "Allwinner suniv Family")
	.dt_compat	= suniv_board_dt_compat,
MACHINE_END
