/* linux/arch/arm/mach-s5pv210/mach-mid.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/sysdev.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pwm_backlight.h>
#include <linux/usb/ch9.h>
#include <linux/gpio_keys.h>
#include <linux/console.h>
#include <linux/mtd/partitions.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-fb.h>
#include <mach/adc.h>
#include <mach/ts-s3c.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/gpio-smdkc110.h>
#include <mach/gpio-mid.h>
#include <mach/mid-cfg.h>

#include <plat/regs-serial.h>
#include <plat/regs-srom.h>
#include <plat/gpio-cfg.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/ts.h>
#include <plat/ata.h>
#include <plat/iic.h>
#include <plat/pm.h>
#include <plat/fb.h>
#include <plat/s5p-time.h>
#include <plat/sdhci.h>
#include <plat/fimc.h>
#include <plat/jpeg.h>
#include <plat/mfc.h>
#include <plat/clock.h>
#include <plat/regs-otg.h>
#include <plat/nand.h>
#include <plat/ehci.h>
#include <plat/usb-phy.h>

#include <../../../drivers/video/samsung/s3cfb.h>
#include <media/midcam_platform.h>
#include <mtd/mtd-abi.h>
#include <video/platform_lcd.h>

#ifdef CONFIG_MFD_MAX8998_URBETTER
#include <linux/regulator/consumer.h>
#include <linux/mfd/max8998-urbetter.h>
#endif

#ifdef CONFIG_DM9000
#include <linux/dm9000.h>
#endif

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <plat/media.h>
#include <mach/media.h>

#ifdef CONFIG_S5PV210_POWER_DOMAIN
#include <mach/power-domain.h>
#endif

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDKV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDKV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDKV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg mid_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDKV210_UCON_DEFAULT,
		.ulcon		= SMDKV210_ULCON_DEFAULT,
		.ufcon		= SMDKV210_UFCON_DEFAULT,
	},
};

#ifdef CONFIG_DM9000
static struct resource mid_dm9000_resources[] = {
	[0] = {
		.start	= S5PV210_PA_SROM_BANK5,
		.end	= S5PV210_PA_SROM_BANK5,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= S5PV210_PA_SROM_BANK5 + 2,
		.end	= S5PV210_PA_SROM_BANK5 + 2,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_EINT(9),
		.end	= IRQ_EINT(9),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct dm9000_plat_data mid_dm9000_platdata = {
	.flags		= DM9000_PLATF_16BITONLY | DM9000_PLATF_NO_EEPROM,
	.dev_addr	= { 0x00, 0x09, 0xc0, 0xff, 0xec, 0x48 },
};

struct platform_device mid_dm9000 = {
	.name			= "dm9000",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(mid_dm9000_resources),
	.resource		= mid_dm9000_resources,
	.dev			= {
		.platform_data = &mid_dm9000_platdata,
	},
};
#endif

#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (9900 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD (3072 * SZ_1K * \
                                             (CONFIG_FB_S3C_NR_BUFFERS + \
                                                 (CONFIG_FB_S3C_NUM_OVLY_WIN * \
                                                  CONFIG_FB_S3C_NUM_BUF_OVLY_WIN)))
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (8192 * SZ_1K)

/* 1920 * 1080 * 4 (RGBA)
 * - framesize == 1080p : 1920 * 1080 * 2(16bpp) * 2(double buffer) = 8MB
 * - framesize <  1080p : 1080 *  720 * 4(32bpp) * 2(double buffer) = under 8MB
 **/
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_G2D (8192 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXSTREAM (3000 * SZ_1K)
#define  S5PV210_ANDROID_PMEM_MEMSIZE_PMEM_GPU1 (3300 * SZ_1K)

static struct s5p_media_device s5pv210_media_devs[] = {
        [0] = {
                .id = S5P_MDEV_MFC,
                .name = "mfc",
                .bank = 0,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0,
                .paddr = 0,
        },
        [1] = {
                .id = S5P_MDEV_MFC,
                .name = "mfc",
                .bank = 1,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1,
                .paddr = 0,
        },
        [2] = {
                .id = S5P_MDEV_FIMC0,
                .name = "fimc0",
                .bank = 1,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0,
                .paddr = 0,
        },
        [3] = {
                .id = S5P_MDEV_FIMC1,
                .name = "fimc1",
                .bank = 1,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1,
                .paddr = 0,
        },
        [4] = {
                .id = S5P_MDEV_FIMC2,
                .name = "fimc2",
                .bank = 1,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2,
                .paddr = 0,
        },
        [5] = {
                .id = S5P_MDEV_JPEG,
                .name = "jpeg",
                .bank = 0,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG,
                .paddr = 0,
        },
		[6] = {
                .id = S5P_MDEV_FIMD,
                .name = "fimd",
                .bank = 1,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD,
                .paddr = 0,
        },
        [7] = {
				.id = S5P_MDEV_TEXSTREAM,
				.name = "texstream",
				.bank = 1,
				.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXSTREAM,
				.paddr = 0,
		},
		[8] = {
				.id = S5P_MDEV_PMEM_GPU1,
				.name = "pmem_gpu1",
				.bank = 0, /* OneDRAM */
				.memsize = S5PV210_ANDROID_PMEM_MEMSIZE_PMEM_GPU1,
				.paddr = 0,
		},
		[9] = {
				.id = S5P_MDEV_G2D,
				.name = "g2d",
				.bank = 0,
				.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_G2D,
				.paddr = 0,
		},
};

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
        .name = "pmem",
        .no_allocator = 1,
        .cached = 1,
        .start = 0,
        .size = 0,
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
        .name = "pmem_gpu1",
        .no_allocator = 1,
        .cached = 1,
        .buffered = 1,
        .start = 0,
        .size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
        .name = "pmem_adsp",
        .no_allocator = 1,
        .cached = 1,
        .buffered = 1,
        .start = 0,
        .size = 0,
};

