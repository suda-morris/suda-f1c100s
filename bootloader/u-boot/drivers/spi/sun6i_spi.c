/*
 * SPI driver for Allwinner sunxi SoCs
 *
 * Copyright (C) 2015-2017 Theobroma Systems Design und Consulting GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <common.h>
#ifdef CONFIG_DM_GPIO
#include <asm/gpio.h>
#endif
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <wait_bit.h>
#include <reset.h>
#include <spi.h>

DECLARE_GLOBAL_DATA_PTR;

struct sunxi_spi_platdata {
	void *base;
	unsigned int max_hz;

	/* We could do with a single delay counter, but it won't do harm
	   to have two, as the same is the case for most other driver. */
	uint deactivate_delay_us;	/* Delay to wait after deactivate */
	uint activate_delay_us;		/* Delay to wait after activate */

#if defined(CONFIG_DM_GPIO)
	int cs_gpios_num;
	struct gpio_desc *cs_gpios;
#endif
};

struct sunxi_spi_driverdata {
	unsigned int  fifo_depth;
};

struct sunxi_spi_privdata {
	ulong last_transaction_us;	/* Time of last transaction end */
	unsigned int hz_requested;      /* last requested bitrate */
	unsigned int hz_actual;         /* currently set bitrate */
};

struct sunxi_spi_reg {
	u8	_rsvd[0x4];
	u32	GCR;   /* SPI Global Control register */
	u32	TCR;   /* SPI Transfer Control register */
	u8	_rsvd1[0x4];
	u32	IER;   /* SPI Interrupt Control register */
	u32	ISR;   /* SPI Interrupt Status register */
	u32	FCR;   /* SPI FIFO Control register */
	u32	FSR;   /* SPI FIFO Status register */
	u32	WCR;   /* SPI Wait Clock Counter register */
	u32	CCR;   /* SPI Clock Rate Control register */
	u8	_rsvd2[0x8];
	u32	MBC;   /* SPI Burst Counter register */
	u32	MTC;   /* SPI Transmit Counter register */
	u32	BCC;   /* SPI Burst Control register */
	u8      _rsvd3[0x4c];
	u32     NDMA_MODE_CTL;
	u8	_rsvd4[0x174];
	u32	TXD;   /* SPI TX Data register */
	u8	_rsvd5[0xfc];
	u32	RXD;   /* SPI RX Data register */
};


#define GCR_MASTER	 BIT(1)
#define GCR_EN		 BIT(0)

#define TCR_XCH          BIT(31)
#define TCR_SDC          BIT(11)
#define TCR_DHB          BIT(8)
#define TCR_SSSEL_SHIFT  (4)
#define TCR_SSSEL_MASK   (0x3 << TCR_SSSEL_SHIFT)
#define TCR_SSLEVEL      BIT(7)
#define TCR_SSOWNER      BIT(6)
#define TCR_CPOL         BIT(1)
#define TCR_CPHA         BIT(0)

#define FCR_RX_FIFO_RST  BIT(31)
#define FCR_TX_FIFO_RST  BIT(15)

#define BCC_STC_MASK     (0x00FFFFFF)

#define CCTL_SEL_CDR1    0
#define CCTL_SEL_CDR2    BIT(12)
#define CDR1(n)          ((n & 0xf) << 8)
#define CDR2(n)          (((n/2) - 1) & 0xff)

