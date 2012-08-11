/*
 *  mid_wm8976.c
 *
 *  Copyright (c) 
 *  Author: 
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/io.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-audss.h>

#include "../codecs/wm8976.h"
#include "s3c-dma.h"
#include "s5pc1xx-i2s.h"

#ifdef DEBUG
#define WM8976_DEBUG(format, arg...) printk("[audio_wm wm8976] " format, ## arg)
#else
#define WM8976_DEBUG(format, arg...) do{ } while(0)
#endif

#define wait_stable(utime_out)	\
	do {						\
		while (--utime_out) { 	\
			cpu_relax();		\
		}						\
	} while (0);

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;
	unsigned int wait_utime = 500;

	WM8976_DEBUG("Entered %s , rate=%ld\n", __func__, rate);
	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_disable(fout_epll);
	wait_stable(wait_utime);
	clk_set_rate(fout_epll, rate);
	clk_enable(fout_epll);

out:
	clk_put(fout_epll);

	return 0;
}

static int s5pv210_hifi_hw_params(struct snd_pcm_substream *substream,	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int epll_out_rate;
	int ret;
	int rfs, bfs, psr, rclk;

	WM8976_DEBUG("Entered %s , %d\n",__FUNCTION__, params_format(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	epll_out_rate = rclk * psr;
	printk("epll_out=%d, rclk=%d, psr=%d\n", epll_out_rate, rclk, psr);

	/* Set EPLL clock rate */
	ret = set_epll_rate(epll_out_rate);
	if (ret < 0) {
		printk(KERN_ERR "%s: set epll rate failed\n", __func__);
		return ret;
	}

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_ERR "%s: set codec dai failed\n", __func__);
		return ret;
	}
	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_ERR "%s: set cpu dai failed\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		printk(KERN_ERR "%s: set S3C64XX_CLKSRC_CDCLK for cpu dai failed\n", __func__);
		return ret;
	}
	
	/* We use SCLK_AUDIO for basic ops in SoC-Master mode */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_MUX,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "%s: set S3C64XX_CLKSRC_MUX for cpu dai failed\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
	if (ret < 0) {
		printk(KERN_ERR "%s:set clk divider RCLK for cpu dai failed\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_BCLK, bfs);
	if (ret < 0) {
		printk(KERN_ERR "%s:set clk divider BCLK for cpu dai failed\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER,psr-1);
	if (ret < 0) {
		printk(KERN_ERR "%s:set clk prescaler for cpu dai failed\n", __func__);
		return ret;
	}

	return 0;
}

static int s5pv210_wm8976_init(struct snd_soc_pcm_runtime *rtd)
{
	WM8976_DEBUG("Entered %s\n",__FUNCTION__);
	return 0;
}

static struct snd_soc_ops s5pv210_hifi_ops = {
	.hw_params = s5pv210_hifi_hw_params,
};

static struct snd_soc_dai_link mid_dai[] = {
    {  /* Primary Playback i/f */
	    .name = "WM8976 PAIF RX",
	    .stream_name = "Playback",
		.codec_name = "wm8976.1-001a",
		.platform_name = "samsung-audio",
		.cpu_dai_name = "samsung-i2s.1",
		.codec_dai_name = "wm8976-hifi",
	    .init = s5pv210_wm8976_init,		
	    .ops = &s5pv210_hifi_ops,
    },
};

static struct snd_soc_card card = {
	.name = "MID WM8976",
	.dai_link = mid_dai,
	.num_links = ARRAY_SIZE(mid_dai),
};

static struct platform_device *mid_snd_device;

static int __init mid_audio_init(void)
{
	int ret;

	WM8976_DEBUG("mid_audio_init start\n");

	mid_snd_device = platform_device_alloc("soc-audio", -1);
	if (!mid_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mid_snd_device, &card);
	ret = platform_device_add(mid_snd_device);
	if (ret)
		platform_device_put(mid_snd_device);

	printk("mid_audio_init done\n");
	return ret;
}

static void __exit mid_audio_exit(void)
{
	platform_device_unregister(mid_snd_device);
}

module_init(mid_audio_init);
module_exit(mid_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MID WM8976");
MODULE_AUTHOR("Urbetter");
MODULE_LICENSE("GPL");