static struct platform_device pmem_device = {
        .name = "android_pmem",
        .id = 0,
        .dev = { .platform_data = &pmem_pdata },
};

static struct platform_device pmem_gpu1_device = {
        .name = "android_pmem",
        .id = 1,
        .dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
        .name = "android_pmem",
        .id = 2,
        .dev = { .platform_data = &pmem_adsp_pdata },
};

static void __init android_pmem_set_platdata(void)
{
        pmem_pdata.start = (u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM, 0);
        pmem_pdata.size = (u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM, 0);

        pmem_gpu1_pdata.start =
                (u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_GPU1, 0);
        pmem_gpu1_pdata.size =
                (u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_GPU1, 0);

        pmem_adsp_pdata.start =
                (u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_ADSP, 0);
        pmem_adsp_pdata.size =
                (u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_ADSP, 0);
}
#endif

#ifdef CONFIG_MFD_MAX8998_URBETTER
static struct regulator_consumer_supply ldo3_consumer[] = {
	REGULATOR_SUPPLY("pd_io", "s3c-usbgadget")
};

static struct regulator_consumer_supply ldo4_consumer[] = {
	{   .supply = "vddmipi", },
};

static struct regulator_consumer_supply ldo6_consumer[] = {
	{   .supply = "vddlcd", },
};

static struct regulator_consumer_supply ldo7_consumer[] = {
	{   .supply = "vddmodem", },
};

static struct regulator_consumer_supply ldo8_consumer[] = {
	REGULATOR_SUPPLY("pd_core", "s3c-usbgadget")
};

static struct regulator_consumer_supply buck1_consumer[] = {
	{   .supply = "vddarm", },
};

static struct regulator_consumer_supply buck2_consumer[] = {
	{   .supply = "vddint", },
};