static int sunxi_spi_cs_activate(struct udevice *dev, unsigned cs)
{
	struct udevice *bus = dev->parent;
	struct sunxi_spi_platdata *plat = dev_get_platdata(bus);
	struct sunxi_spi_reg *spi = (struct sunxi_spi_reg *)plat->base;
	struct sunxi_spi_privdata *priv = dev_get_priv(bus);
	int ret = 0;

	debug("%s (%s): cs %d cs_gpios_num %d cs_gpios %p\n",
	      dev->name, __func__, cs, plat->cs_gpios_num, plat->cs_gpios);

	/* If it's too soon to do another transaction, wait... */
	if (plat->deactivate_delay_us && priv->last_transaction_us) {
		ulong delay_us;
		delay_us = timer_get_us() - priv->last_transaction_us;
		if (delay_us < plat->deactivate_delay_us)
			udelay(plat->deactivate_delay_us - delay_us);
	}

#if defined(CONFIG_DM_GPIO)
	/* Use GPIOs as chip selects? */
	if (plat->cs_gpios) {
		/* Guard against out-of-bounds accesses */
		if (!(cs < plat->cs_gpios_num))
			return -ENOENT;

		if (dm_gpio_is_valid(&plat->cs_gpios[cs])) {
			ret = dm_gpio_set_value(&plat->cs_gpios[cs], 1);
			goto done;
		}
	}
#endif
	/* The hardware can control up to 4 CS, however not all of
	   them will be going to pads. We don't try to second-guess
	   the DT or higher-level drivers though and just test against
	   the hard limit. */

	if (!(cs < 4))
		return -ENOENT;

	/* Control the positional CS output */
	clrsetbits_le32(&spi->TCR, TCR_SSSEL_MASK, cs << TCR_SSSEL_SHIFT);
	clrsetbits_le32(&spi->TCR, TCR_SSLEVEL, TCR_SSOWNER);

done:
	/* We'll delay, even it this is an error return... */
	if (plat->activate_delay_us)
		udelay(plat->activate_delay_us);

	return ret;
}

static void sunxi_spi_cs_deactivate(struct udevice *dev, unsigned cs)
{
	struct udevice *bus = dev->parent;
	struct sunxi_spi_platdata *plat = dev_get_platdata(bus);
	struct sunxi_spi_reg *spi = (struct sunxi_spi_reg *)plat->base;
	struct sunxi_spi_privdata *priv = dev_get_priv(bus);

#if defined(CONFIG_DM_GPIO)
	/* Use GPIOs as chip selects? */
	if (plat->cs_gpios) {
		if (dm_gpio_is_valid(&plat->cs_gpios[cs])) {
			dm_gpio_set_value(&plat->cs_gpios[cs], 0);
			return;
		}
	}
#endif

	/* We have only the hardware chip select, so use those */
	setbits_le32(&spi->TCR, TCR_SSLEVEL | TCR_SSOWNER);

	/* Remember time of this transaction for the next delay */
	if (plat->deactivate_delay_us)
		priv->last_transaction_us = timer_get_us();
}

static inline uint8_t *spi_fill_writefifo(struct sunxi_spi_reg *spi,
					  uint8_t *dout, int cnt)
{
	debug("%s: dout = %p, cnt = %d\n", __func__, dout, cnt);

	if (dout) {
		int i;

		for (i = 0; i < cnt; i++)
			writeb(dout[i], &spi->TXD);

		dout += cnt;
	}

	return dout;
}

static inline uint8_t *spi_drain_readfifo(struct sunxi_spi_reg *spi,
					  uint8_t *din, int cnt)
{
	debug("%s: din = %p, cnt = %d\n", __func__, din, cnt);

	if (din) {
		int i;

		for (i = 0; i < cnt; i++)
			din[i] = readb(&spi->RXD);

		din += cnt;
	}

	return din;
}

static int sunxi_spi_xfer(struct udevice *dev, unsigned int bitlen,
			  const void *out, void *in, unsigned long flags)
{
	struct udevice *bus = dev->parent;
	struct sunxi_spi_platdata *plat = dev_get_platdata(bus);
	struct sunxi_spi_privdata *priv = dev_get_priv(bus);
	struct sunxi_spi_reg *spi = (struct sunxi_spi_reg *)plat->base;
	struct sunxi_spi_driverdata *data =
		(struct sunxi_spi_driverdata *)dev_get_driver_data(dev->parent);
	struct dm_spi_slave_platdata *slave = dev_get_parent_platdata(dev);
	uint8_t *dout = (uint8_t *)out;
	uint8_t *din = (uint8_t *)in;
	int fifo_depth = data->fifo_depth;
	unsigned int n_bytes = DIV_ROUND_UP(bitlen, 8);
	int ret = 0;
	/*
	 * We assume that 1ms (for any delays within the module to
	 * start the transfer) + 2x the time to transfer a full FIFO
	 * (for the data- and bitrate-dependent part) is a reasonable
	 * timeout to detect the module being stuck.
	 */
	ulong timeout_ms =
		(DIV_ROUND_UP(fifo_depth * 16000, priv->hz_actual)) + 1;

	debug("%s (%s): regs %p bitlen %d din %p flags %lx fifo_depth %d\n",
	      dev->name, __func__, spi, bitlen, din, flags, fifo_depth);

	if (flags & SPI_XFER_BEGIN) {
		ret = sunxi_spi_cs_activate(dev, slave->cs);
		if (ret < 0) {
			printf("%s: failed to activate chip-select %d\n",
			       dev->name, slave->cs);
			return ret;
		}
	}

	/* Reset FIFO */
	writel(FCR_RX_FIFO_RST | FCR_TX_FIFO_RST, &spi->FCR);
	/* Wait until the FIFO reset autoclears */
	ret = wait_for_bit(dev->name, &spi->FCR,
			   FCR_RX_FIFO_RST | FCR_TX_FIFO_RST,
			   false, 10, true);
	if (ret < 0) {
		printf("%s: failed to reset FIFO within 10ms\n", bus->name);
		return ret;
	}

	/* Set the discard burst bits depending on whether we are receiving */
	clrbits_le32(&spi->TCR, TCR_DHB);
	if (!din)
		setbits_le32(&spi->TCR, TCR_DHB);

	/* Transfer in blocks of FIFO_DEPTH */
	while (n_bytes > 0) {
		int cnt = (n_bytes < fifo_depth) ? n_bytes : fifo_depth;
		int txcnt = dout ? cnt : 0;

		/* We need to set up the transfer counters in every
		   iteration, as the hardware block counts those down
		   to 0 and leaves the 0 in the register (i.e. there's
		   no shadow register within the controller that these
		   values are copied into). */

		/* master burst counter:     total length (tx + rx + dummy) */
		writel(cnt, &spi->MBC);
		/* master transmit counter:  tx */
		writel(txcnt, &spi->MTC);
		/* burst control counter:    single-mode tx */
		clrsetbits_le32(&spi->BCC, BCC_STC_MASK, txcnt & BCC_STC_MASK);

		dout = spi_fill_writefifo(spi, dout, txcnt);

		/* Start transfer ... */
		setbits_le32(&spi->TCR, TCR_XCH);
		/* ... and wait until it finshes. */
		ret = wait_for_bit(dev->name, &spi->TCR, TCR_XCH,
				   false, timeout_ms, true);
		if (ret < 0) {
			printf("%s: stuck in XCH for %ld ms\n",
			       bus->name, timeout_ms);
			goto fail;
		}

		din = spi_drain_readfifo(spi, din, cnt);

		n_bytes -= cnt;
	}

 fail:
	if (flags & SPI_XFER_END)
		sunxi_spi_cs_deactivate(dev, slave->cs);

	return 0;
};

static int sunxi_spi_ofdata_to_platdata(struct udevice *dev)
{
	struct sunxi_spi_platdata *plat = dev_get_platdata(dev);
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(dev);
	fdt_addr_t addr;
	fdt_size_t size;

	debug("%s: %p\n", __func__, dev);

	addr = fdtdec_get_addr_size_auto_noparent(blob, node, "reg", 0,
						  &size, false);
	if (addr == FDT_ADDR_T_NONE) {
		debug("%s: failed to find base address\n", dev->name);
		return -ENODEV;
	}
	plat->base = (void *)addr;
	plat->max_hz = fdtdec_get_int(blob, node, "spi-max-frequency", 0);
	plat->activate_delay_us = fdtdec_get_int(blob, node,
						 "spi-activate_delay", 0);
	plat->deactivate_delay_us = fdtdec_get_int(blob, node,
						   "spi-deactivate-delay", 0);

#if defined(CONFIG_DM_GPIO)
	plat->cs_gpios_num = gpio_get_list_count(dev, "cs-gpios");
	if (plat->cs_gpios_num > 0) {
		int i;

		plat->cs_gpios = calloc(plat->cs_gpios_num,
					sizeof(struct gpio_desc));
		if (!plat->cs_gpios)
			return -ENOMEM;

		for (i = 0; i < plat->cs_gpios_num; ++i)
			gpio_request_by_name(dev, "cs-gpios", i,
					     &plat->cs_gpios[i], 0);
	}
#endif
	return 0;
}