static struct regulator_init_data mid_ldo2_data = {
	.constraints	= {
		.name		= "VALIVE_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data mid_ldo3_data = {
	.constraints	= {
		.name		= "VUOTG_D_1.1V/VUHOST_D_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo3_consumer),
	.consumer_supplies	= ldo3_consumer,
};

static struct regulator_init_data mid_ldo4_data = {
	.constraints	= {
		.name		= "V_MIPI_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo4_consumer),
	.consumer_supplies	= ldo4_consumer,
};

static struct regulator_init_data mid_ldo5_data = {
	.constraints	= {
		.name		= "VMMC_2.8V/VEXT_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data mid_ldo6_data = {
	.constraints	= {
		.name		= "VCC_2.6V",
		.min_uV		= 2600000,
		.max_uV		= 2600000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	 = {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo6_consumer),
	.consumer_supplies	= ldo6_consumer,
};

static struct regulator_init_data mid_ldo7_data = {
	.constraints	= {
		.name		= "VDAC_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies  = ARRAY_SIZE(ldo7_consumer),
	.consumer_supplies  = ldo7_consumer,
};

static struct regulator_init_data mid_ldo8_data = {
	.constraints	= {
		.name		= "VUOTG_A_3.3V/VUHOST_A_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo8_consumer),
	.consumer_supplies	= ldo8_consumer,
};

static struct regulator_init_data mid_ldo9_data = {
	.constraints	= {
		.name		= "{VADC/VSYS/VKEY}_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data mid_buck1_data = {
	.constraints	= {
		.name		= "VCC_ARM",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1250000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck1_consumer),
	.consumer_supplies	= buck1_consumer,
};

static struct regulator_init_data mid_buck2_data = {
	.constraints	= {
		.name		= "VCC_INTERNAL",
		.min_uV		= 950000,
		.max_uV		= 1200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies  = ARRAY_SIZE(buck2_consumer),
	.consumer_supplies  = buck2_consumer,
};

static struct regulator_init_data mid_buck3_data = {
	.constraints	= {
		.name		= "VCC_MEM",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.always_on	= 1,
		.apply_uV	= 1,
		.state_mem	= {
			.uV	= 1800000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 1,
		},
	},
};

static struct max8998_regulator_data mid_regulators[] = {
	{ MAX8998_LDO2,  &mid_ldo2_data },
	{ MAX8998_LDO3,  &mid_ldo3_data },
	{ MAX8998_LDO4,  &mid_ldo4_data },
	{ MAX8998_LDO5,  &mid_ldo5_data },
	{ MAX8998_LDO6,  &mid_ldo6_data },
	{ MAX8998_LDO7,  &mid_ldo7_data },
	{ MAX8998_LDO8,  &mid_ldo8_data },
	{ MAX8998_LDO9,  &mid_ldo9_data },
	{ MAX8998_BUCK1, &mid_buck1_data },
	{ MAX8998_BUCK2, &mid_buck2_data },
	{ MAX8998_BUCK3, &mid_buck3_data },
};

static struct max8998_platform_data max8998_pdata = {
	.num_regulators	= ARRAY_SIZE(mid_regulators),
	.regulators	= mid_regulators,
	.charger	= NULL,
};
#endif

static struct s3cfb_lcd lcd_ut7gm = {
	.width = 800,
	.height = 480,
	.p_width = 152,
	.p_height = 91,
	.bpp = 32,
	.freq = 60,

	.timing = {
		.h_fp = 10,
		.h_bp = 78,
		.h_sw = 10,
		.v_fp = 30,
		.v_fpe = 1,
		.v_bp = 30,
		.v_bpe = 1,
		.v_sw = 2,
	},
	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};

static struct s3cfb_lcd lcd_ut08gm = {
	.width = 800,
	.height = 600,
	.p_width = 163,
	.p_height = 122,
	.bpp = 32,
	.freq = 60,

	.timing = {
		.h_fp = 50,
		.h_bp = 150,
		.h_sw = 60,
		.v_fp = 30,
		.v_fpe = 1,
		.v_bp = 30,
		.v_bpe = 1,
		.v_sw = 2,
	},
	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};

static struct s3cfb_lcd lcd_ut10gm = {
	.width = 1024,
	.height = 600,
	.p_width = 221,
	.p_height = 130,
	.bpp = 32,
	.freq = 60,

	.timing = {
		.h_fp = 24,
		.h_bp = 160,
		.h_sw = 136,
		.v_fp = 0,
		.v_fpe = 1,
		.v_bp = 22,
		.v_bpe = 1,
		.v_sw = 3,
	},
	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};

static void mid_lcd_onoff(unsigned int onoff) {
    int err;

    unsigned int nGPIOs[] = {GPIO_LCD_BACKLIGHT_POWER, GPIO_LCD_BACKLIGHT_POWERB};

	if ((err = gpio_request(nGPIOs[0], "lcd-backlight-en"))) {
		printk(KERN_ERR "failed to request gpio for lcd control: %d\n", err);
		return;
	}

	if ((err = gpio_request(nGPIOs[1], "lcd-backlight-en"))) {
		printk(KERN_ERR "failed to request gpio for lcd control: %d\n", err);
        gpio_free(nGPIOs[0]);
		return;
	}

    printk("%s: %d\n", "mid_lcd_onoff", onoff);

    if (onoff) {
        gpio_direction_output(nGPIOs[0], 1);
        gpio_direction_output(nGPIOs[1], 0);
    } else {
        gpio_direction_output(nGPIOs[0], 0);
        gpio_direction_output(nGPIOs[1], 1);
    }

	mdelay(20);
    gpio_free(nGPIOs[1]);
    gpio_free(nGPIOs[0]);
}

static void mid_backlight_onoff(int onoff) {
    int err;

    unsigned int nGPIO = GPIO_BACKLIGHT;
	err = gpio_request(nGPIO, "backlight-en");

	if (err) {
		printk(KERN_ERR "failed to request gpio for backlight control: %d\n", err);
		return;
	}

    printk("%s: %d\n", "mid_backlight_onoff", onoff);

    if (onoff)
        gpio_direction_output(nGPIO, 1);
    else
        gpio_direction_output(nGPIO, 0);

	mdelay(10);
    gpio_free(nGPIO);
}

static void mid_lcd_cfg_gpio(struct platform_device *pdev) {
    int i;

    for (i = 0; i < 8; i++) {
        s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
        s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
    }

    for (i = 0; i < 8; i++) {
        s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
        s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
    }

    for (i = 0; i < 8; i++) {
        s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
        s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
    }

    for (i = 0; i < 6; i++) {
        s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
        s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
    }

    /* mDNIe SEL: why we shall write 0x2 ? */
    writel(0x2, S5P_MDNIE_SEL);

    /* drive strength to max */
    writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
    writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
    writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
    writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}

int mid_lcd_backlight_onoff(struct platform_device *pdev, int onoff) {
    if (onoff) {
        mid_lcd_onoff(1);
        mid_backlight_onoff(1);
    } else {
        mid_backlight_onoff(0);
        mid_lcd_onoff(0);
    }

	return 0;
}

int mid_lcd_backlight_on(struct platform_device *pdev) {
    return mid_lcd_backlight_onoff(pdev, 1);
}

static int mid_lcd_reset(struct platform_device *pdev) {
    mid_backlight_onoff(0);
    mid_lcd_onoff(0);
	mdelay(180);
    mid_lcd_onoff(1);
    mid_backlight_onoff(1);

    return 0;
}

static struct s3c_platform_fb mid_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.lcd = &lcd_ut7gm,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.cfg_gpio	        = mid_lcd_cfg_gpio,
	.backlight_on	    = mid_lcd_backlight_on,
	.backlight_onoff    = mid_lcd_backlight_onoff,
	.reset_lcd	        = mid_lcd_reset,
};

static void mid_detect_lcd(void) {
    if (!strcmp(mid_lcd, "lp101")) {
        // Coby 1024
        printk("* Selecting 10.1\" LCD...\n");
        mid_fb_data.lcd = &lcd_ut10gm;
    }
    else if (!strcmp(mid_lcd, "ut08gm")) {
        // Coby 8024
        printk("* Selecting 8.0\" LCD...\n");
        mid_fb_data.lcd = &lcd_ut08gm;
    }
    else if (!strcmp(mid_lcd, "ut7gm")) {
        // Herotab C8/Dropad A8/Haipad M7/iBall Slide/Coby 7024
        printk("* Selecting 7.0\" LCD...\n");
        mid_fb_data.lcd = &lcd_ut7gm;
    }
    else {
        // At this point, the kernel will panic in S3CFB initialization code
        // because "mid_fb_data.lcd" is set to NULL.
        printk("\n\n\n\n *** FATAL ERROR: cannot determine LCD ***\n\n\n");
    }
}

#define S5PV210_GPD_0_0_TOUT_0  (0x2)
#define S5PV210_GPD_0_1_TOUT_1  (0x2 << 4)
#define S5PV210_GPD_0_2_TOUT_2  (0x2 << 8)
#define S5PV210_GPD_0_3_TOUT_3  (0x2 << 12)

static struct { int gpio, cfg; } mid_pwm_cfg[] = {
    {
        .gpio = S5PV210_GPD0(0),
        .cfg = S5PV210_GPD_0_0_TOUT_0
    },
    {
        .gpio = S5PV210_GPD0(1),
        .cfg = S5PV210_GPD_0_1_TOUT_1
    },
    {
        .gpio = S5PV210_GPD0(2),
        .cfg = S5PV210_GPD_0_2_TOUT_2
    },
    {
        .gpio = S5PV210_GPD0(3),
        .cfg = S5PV210_GPD_0_3_TOUT_3
    }
};

static int mid_backlight_init(struct device *dev) {
	int i, j, ret;

    for (i = 0; i < ARRAY_SIZE(mid_pwm_cfg); i++) {
	    if ((ret = gpio_request(mid_pwm_cfg[i].gpio, "PWM-OUT"))) {
            for (j = i - 1; j >= 0; j--)
                gpio_free(mid_pwm_cfg[j].gpio);

		    printk(KERN_ERR "failed to request gpio for PWM-OUT %d\n", i);
		    return ret;
	    }

	    /* Configure GPIO pin with S5PV210_GPD_0_x_TOUT_x */
        gpio_direction_output(mid_pwm_cfg[i].gpio, 1);
        s3c_gpio_cfgpin(mid_pwm_cfg[i].gpio, mid_pwm_cfg[i].cfg);
    }

	return 0;
}

static void mid_backlight_exit(struct device *dev) {
    int i;

    for (i = 0; ARRAY_SIZE(mid_pwm_cfg); i++) {
	    s3c_gpio_cfgpin(mid_pwm_cfg[i].gpio, S3C_GPIO_OUTPUT);
	    gpio_free(mid_pwm_cfg[i].gpio);
    }
}

static struct platform_pwm_backlight_data mid_backlight_data = {
	.pwm_id			= 0,
	.max_brightness	= 255,
	.dft_brightness	= 127,
	.pwm_period_ns	= 40000,
	.init			= mid_backlight_init,
	.exit			= mid_backlight_exit,
};

static struct platform_device mid_backlight = {
	.name = "pwm-backlight",
	.dev = {
		.parent = &s3c_device_timer[0].dev,
		.platform_data = &mid_backlight_data,
	},
};

#ifdef CONFIG_S5P_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* s5pc110 support 12-bit resolution */
	.delay  = 10000,
	.presc  = 49,
	.resolution = 12,
};
#endif

static struct s3c_ts_mach_info s3c_ts_platform __initdata = {
	.delay                  = 10000,
	.presc                  = 49,
	.resol_bit              = 12,
	.oversampling_shift     = 2,
	.s3c_adc_con            = ADC_TYPE_2,
};

/* Touch srcreen */
static struct resource s3c_ts_resource[] = {
	[0] = {
		.start = SAMSUNG_PA_ADC1,
		.end   = SAMSUNG_PA_ADC1 + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_PENDN1,
		.end   = IRQ_PENDN1,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_ADC1,
		.end   = IRQ_ADC1,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_ts = {
	.name		    = "s3c-ts",
	.id		        = -1,
	.num_resources  = ARRAY_SIZE(s3c_ts_resource),
	.resource	    = s3c_ts_resource,
};

void __init s3c_ts_set_platdata(struct s3c_ts_mach_info *pd)
{
	struct s3c_ts_mach_info *npd;

	npd = kmalloc(sizeof(*npd), GFP_KERNEL);
	if (npd) {
		memcpy(npd, pd, sizeof(*npd));
		s3c_device_ts.dev.platform_data = npd;
	} else {
		pr_err("no memory for Touchscreen platform data\n");
	}
}

static int midcam_power_en(int onoff) {
    int err;
    unsigned int nGPIOs[] = {
        GPIO_CAMERA_POWERB,
        GPIO_CAMERA_POWER,
        GPIO_CAMERA_RESET
    };

    if ((err = gpio_request(nGPIOs[0], "camera-reset-hi253"))) {
        printk(KERN_ERR "Failed to request GPHJ4[4] for camera reset (hi253)!\n");
        return err;
    }

    if ((err = gpio_request(nGPIOs[1], "camera-en"))) {
        printk(KERN_ERR "Failed to request GPH2[2] for camera power!\n");
        gpio_free(nGPIOs[0]);
        return err;
    }

    if ((err = gpio_request(nGPIOs[2], "camera-reset"))) {
        printk(KERN_ERR "Failed to request GPE1[4] for camera reset!\n");
        gpio_free(nGPIOs[1]);
        gpio_free(nGPIOs[0]);
        return err;
    }

    gpio_direction_output(nGPIOs[0], 0);
    gpio_direction_output(nGPIOs[1], (onoff ? 0 : 1));

    gpio_direction_output(nGPIOs[2], 1);
    msleep(50);
    gpio_direction_output(nGPIOs[2], 0);
    msleep(50);
    gpio_direction_output(nGPIOs[2], 1);
    msleep(50);

    gpio_free(nGPIOs[2]);
    gpio_free(nGPIOs[1]);
    gpio_free(nGPIOs[0]);

    return 0;
}

static struct midcam_platform_data cam_hm2055_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct midcam_platform_data cam_gc0308_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct midcam_platform_data cam_hi704_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info cam_hm2055_i2c_info = {
	I2C_BOARD_INFO("hm2055", (0x48 >> 1)),
	.platform_data = &cam_hm2055_plat,
};

static struct i2c_board_info cam_gc0308_i2c_info = {
	I2C_BOARD_INFO("gc0308", (0x42 >> 1)),
	.platform_data = &cam_gc0308_plat,
};

static struct i2c_board_info cam_hi704_i2c_info = {
	I2C_BOARD_INFO("hi704", (0x60 >> 1)),
	.platform_data = &cam_hi704_plat,
};

static struct s3c_platform_camera cam_hm2055 = {
	.id				= CAMERA_PAR_A,
	.type			= CAM_TYPE_ITU,
	.fmt			= ITU_601_YCBCR422_8BIT,
	.order422		= CAM_ORDER422_8BIT_CBYCRY,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.i2c_busnum		= 1,
	.info			= &cam_hm2055_i2c_info,
	.srclk_name		= "mout_mpll",
	.clk_name		= "sclk_cam",
	.clk_rate		= 24000000,
	.line_length	= 1600,
	.width			= 640,
	.height			= 480,
	.window	= {
		.left		= 0,
		.top		= 0,
		.width		= 640,
		.height		= 480,
	},

	.inv_pclk		= 0,
	.inv_vsync		= 1,
	.inv_href		= 0,
	.inv_hsync		= 0,

	.initialized	= 0,
	.cam_power		= midcam_power_en,
};

static struct s3c_platform_camera cam_gc0308 = {
	.id				= CAMERA_PAR_A,
	.type			= CAM_TYPE_ITU,
	.fmt			= ITU_601_YCBCR422_8BIT,
	.order422		= CAM_ORDER422_8BIT_CBYCRY,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.i2c_busnum		= 1,
	.info			= &cam_gc0308_i2c_info,
	.srclk_name		= "mout_mpll",
	.clk_name		= "sclk_cam",
	.clk_rate		= 24000000,
	.line_length	= 640,
	.width			= 640,
	.height			= 480,
	.window	= {
		.left		= 0,
		.top		= 0,
		.width		= 640,
		.height		= 480,
	},

	.inv_pclk		= 0,
	.inv_vsync		= 1,
	.inv_href		= 0,
	.inv_hsync		= 0,

	.initialized	= 0,
	.cam_power		= midcam_power_en,
};

static struct s3c_platform_camera cam_hi704 = {
	.id				= CAMERA_PAR_A,
	.type			= CAM_TYPE_ITU,
	.fmt			= ITU_601_YCBCR422_8BIT,
	.order422		= CAM_ORDER422_8BIT_YCBYCR,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.i2c_busnum		= 1,
	.info			= &cam_hi704_i2c_info,
	.srclk_name		= "mout_mpll",
	.clk_name		= "sclk_cam",
	.clk_rate		= 24000000,
	.line_length	= 640,
	.width			= 640,
	.height			= 480,
	.window	= {
		.left		= 16,
		.top		= 0,
		.width		= 624,
		.height		= 480,
	},

	.inv_pclk		= 0,
	.inv_vsync		= 1,
	.inv_href		= 0,
	.inv_hsync		= 0,

	.initialized	= 0,
	.cam_power		= midcam_power_en,
};

/* Interface setting */
static struct s3c_platform_fimc fimc_plat_lsi = {
	.srclk_name		= "mout_mpll",
	.clk_name		= "sclk_fimc",
	.lclk_name		= "fimc",
	.clk_rate		= 166750000,
	.default_cam	= CAMERA_PAR_A,
	.camera = {
        &cam_hm2055
	},
	.hw_ver			= 0x43,
};

static void mid_detect_camera(void) {
    if (!strcmp(mid_camera, "hm2055")) {
        // Himax HM2055
        printk("* Selecting 2MP camera...\n");
        fimc_plat_lsi.camera[0] = &cam_hm2055;
    }
    else if (!strcmp(mid_camera, "gc0308")) {
        // Galaxycore GC0308
        printk("* Selecting 0.3MP camera...\n");
        fimc_plat_lsi.camera[0] = &cam_gc0308;
    }
    else if (!strcmp(mid_camera, "hi704")) {
        // Hynix HI704
        printk("* Selecting 0.3MP camera...\n");
        fimc_plat_lsi.camera[0] = &cam_hi704;
    }
    else {
        printk("*** WARNING: cannot determine camera; camera will not work ***");
    }
}

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
	.max_main_width		= 800,
	.max_main_height	= 480,
	.max_thumb_width	= 320,
	.max_thumb_height	= 240,
};
#endif

#ifdef CONFIG_DM9000
static void __init mid_dm9000_init(void) {
	unsigned int tmp;

	gpio_request(S5PV210_MP01(5), "nCS5");
	s3c_gpio_cfgpin(S5PV210_MP01(5), S3C_GPIO_SFN(2));
	gpio_free(S5PV210_MP01(5));

	tmp = (5 << S5P_SROM_BCX__TACC__SHIFT);
	__raw_writel(tmp, S5P_SROM_BC5);

	tmp = __raw_readl(S5P_SROM_BW);
	tmp &= (S5P_SROM_BW__CS_MASK << S5P_SROM_BW__NCS5__SHIFT);
	tmp |= (1 << S5P_SROM_BW__NCS5__SHIFT);
	__raw_writel(tmp, S5P_SROM_BW);
}
#endif

static struct platform_device mid_battery = {
    .name = "mid-battery",
    .id = -1,
};

static struct platform_device mid_button = {
    .name = "s3c-button",
    .id = -1,
};

static struct platform_device mid_control = {
    .name = "mid-control",
    .id = -1,
};

static struct i2c_board_info mid_i2c_devs0[] __initdata = {
#ifdef CONFIG_SND_SOC_WM8580
	{
        I2C_BOARD_INFO("wm8580", (0x36>>1)),
    },
#endif
};

static struct i2c_board_info mid_i2c_devs1[] __initdata = {
#ifdef CONFIG_SND_SOC_WM8976
	{
        I2C_BOARD_INFO("wm8976", (0x34>>1)),
    },
#endif
#ifdef CONFIG_ACCEL_MMA7660
	{
		I2C_BOARD_INFO("mma7660", (0x98>>1)),
	},
#endif
};

static struct i2c_board_info mid_i2c_devs2[] __initdata = {
#ifdef CONFIG_MFD_MAX8998_URBETTER
    {
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8998", (0xCC >> 1)),
		.platform_data = &max8998_pdata,
    },
#endif
};

static struct i2c_board_info i2c_ft5x0x_ts __initdata = {
	I2C_BOARD_INFO("ft5x0x_ts", (0x70>>1)),
    .irq = IRQ_EINT(8),
};

static struct i2c_board_info i2c_goodix_ts __initdata = {
	I2C_BOARD_INFO("Goodix-TS", (0xAA>>1)),
    .irq = IRQ_EINT(4),
};

static struct i2c_board_info i2c_ata2538 __initdata = {
	I2C_BOARD_INFO("ata2538", (0xD0>>1))
};

static void mid_detect_i2cdevs(void) {
    int ret;

    if (!strcmp(mid_model, "703")) {
        // Herotab C8/Dropad A8/Haipad M7
        printk("* Selecting capacitive touchscreen...\n");
        if ((ret = i2c_register_board_info(1, &i2c_ft5x0x_ts, 1)) != 0)
            printk("*** ERROR: cannot register capacitive touchscreen (%d); "
                   "touch will not work ***\n", ret);
    }
    else if (!strcmp(mid_model, "712")) {
        // Dropad A8T/Haipad M7S
        printk("* Selecting capacitive touchscreen...\n");
        if ((ret = i2c_register_board_info(2, &i2c_ft5x0x_ts, 1)) != 0)
            printk("*** ERROR: cannot register capacitive touchscreen (%d); "
                   "touch will not work ***\n", ret);
    }
    else if (!strcmp(mid_model, "7024") || !strcmp(mid_model, "8024")) {
        // Coby 7024 and Coby 8024.
        printk("* Selecting resistive touchscreen...\n");
        if ((ret = platform_device_register(&s3c_device_ts)) != 0)
            printk("*** ERROR: cannot register resistive touchscreen (%d); "
                   "touch will not work ***\n", ret);

        printk("* Selecting touch buttons...\n");
        if ((ret = i2c_register_board_info(1, &i2c_ata2538, 1)) != 0)
            printk("*** ERROR: cannot register touch buttons (%d); "
                   "buttons will not work ***\n", ret);

        // Use S3C fake battery driver until conflict with the touchscreen is resolved.
        mid_battery.name = "sec-fake-battery";
    }
    else if (!strcmp(mid_model, "1024")) {
        // Coby 1024
        printk("* Selecting capacitive touchscreen...\n");
        if ((ret = i2c_register_board_info(1, &i2c_goodix_ts, 1)) != 0)
            printk("*** ERROR: cannot register capacitive touchscreen (%d); "
                   "touch will not work ***\n", ret);

        printk("* Selecting touch buttons...\n");
        if ((ret = i2c_register_board_info(1, &i2c_ata2538, 1)) != 0)
            printk("*** ERROR: cannot register touch buttons (%d); "
                   "buttons will not work ***\n", ret);
    }
    else {
        printk("*** WARNING: unknwon model; some devices will may work ***\n");
    }
}

#ifdef CONFIG_S3C_DEV_NAND
struct mtd_partition mid_nand_partitions[] __initdata = {
	{
		.name		= "misc",
		.offset		= (512*SZ_1K),
		.size		= (512*SZ_1K),
		.mask_flags	= MTD_CAP_NANDFLASH,
	},
	{
		.name		= "recovery",
		.offset		= (1*SZ_1M),
		.size		= (8*SZ_1M),
	},
	{
		.name		= "kernel",
		.offset		= (9*SZ_1M),
		.size		= (6*SZ_1M),
	},
	{
		.name		= "logo",
		.offset		= (15*SZ_1M),
		.size		= (4*SZ_1M),
	},
	{
		.name		= "rootfs",
		.offset		= (21*SZ_1M),
		.size		= MTDPART_SIZ_FULL,
	},
	{
		.name		= "param",
		.offset		= (19*SZ_1M),
		.size		= (2*SZ_1M),
	},
	{
		.name		= "bootloader",
		.offset		= 0,
		.size		= (512*SZ_1K),
		.mask_flags	= MTD_CAP_NANDFLASH,
	}
};

struct s3c2410_nand_set mid_nand_sets[]  __initdata = {
    {
	    .nr_chips       = 1,
	    .nr_partitions  = ARRAY_SIZE(mid_nand_partitions),
	    .partitions     = mid_nand_partitions,
    }
};

struct s3c2410_platform_nand mid_nand_info __initdata = {
	.nr_sets        = ARRAY_SIZE(mid_nand_sets),
	.sets           = mid_nand_sets,
};
#endif

static struct delayed_work mid_audio_jack_watcher_work;

static void mid_audio_jack_watcher(struct work_struct *work)
{
    unsigned int nGPIO = GPIO_SPEAKER_POWER;
    gpio_request(nGPIO, "speaker-enable");
    gpio_direction_output(nGPIO, gpio_get_value(GPIO_AUDIO_JACK_CONN));
    gpio_free(nGPIO);

    schedule_delayed_work(&mid_audio_jack_watcher_work, HZ / 4);
}

static void mid_audio_jack_init(void) {
    int nGPIO = GPIO_AUDIO_JACK_CONN;
    gpio_request(nGPIO, "3.5-jack-detection");
    gpio_direction_input(nGPIO);
    gpio_free(nGPIO);

    INIT_DELAYED_WORK(&mid_audio_jack_watcher_work, mid_audio_jack_watcher);
    schedule_delayed_work(&mid_audio_jack_watcher_work, 0);
}

static void __init sound_init(void) {
	u32 reg;

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~(0x1f << 12);
	reg &= ~(0xf << 20);
	reg |= 0x12 << 12;
	reg |= 0x1  << 20;
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x3 << 8);
	reg |= 0x0 << 8;
	__raw_writel(reg, S5P_OTHERS);
}

static void __init mid_setup_clocks(void) {
	struct clk *pclk;
	struct clk *clk;

#ifdef CONFIG_S3C_DEV_HSMMC
	/* set MMC0 clock */
	clk = clk_get(&s3c_device_hsmmc0.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
				clk_get_rate(clk));
#endif

#ifdef CONFIG_S3C_DEV_HSMMC1
	/* set MMC1 clock */
	clk = clk_get(&s3c_device_hsmmc1.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
				clk_get_rate(clk));
#endif

#ifdef CONFIG_S3C_DEV_HSMMC2
	/* set MMC2 clock */
	clk = clk_get(&s3c_device_hsmmc2.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
				clk_get_rate(clk));
#endif

#ifdef CONFIG_S3C_DEV_HSMMC3
	/* set MMC3 clock */
	clk = clk_get(&s3c_device_hsmmc3.dev, "sclk_mmc");
	pclk = clk_get(NULL, "mout_mpll");
	clk_set_parent(clk, pclk);
	clk_set_rate(clk, 50*MHZ);

	pr_info("%s: %s: source is %s, rate is %ld\n",
				__func__, clk->name, clk->parent->name,
			 clk_get_rate(clk));
#endif
}

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_host_phy_init(void)
{
	/* USB PHY0 Enable */
	writel(readl(S5P_USB_PHY_CONTROL) | (0x1<<0),
			S5P_USB_PHY_CONTROL);
	writel((readl(S3C_USBOTG_PHYPWR) & ~(0x3<<3) & ~(0x1<<0)) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	writel((readl(S3C_USBOTG_PHYCLK) & ~(0x5<<2)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	writel((readl(S3C_USBOTG_RSTCON) & ~(0x3<<1)) | (0x1<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);
	writel(readl(S3C_USBOTG_RSTCON) & ~(0x7<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);

	/* rising/falling time */
	writel(readl(S3C_USBOTG_PHYTUNE) | (0x1<<20),
			S3C_USBOTG_PHYTUNE);

	/* set DC level as 6 (6%) */
	writel((readl(S3C_USBOTG_PHYTUNE) & ~(0xf)) | (0x1<<2) | (0x1<<1),
			S3C_USBOTG_PHYTUNE);
}
EXPORT_SYMBOL(otg_host_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(64)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	writel(readl(S3C_USBOTG_PHYPWR) | (0x3<<3),
			S3C_USBOTG_PHYPWR);
	writel(readl(S5P_USB_PHY_CONTROL) & ~(1<<0),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_phy_init(void)
{
	struct clk *otg_clk;

	otg_clk = clk_get(NULL, "otg");
	clk_enable(otg_clk);

	if (readl(S5P_USB_PHY_CONTROL) & (0x1<<1))
		return;

	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) | (0x1<<1),
			S5P_USB_PHY_CONTROL);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
			& ~(0x1<<7) & ~(0x1<<6)) | (0x1<<8) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK) & ~(0x1<<7)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)) | (0x1<<4) | (0x1<<3),
			S3C_USBOTG_RSTCON);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON) & ~(0x1<<4) & ~(0x1<<3),
			S3C_USBOTG_RSTCON);
}
EXPORT_SYMBOL(usb_host_phy_init);

void usb_host_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR) | (0x1<<7)|(0x1<<6),
			S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) & ~(1<<1),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(usb_host_phy_off);
#endif

#ifdef CONFIG_USB_EHCI_HCD
void mid_usbhost_setpower(int onoff) {
    unsigned int nGPIO = GPIO_USB_POWER;
    gpio_request(nGPIO, "smdk-usb-host");
    gpio_direction_output(nGPIO, onoff);
    gpio_free(nGPIO);

    if (onoff) {
        nGPIO = GPIO_USB_RESET;
        gpio_request(nGPIO, "smdk-usb-host-reset");
        gpio_direction_output(nGPIO, 0);
        gpio_free(nGPIO);

        mdelay(100);

        nGPIO = GPIO_USB_RESET;
        gpio_request(nGPIO, "smdk-usb-host-reset");
        gpio_direction_output(nGPIO, 1);
        gpio_free(nGPIO);
    }
};

int mid_ehci_phy_init(struct platform_device *pdev, int type) {
    if (type == S5P_USB_PHY_HOST)
        return 0;

    return -EINVAL;
}

int mid_ehci_phy_exit(struct platform_device *pdev, int type) {
    if (type == S5P_USB_PHY_HOST)
        return 0;

    return -EINVAL;
}

static struct s5p_ehci_platdata mid_ehci_pdata = {
    .phy_init = mid_ehci_phy_init,
    .phy_exit = mid_ehci_phy_exit
};

static void __init s3c_ehci_set_platdata(struct s5p_ehci_platdata *pd) {
	struct s5p_ehci_platdata *npd;

	npd = s3c_set_platdata(pd, sizeof(struct s5p_ehci_platdata),
			&s3c_device_usb_ehci);
}
#endif

#define S5PV210_PS_HOLD_CONTROL_REG (S3C_VA_SYS+0xE81C)
static void mid_power_off(void) {
    int nGPIO = GPIO_MAIN_POWER;

    /* PS_HOLD output High --> Low */
    writel(readl(S5PV210_PS_HOLD_CONTROL_REG) & 0xFFFFFEFF,
            S5PV210_PS_HOLD_CONTROL_REG);

    gpio_request(nGPIO, "main-power-en");
    gpio_direction_output(nGPIO, 0);
    gpio_free(nGPIO);

	while (1);
}

static bool console_flushed;

static void flush_console(void) {
	if (console_flushed)
		return;

	console_flushed = true;

	printk("\n");
	pr_emerg("Restarting %s\n", linux_banner);

	if (!is_console_locked())
		return;

	mdelay(50);

	local_irq_disable();
	if (!console_trylock())
		pr_emerg("flush_console: console was locked! busting!\n");
	else
		pr_emerg("flush_console: console was locked!\n");
	console_unlock();
}

static void mid_pm_restart(char mode, const char *cmd) {
	flush_console();
	arm_machine_restart(mode, cmd);
}

static struct platform_device *mid_devices[] __initdata = {
#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif
#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif
#ifdef CONFIG_VIDEO_G2D
	&s3c_device_g2d,
#endif
    &s3c_device_g3d,

#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif

	&s3c_device_i2c0,
#ifdef CONFIG_S3C_DEV_I2C1
	&s3c_device_i2c1,
#endif
#ifdef CONFIG_S3C_DEV_I2C2
	&s3c_device_i2c2,
#endif

	&s3c_device_rtc,
	&s3c_device_wdt,
	&s5pv210_device_iis0,
	&s5pv210_device_iis1,
	&samsung_asoc_dma,

#ifdef CONFIG_S3C_DEV_NAND
	&s3c_device_nand,
#endif

#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif

#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif

#ifdef CONFIG_USB_EHCI_HCD
	&s3c_device_usb_ehci,
#endif
#ifdef CONFIG_USB_OHCI_HCD
	&s3c_device_usb_ohci,
#endif

#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif

#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#endif

#ifdef CONFIG_S5P_ADC
    &s3c_device_adc,
#endif

#ifdef CONFIG_VIDEO_FIMC
    &s3c_device_fimc0,
    &s3c_device_fimc1,
    &s3c_device_fimc2,
#endif
#ifdef CONFIG_S5PV210_POWER_DOMAIN
	&s5pv210_pd_audio,
	&s5pv210_pd_cam,
	&s5pv210_pd_tv,
	&s5pv210_pd_lcd,
	&s5pv210_pd_g3d,
	&s5pv210_pd_mfc,
#endif
#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_adsp_device,
#endif

    &mid_backlight,
    &mid_battery,
    &mid_button,
    &mid_control,

#ifdef CONFIG_DM9000
	&mid_dm9000,
#endif
};

unsigned int pm_debug_scratchpad;

static void __init mid_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline,
		struct meminfo *mi) {

	mi->bank[0].start = 0x20000000;
	mi->bank[0].size = 256 * SZ_1M;

	mi->bank[1].start = 0x40000000;
	mi->bank[1].size = 256 * SZ_1M;

    mi->nr_banks = 2;

    // Use our internal RTC (smdkc110-rtc.c)
	s3c_device_rtc.name = "smdkc110-rtc";

#ifdef CONFIG_S3C_DEV_NAND
    // Use the s3c_nand driver (s3c_nand.c)
	s3c_device_nand.name = "s5pv210-nand";
#endif
}

static void __init mid_map_io(void) {
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s5pv210_gpiolib_init();
	s3c24xx_init_uarts(mid_uartcfgs, ARRAY_SIZE(mid_uartcfgs));
	s5p_reserve_bootmem(s5pv210_media_devs, ARRAY_SIZE(s5pv210_media_devs), 0);
}

static void __init mid_machine_init(void) {
	pm_power_off = mid_power_off;
	arm_pm_restart = mid_pm_restart;

    // Read configuration
    mid_init_cfg();
    mid_gpio_init();

    // Setup devices
    mid_detect_lcd();
    mid_detect_camera();
    mid_detect_i2cdevs();

#ifdef CONFIG_USB_EHCI_HCD
    // Power up the USB hub.
    mid_usbhost_setpower(1);
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif

#ifdef CONFIG_ANDROID_PMEM
    android_pmem_set_platdata();
#endif

#ifdef CONFIG_DM9000
	mid_dm9000_init();
#endif

	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_i2c2_set_platdata(NULL);

	i2c_register_board_info(0, mid_i2c_devs0, ARRAY_SIZE(mid_i2c_devs0));
	i2c_register_board_info(1, mid_i2c_devs1, ARRAY_SIZE(mid_i2c_devs1));
	i2c_register_board_info(2, mid_i2c_devs2, ARRAY_SIZE(mid_i2c_devs2));

#ifdef CONFIG_S3C_DEV_NAND
    s3c_nand_set_platdata(&mid_nand_info);
#endif

#ifdef CONFIG_USB_EHCI_HCD
    s3c_ehci_set_platdata(&mid_ehci_pdata);
#endif

	s3c_fb_set_platdata(&mid_fb_data);
    platform_add_devices(mid_devices, ARRAY_SIZE(mid_devices));

    sound_init();
    mid_audio_jack_init();

#ifdef CONFIG_S5P_ADC
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

	s3c_ts_set_platdata(&s3c_ts_platform);

#ifdef CONFIG_VIDEO_FIMC
    /* fimc */
    s3c_fimc0_set_platdata(&fimc_plat_lsi);
    s3c_fimc1_set_platdata(&fimc_plat_lsi);
    s3c_fimc2_set_platdata(&fimc_plat_lsi);
#endif
#ifdef CONFIG_VIDEO_JPEG_V2
	s3c_jpeg_set_platdata(&jpeg_plat);
#endif
#ifdef CONFIG_VIDEO_MFC50
	 /* mfc */
	s3c_mfc_set_platdata(NULL);
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	s5pv210_default_sdhci0();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s5pv210_default_sdhci3();
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif

#ifdef CONFIG_MFD_MAX8998_URBETTER
	regulator_has_full_constraints();
#endif

    mid_backlight_onoff(0);
    mid_lcd_onoff(0);
    mid_setup_clocks();
}

MACHINE_START(SMDKV210, "SMDKV210")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup			= mid_fixup,
	.init_irq		= s5pv210_init_irq,
	.map_io			= mid_map_io,
	.init_machine	= mid_machine_init,
	.timer			= &s5p_systimer,
MACHINE_END