static int sunxi_spi_probe(struct udevice *dev)
{
	unsigned int pin_func = SUNXI_GPC_SPI0;
	int ret;

	if (IS_ENABLED(CONFIG_MACH_SUN50I))
		pin_func = SUN50I_GPC_SPI0;

	if (IS_ENABLED(CONFIG_MACH_SUNIV))
		pin_func = SUNIV_GPC_SPI0;

	ret = sunxi_gpio_setup_dt_pins(gd->fdt_blob, dev_of_offset(dev),
				       NULL, pin_func);

	if (ret < 0) {
		printf("SPI: pinctrl node not found.\n");
	} else if (!ret) {
		printf("SPI: pinctrl node not valid.\n");
	}

	return (ret <= 0) ? -EINVAL : 0;
}

static int sunxi_spi_claim_bus(struct udevice *dev)
{
	struct sunxi_spi_platdata *plat = dev_get_platdata(dev->parent);
	struct spi_slave *spi_slave = dev_get_parent_priv(dev);
	struct sunxi_spi_reg *spi = (struct sunxi_spi_reg *)plat->base;

	debug("%s: %p %p\n", __func__, dev, dev->parent);

	/* Enable in master-mode */
	setbits_le32(&spi->GCR, GCR_MASTER | GCR_EN);
	/* All CS control is manual and set them to inactive */
	clrbits_le32(&spi->TCR, TCR_SSSEL_MASK);
	setbits_le32(&spi->TCR, TCR_SSOWNER);
	/* Apply polarity and phase from the mode bits */
	if (spi_slave->mode & SPI_CPOL)
		setbits_le32(&spi->TCR, TCR_CPOL);
	if (spi_slave->mode & SPI_CPHA)
		setbits_le32(&spi->TCR, TCR_CPHA);

#if defined(DM_GPIO)
	/* Set all cs-gpios to inactive */
	for (i = 0; i < plat->cs_gpios_num; ++i)
		if (dm_gpio_is_valid(&plat->cs_gpios[i]))
			dm_gpio_set_value(&plat->cs_gpios[i], 0);
#endif


	return 0;
}

static int sunxi_spi_release_bus(struct udevice *dev)
{
	struct sunxi_spi_platdata *plat = dev_get_platdata(dev->parent);
	struct sunxi_spi_reg *spi = (struct sunxi_spi_reg *)plat->base;

	clrbits_le32(&spi->GCR, GCR_EN);

	return 0;
}

static int sunxi_spi_set_speed(struct udevice *bus, unsigned int hz)
{
	struct sunxi_ccm_reg *ccm = (struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	struct sunxi_spi_platdata *plat = dev_get_platdata(bus);
	struct sunxi_spi_reg *spi = (struct sunxi_spi_reg *)plat->base;
	struct sunxi_spi_privdata *priv = dev_get_priv(bus);
	unsigned sclk_shift, hz_ahb, hz_sclk, target_freq;

	debug("%s: %p, %d\n", __func__, bus, hz);

	if (plat->max_hz && (hz > plat->max_hz)) {
		debug("%s: selected speed (%d) exceeds maximum of %d\n",
		      bus->name, hz, plat->max_hz);
		hz = plat->max_hz;
	}

	/* If the last request was for the same speed, we're done */
	if (priv->hz_requested == hz)
		return 0;

	/* The CCU section in the manual recommends to have the module
	   reset deasserted before the module clock gate is opened. */
	setbits_le32(&ccm->ahb_reset0_cfg, 1 << AHB_RESET_OFFSET_SPI0);


	/* Enable and set the module clock.
	 *
	 * At least for the A31, there's a requirements to provide at
	 * least 2x the sample clock, so we should never go below that
	 * ratio between the AHB clock and the (ampling) SCLK. On the
	 * low end of the clock, we use the provide two step-downs for
	 * clocks on the low end (below 375kHz).
	 *
	 * However, testing shows that for high-speed modes (on the
	 * A64), we may not divide SCLK from the AHB clock.
	 */
#ifndef CONFIG_MACH_SUNIV
	if (hz < 100000)
		sclk_shift = 8;
	else if (hz < 50000000)
		sclk_shift = 2;
	else
		sclk_shift = 0;

	setbits_le32(&ccm->ahb_gate0, 1 << AHB_GATE_OFFSET_SPI0);

	/* Program the SPI clock control */
	writel(CCTL_SEL_CDR1 | CDR1(sclk_shift), &spi->CCR);

	target_freq = hz * (1 << sclk_shift);

	if (target_freq < 24000000) {
		writel(BIT(31) | max(24000000 / target_freq - 1, 0xf),
		       &ccm->spi0_clk_cfg);
		hz_ahb = 24000000 / ((readl(&ccm->spi0_clk_cfg) & 0xf) + 1);
	} else {
		writel(BIT(31) | (0x01 << 24) | max(600000000 / target_freq - 1, 0xf),
		       &ccm->spi0_clk_cfg);
		hz_ahb = 600000000 / ((readl(&ccm->spi0_clk_cfg) & 0xf) + 1);
	}
#else
	hz_ahb = target_freq = 200000000; /* AHB clock on suniv */
	sclk_shift = 0;
	while(target_freq > hz && sclk_shift < 0xf) {
		target_freq >>= 1;
		sclk_shift += 1;
	};

	setbits_le32(&ccm->ahb_gate0, 1 << AHB_GATE_OFFSET_SPI0);

	writel(CCTL_SEL_CDR1 | CDR1(sclk_shift), &spi->CCR);
#endif

	hz_sclk = hz_ahb >> sclk_shift;
	priv->hz_actual = hz_sclk;
	debug("%s: hz_ahb %d  hz_sclk %d\n", bus->name, hz_ahb, hz_sclk);

	/* If this is a high-speed mode (which we define---based upon
	   empirical testing---to be above 50 MHz), we need to move the
	   sampling point during data read. */
	if (hz_sclk > 50000000)
		setbits_le32(&spi->TCR, TCR_SDC);
	else
		clrbits_le32(&spi->TCR, TCR_SDC);

	return 0;
};

static int sunxi_spi_set_mode(struct udevice *bus, unsigned int mode)
{
	return 0;
};

static const struct dm_spi_ops sunxi_spi_ops = {
	.claim_bus	= sunxi_spi_claim_bus,
	.release_bus	= sunxi_spi_release_bus,
	.xfer		= sunxi_spi_xfer,
	.set_speed	= sunxi_spi_set_speed,
	.set_mode	= sunxi_spi_set_mode,
	/*
	 * cs_info is not needed, since we require all chip selects to be
	 * in the device tree explicitly
	 */
};

static struct sunxi_spi_driverdata  sun6i_a31_data = {
	.fifo_depth = 128,
};

static struct sunxi_spi_driverdata  sun50i_a64_data = {
	.fifo_depth = 64,
};

static const struct udevice_id sunxi_spi_ids[] = {
	{ .compatible = "allwinner,sun6i-a31-spi",
	  .data = (uintptr_t)&sun6i_a31_data },
	{ .compatible = "allwinner,sun8i-h3-spi",
	  .data = (uintptr_t)&sun50i_a64_data },
	{ }
};

U_BOOT_DRIVER(sunxi_spi) = {
	.name = "sunxi_spi",
	.id = UCLASS_SPI,
	.of_match = sunxi_spi_ids,
	.ofdata_to_platdata = sunxi_spi_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct sunxi_spi_platdata),
	.priv_auto_alloc_size = sizeof(struct sunxi_spi_privdata),
	.probe = sunxi_spi_probe,
	.ops = &sunxi_spi_ops,
};
