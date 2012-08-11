/*
 * wm8976.c  --  WM8976 ALSA SoC Audio Codec driver
 *
 * Copyright 2006 Wolfson Microelectronics PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <asm/div64.h>

#include "wm8976.h"

#ifdef DEBUG
#define WM8976_DEBUG(x...) printk("[wm8976]" x)
#else
#define WM8976_DEBUG(x...) do { } while(0)
#endif

/* wm8976 register cache. Note that register 0 is not included in the cache. */
static const u16 wm8976_reg[WM8976_CACHEREGNUM] = {
    0x0000, 0x0000, 0x0000, 0x0000,	/* 0x00...0x03 */
    0x0050, 0x0000, 0x0140, 0x0000,	/* 0x04...0x07 */
    0x0000, 0x0000, 0x0000, 0x00ff,	/* 0x08...0x0b */
    0x00ff, 0x0000, 0x0100, 0x01ff,	/* 0x0c...0x0f */
    0x00ff, 0x0000, 0x012c, 0x002c,	/* 0x10...0x13 */
    0x002c, 0x002c, 0x002c, 0x0000,	/* 0x14...0x17 */
    0x0032, 0x0000, 0x0000, 0x0000,	/* 0x18...0x1b */
    0x0000, 0x0000, 0x0000, 0x0000,	/* 0x1c...0x1f */
    0x0038, 0x000b, 0x0032, 0x0000,	/* 0x20...0x23 */
    0x0008, 0x000c, 0x0093, 0x00e9,	/* 0x24...0x27 */
    0x0000, 0x0000, 0x0000, 0x0000,	/* 0x28...0x2b */
    0x0033, 0x0010, 0x0010, 0x0100,	/* 0x2c...0x2f */
    0x0100, 0x0002, 0x0001, 0x0001,	/* 0x30...0x33 */
    0x0032, 0x0032, 0x0039, 0x0039,	/* 0x34...0x37 */
    0x0001, 0x0001,			/* 0x38...0x3b */
};

/* codec private data */
struct wm8976_priv {
	void *control_data;
};

static const char *wm8976_companding[] = { "Off", "NC", "u-law", "A-law" };
static const char *wm8976_eqmode[] = { "Capture", "Playback" };
static const char *wm8976_bw[] = {"Narrow", "Wide" };
static const char *wm8976_eq1[] = {"80Hz", "105Hz", "135Hz", "175Hz" };
static const char *wm8976_eq2[] = {"230Hz", "300Hz", "385Hz", "500Hz" };
static const char *wm8976_eq3[] = {"650Hz", "850Hz", "1.1kHz", "1.4kHz" };
static const char *wm8976_eq4[] = {"1.8kHz", "2.4kHz", "3.2kHz", "4.1kHz" };
static const char *wm8976_eq5[] = {"5.3kHz", "6.9kHz", "9kHz", "11.7kHz" };
static const char *wm8976_alc[] = {"ALC", "Limiter" };

static const struct soc_enum wm8976_enum[] = {
	SOC_ENUM_SINGLE(WM8976_COMP, 1, 4, wm8976_companding),	/* adc */
	SOC_ENUM_SINGLE(WM8976_COMP, 3, 4, wm8976_companding),	/* dac */

	SOC_ENUM_SINGLE(WM8976_EQ1,  8, 2, wm8976_eqmode),
	SOC_ENUM_SINGLE(WM8976_EQ1,  5, 4, wm8976_eq1),

	SOC_ENUM_SINGLE(WM8976_EQ2,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ2,  5, 4, wm8976_eq2),

	SOC_ENUM_SINGLE(WM8976_EQ3,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ3,  5, 4, wm8976_eq3),

	SOC_ENUM_SINGLE(WM8976_EQ4,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ4,  5, 4, wm8976_eq4),

	SOC_ENUM_SINGLE(WM8976_EQ5,  8, 2, wm8976_bw),
	SOC_ENUM_SINGLE(WM8976_EQ5,  5, 4, wm8976_eq5),
	
	SOC_ENUM_SINGLE(WM8976_ALC3,  8, 2, wm8976_alc),
};

static const struct snd_kcontrol_new wm8976_snd_controls[] = 
{
	SOC_SINGLE("Digital Loopback Switch", WM8976_COMP, 0, 1, 0),

	SOC_ENUM("DAC Companding", wm8976_enum[1]),
	SOC_ENUM("ADC Companding", wm8976_enum[0]),

	SOC_SINGLE("High Pass Filter Switch", WM8976_ADC, 8, 1, 0),	
	SOC_SINGLE("High Pass Cut Off", WM8976_ADC, 4, 7, 0),	
	SOC_DOUBLE("ADC Inversion Switch", WM8976_ADC, 0, 1, 1, 0),
	SOC_SINGLE("Capture Volume", WM8976_ADCVOL,  0, 255, 0),
	SOC_SINGLE("Capture Boost(+20dB)", WM8976_ADCBOOST, 8, 1, 0),
	SOC_SINGLE("Capture PGA ZC Switch", WM8976_INPPGA,  7, 1, 0),
	SOC_SINGLE("Capture PGA Volume", WM8976_INPPGA,  0, 63, 0),

    SOC_SINGLE("ALC Enable Switch", WM8976_ALC1,  8, 1, 0),
    SOC_SINGLE("ALC Capture Max Gain", WM8976_ALC1,  3, 7, 0),
    SOC_SINGLE("ALC Capture Min Gain", WM8976_ALC1,  0, 7, 0),
    SOC_SINGLE("ALC Capture ZC Switch", WM8976_ALC2,  8, 1, 0),
    SOC_SINGLE("ALC Capture Hold", WM8976_ALC2,  4, 7, 0),
    SOC_SINGLE("ALC Capture Target", WM8976_ALC2,  0, 15, 0),
    SOC_ENUM("ALC Capture Mode", wm8976_enum[12]),
    SOC_SINGLE("ALC Capture Decay", WM8976_ALC3,  4, 15, 0),
    SOC_SINGLE("ALC Capture Attack", WM8976_ALC3,  0, 15, 0),
    SOC_SINGLE("ALC Capture Noise Gate Switch", WM8976_NGATE,  3, 1, 0),
    SOC_SINGLE("ALC Capture Noise Gate Threshold", WM8976_NGATE,  0, 7, 0),

	SOC_ENUM("Eq-3D Mode Switch", wm8976_enum[2]),	
	SOC_ENUM("Eq1 Cut-Off Frequency", wm8976_enum[3]),	
	SOC_SINGLE("Eq1 Volume", WM8976_EQ1,  0, 31, 1),	
	
	SOC_ENUM("Eq2 BandWidth Switch", wm8976_enum[4]),	
	SOC_ENUM("Eq2 Centre Frequency", wm8976_enum[5]),	
	SOC_SINGLE("Eq2 Volume", WM8976_EQ2,  0, 31, 1),

	SOC_ENUM("Eq3 BandWidth Switch", wm8976_enum[6]),	
	SOC_ENUM("Eq3 Centre Frequency", wm8976_enum[7]),	
	SOC_SINGLE("Eq3 Volume", WM8976_EQ3,  0, 31, 1),

	SOC_ENUM("Eq4 BandWidth Switch", wm8976_enum[8]),	
	SOC_ENUM("Eq4 Centre Frequency", wm8976_enum[9]),	
	SOC_SINGLE("Eq4 Volume", WM8976_EQ4,  0, 31, 1),
	
	SOC_ENUM("Eq5 BandWidth Switch", wm8976_enum[10]),	
	SOC_ENUM("Eq5 Centre Frequency", wm8976_enum[11]),	
	SOC_SINGLE("Eq5 Volume", WM8976_EQ5,  0, 31, 1),
	SOC_DOUBLE_R("PCM Playback Volume", WM8976_DACVOLL, WM8976_DACVOLR, 0, 127, 0),

	SOC_DOUBLE_R("Headphone Playback Switch", WM8976_HPVOLL,  WM8976_HPVOLR, 6, 1, 1),
	SOC_DOUBLE_R("Headphone Playback Volume", WM8976_HPVOLL,  WM8976_HPVOLR, 0, 62, 0),

	SOC_DOUBLE_R("Speaker Playback Switch", WM8976_SPKVOLL,  WM8976_SPKVOLR, 6, 1, 1),
	SOC_DOUBLE_R("Speaker Playback Volume", WM8976_SPKVOLL,  WM8976_SPKVOLR, 0, 62, 0),
};

/* Left Output Mixer */
static const struct snd_kcontrol_new wm8976_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left Playback Switch", WM8976_OUTPUT, 6, 1, 0),
	SOC_DAPM_SINGLE("Right Playback Switch", WM8976_MIXL, 0, 1, 0),
	SOC_DAPM_SINGLE("Bypass Playback Switch", WM8976_MIXL, 1, 1, 0),
	SOC_DAPM_SINGLE("Left Aux Switch", WM8976_MIXL, 5, 1, 0),
};

/* Right Output Mixer */
static const struct snd_kcontrol_new wm8976_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left Playback Switch", WM8976_OUTPUT, 5, 1, 0),
	SOC_DAPM_SINGLE("Right Playback Switch", WM8976_MIXR, 0, 1, 0),
	SOC_DAPM_SINGLE("Right Aux Switch", WM8976_MIXR, 5, 1, 0),
};

/* Out4 Mixer */
static const struct snd_kcontrol_new wm8976_out4_mixer_controls[] = {
	SOC_DAPM_SINGLE("VMID", WM8976_MONOMIX, 6, 1, 0),
	SOC_DAPM_SINGLE("Out4 LeftMixer Switch", WM8976_MONOMIX, 4, 1, 0),
	SOC_DAPM_SINGLE("Out4 LeftDac Switch", WM8976_MONOMIX, 3, 1, 0),
	SOC_DAPM_SINGLE("Out4 RightMixer Switch", WM8976_MONOMIX, 1, 1, 0),	
	SOC_DAPM_SINGLE("Out4 RightDac Switch", WM8976_MONOMIX, 0, 1, 0),
};

/* Out3 Mixer */
static const struct snd_kcontrol_new wm8976_out3_mixer_controls[] = {
	SOC_DAPM_SINGLE("VMID", WM8976_OUT3MIX, 6, 1, 0),
	SOC_DAPM_SINGLE("Out3 Out4Mixer Switch", WM8976_OUT3MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("Out3 BypassADC Switch", WM8976_OUT3MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("Out3 LeftMixer Switch", WM8976_OUT3MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("Out3 LeftDac Switch", WM8976_OUT3MIX, 0, 1, 0),
};

static const struct snd_kcontrol_new wm8976_boost_controls[] = {
	SOC_DAPM_SINGLE("Mic PGA Switch", WM8976_INPPGA,  6, 1, 1), 
	SOC_DAPM_SINGLE("AuxL Volume", WM8976_ADCBOOST, 0, 7, 0),
	SOC_DAPM_SINGLE("L2 Volume", WM8976_ADCBOOST, 4, 7, 0),
};

static const struct snd_kcontrol_new wm8976_micpga_controls[] = {
	SOC_DAPM_SINGLE("L2 Switch", WM8976_INPUT, 2, 1, 0),
	SOC_DAPM_SINGLE("MICN Switch", WM8976_INPUT, 1, 1, 0),
	SOC_DAPM_SINGLE("MICP Switch", WM8976_INPUT, 0, 1, 0),
};
static const struct snd_soc_dapm_widget wm8976_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("MICN"),
	SND_SOC_DAPM_INPUT("MICP"),
	SND_SOC_DAPM_INPUT("AUXL"),
	SND_SOC_DAPM_INPUT("AUXR"),
	SND_SOC_DAPM_INPUT("L2"),

	SND_SOC_DAPM_MICBIAS("Mic Bias", WM8976_POWER1, 4, 0),

	SND_SOC_DAPM_MIXER("Left Mixer", WM8976_POWER3, 2, 0,
		&wm8976_left_mixer_controls[0], ARRAY_SIZE(wm8976_left_mixer_controls)),
	SND_SOC_DAPM_PGA("Left Out 1", WM8976_POWER2, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", WM8976_POWER3, 6, 0, NULL, 0),
	SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback", WM8976_POWER3, 0, 0),

	SND_SOC_DAPM_MIXER("Right Mixer", WM8976_POWER3, 3, 0,
		&wm8976_right_mixer_controls[0], ARRAY_SIZE(wm8976_right_mixer_controls)),
	SND_SOC_DAPM_PGA("Right Out 1", WM8976_POWER2, 8, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 2", WM8976_POWER3, 5, 0, NULL, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback", WM8976_POWER3, 1, 0),

	SND_SOC_DAPM_ADC("ADC", "HiFi Capture", WM8976_POWER2, 0, 0),

	SND_SOC_DAPM_MIXER("Mic PGA", WM8976_POWER2, 2, 0,
	&wm8976_micpga_controls[0],ARRAY_SIZE(wm8976_micpga_controls)),	

	SND_SOC_DAPM_MIXER("Boost Mixer", WM8976_POWER2, 4, 0,
		&wm8976_boost_controls[0], ARRAY_SIZE(wm8976_boost_controls)),
	
	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),

	SND_SOC_DAPM_MIXER("Out3 Mixer", WM8976_POWER1, 6, 0,
		&wm8976_out3_mixer_controls[0], ARRAY_SIZE(wm8976_out3_mixer_controls)),	
	SND_SOC_DAPM_PGA("Out 3", WM8976_POWER1, 7, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("OUT3"),
	
	SND_SOC_DAPM_MIXER("Out4 Mixer", WM8976_POWER1, 7, 0,
		&wm8976_out4_mixer_controls[0], ARRAY_SIZE(wm8976_out4_mixer_controls)),
	SND_SOC_DAPM_PGA("Out 4", WM8976_POWER3, 8, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("OUT4"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* left mixer */
	{"Left Mixer", "Left Playback Switch", "Left DAC"},
	{"Left Mixer", "Right Playback Switch", "Right DAC"},
	{"Left Mixer", "Bypass Playback Switch", "Boost Mixer"},
	{"Left Mixer", "Left Aux Switch", "AUXL"},

	/* right mixer */
	{"Right Mixer", "Right Playback Switch", "Right DAC"},
	{"Right Mixer", "Left Playback Switch", "Left DAC"},
	{"Right Mixer", "Right Aux Switch", "AUXR"},
	
	/* left out */
	{"Left Out 1", NULL, "Left Mixer"},
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},
	{"LOUT2", NULL, "Left Out 2"},
	
	/* right out */
	{"Right Out 1", NULL, "Right Mixer"},
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},
	{"ROUT2", NULL, "Right Out 2"},

	/* Microphone PGA */
	{"Mic PGA", "MICN Switch", "MICN"},
	{"Mic PGA", "MICP Switch", "MICP"},
	{"Mic PGA", "L2 Switch", "L2" },
	
	/* Boost Mixer */
	{"Boost Mixer", "Mic PGA Switch", "Mic PGA"},
	{"Boost Mixer", "AuxL Volume", "AUXL"},
	{"Boost Mixer", "L2 Volume", "L2"},
	
	{"ADC", NULL, "Boost Mixer"},

	/* out 3 */
	{"Out3 Mixer", "VMID", "Out4 Mixer"},
	{"Out3 Mixer", "Out3 Out4Mixer Switch", "Out4 Mixer"},
	{"Out3 Mixer", "Out3 BypassADC Switch", "ADC"},
	{"Out3 Mixer", "Out3 LeftMixer Switch", "Left Mixer"},
	{"Out3 Mixer", "Out3 LeftDac Switch", "Left DAC"},
	{"Out 3", NULL, "Out3 Mixer"},
	{"OUT3", NULL, "Out 3"},
	
	/* out 4 */
	{"Out4 Mixer", "VMID", "Out3 Mixer"},
	{"Out4 Mixer", "Out4 LeftMixer Switch", "Left Mixer"},
	{"Out4 Mixer", "Out4 LeftDac Switch", "Left DAC"},
	{"Out4 Mixer", "Out4 RightMixer Switch", "Right Mixer"},	
	{"Out4 Mixer", "Out4 RightDac Switch", "Right DAC"},
	{"Out 4", NULL, "Out4 Mixer"},
	{"OUT4", NULL, "Out 4"},
};

static int wm8976_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_new_controls(dapm, wm8976_dapm_widgets,
				  ARRAY_SIZE(wm8976_dapm_widgets));
	/* set up the WM8976 audio map */
	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	return 0;
}

/*
 * @freq:	when .set_pll() us not used, freq is codec MCLK input frequency
 */
static int wm8976_set_dai_sysclk(struct snd_soc_dai *codec_dai, int clk_id,
				 unsigned int freq, int dir)
{
	return 0;
}

/*
 * Set ADC and Voice DAC format.
 */
static int wm8976_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = snd_soc_read(codec, WM8976_IFACE) & 0x7;
	u16 clk = snd_soc_read(codec, WM8976_CLOCK) & 0xfffe;

	dev_dbg(codec->dev, "%s\n", __func__);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) 
	{
	case SND_SOC_DAIFMT_CBM_CFM:
		clk |= 0x0001;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0010;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= 0x0000;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0008;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0018;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0180;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0100;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0080;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, WM8976_IFACE, iface);
	snd_soc_write(codec, WM8976_CLOCK, clk);

	return 0;
}

/*
 * Set PCM DAI bit size and sample rate.
 */
static int wm8976_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	u16 reg, iface, adn;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	reg = snd_soc_read(codec, WM8976_DACVOLL);
	snd_soc_write(codec, WM8976_DACVOLL, reg | 0x0100);
	reg = snd_soc_read(codec, WM8976_DACVOLR);
	snd_soc_write(codec, WM8976_DACVOLR, reg | 0x0100);
	reg = snd_soc_read(codec, WM8976_ADCVOL);
	snd_soc_write(codec, WM8976_ADCVOL, reg | 0x01ff);
	reg = snd_soc_read(codec, WM8976_INPPGA);
	snd_soc_write(codec, WM8976_INPPGA, reg | 0x01B8);
	reg = snd_soc_read(codec, WM8976_HPVOLL);
	snd_soc_write(codec, WM8976_HPVOLL, reg | 0x0100);
	reg = snd_soc_read(codec, WM8976_HPVOLR);
	snd_soc_write(codec, WM8976_HPVOLR, reg | 0x0100);
	reg = snd_soc_read(codec, WM8976_SPKVOLL);
	snd_soc_write(codec, WM8976_SPKVOLL, reg | 0x0100);
	reg = snd_soc_read(codec, WM8976_SPKVOLR);
	snd_soc_write(codec, WM8976_SPKVOLR, reg | 0x0100);

	iface = snd_soc_read(codec, WM8976_IFACE) & 0xff9f;
	adn = snd_soc_read(codec, WM8976_ADD) & 0x1f1;

	WM8976_DEBUG("%s:%d Entering...\n",__FUNCTION__, __LINE__);

	/* bit size */
	switch (params_format(params))
	{
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0020;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0040;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x0060;
		break;
	}

	/* filter coefficient */
	switch (params_rate(params))
	{
	case 8000:
		adn |= 0x5 << 1;
		break;
	case 11025:
		adn |= 0x4 << 1;
		break;
	case 16000:
		adn |= 0x3 << 1;
		break;
	case 22050:
		adn |= 0x2 << 1;
		break;
	case 32000:
		adn |= 0x1 << 1;
		break;
	case 44100:
	case 48000:
		break;
	}

	/* set iface */
	snd_soc_write(codec, WM8976_IFACE, iface);
	snd_soc_write(codec, WM8976_ADD, adn);
	return 0;
}

/*
 * Configure WM8976 clock dividers.
 */
static int wm8976_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
				 int div_id, int div)
{
	u16 reg;
	struct snd_soc_codec *codec = codec_dai->codec;

	WM8976_DEBUG("%s:%d Entering...\n",__FUNCTION__, __LINE__);
	
	switch (div_id) {
	case WM8976_MCLKDIV:
		reg = snd_soc_read(codec, WM8976_CLOCK) & 0x11f;
		snd_soc_write(codec, WM8976_CLOCK, reg | div);
		break;
	case WM8976_BCLKDIV:
		reg = snd_soc_read(codec, WM8976_CLOCK) & 0x1c7;
		snd_soc_write(codec, WM8976_CLOCK, reg | div);
		break;
	case WM8976_OPCLKDIV:
		reg = snd_soc_read(codec, WM8976_GPIO) & 0x1cf;
		snd_soc_write(codec, WM8976_GPIO, reg | div);
		break;
	case WM8976_DACOSR:
		reg = snd_soc_read(codec, WM8976_DAC) & 0x1f7;
		snd_soc_write(codec, WM8976_DAC, reg | div);
		break;
	case WM8976_ADCOSR:
		reg = snd_soc_read(codec, WM8976_ADC) & 0x1f7;
		snd_soc_write(codec, WM8976_ADC, reg | div);
		break;
	case WM8976_MCLKSEL:
		reg = snd_soc_read(codec, WM8976_CLOCK) & 0x0ff;
		snd_soc_write(codec, WM8976_CLOCK, reg | div);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int wm8976_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = snd_soc_read(codec, WM8976_DAC) & 0xffbf;

	WM8976_DEBUG("%s:%d mute = %d...\n",__FUNCTION__, __LINE__, mute);

	if (mute)
		snd_soc_write(codec, WM8976_DAC, mute_reg | 0x40);
	else
		snd_soc_write(codec, WM8976_DAC, mute_reg);

	return 0;
}

static int wm8976_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u16 pwr_reg = snd_soc_read(codec,WM8976_POWER1) & 0x0fc;
	
	snd_soc_write(codec, WM8976_INPUT, 0x03);
	switch (level) {
	case SND_SOC_BIAS_ON:
        /*set vmid to 75k for*/
        snd_soc_write(codec, WM8976_POWER1, (pwr_reg|0x01D) & 0x01D);
        break;
	case SND_SOC_BIAS_STANDBY:
        /* set vmid to 5k for quick power up */
        snd_soc_write(codec, WM8976_POWER1, (pwr_reg|0x001F) & 0X001F);
        break;
	case SND_SOC_BIAS_PREPARE:
        snd_soc_write(codec, WM8976_POWER1, (pwr_reg|0x001E) & 0X001E);
        break;
	case SND_SOC_BIAS_OFF:
		break;
		
	}
	codec->dapm.bias_level = level;
	return 0;
}

#define WM8976_FORMATS \
	(SNDRV_PCM_FORMAT_S16_LE | SNDRV_PCM_FORMAT_S20_3LE | \
	SNDRV_PCM_FORMAT_S24_3LE | SNDRV_PCM_FORMAT_S24_LE)

static struct snd_soc_dai_ops wm8976_dai_ops = {
	.hw_params		= wm8976_hw_params,
	.set_fmt		= wm8976_set_dai_fmt,
	.set_clkdiv		= wm8976_set_dai_clkdiv,
	.digital_mute	= wm8976_mute,
	.set_sysclk		= wm8976_set_dai_sysclk,
};

/* Also supports 12kHz */
static struct snd_soc_dai_driver wm8976_dai = {
	.name = "wm8976-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = WM8976_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = WM8976_FORMATS,
	},
	.ops = &wm8976_dai_ops,
};

static int wm8976_probe(struct snd_soc_codec *codec)
{
    u16 reg;
	int ret = 0;
	struct wm8976_priv *wm8976 = snd_soc_codec_get_drvdata(codec);

	codec->control_data = wm8976->control_data;
	ret = snd_soc_codec_set_cache_io(codec, 7, 9, SND_SOC_I2C);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/* Reset the codec */
	ret = snd_soc_write(codec, WM8976_RESET, 0);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset\n");
		return ret;
	}

	wm8976_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
	codec->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	msleep(msecs_to_jiffies(2000));

    snd_soc_write(codec, WM8976_IFACE, 0x010);
	snd_soc_write(codec, WM8976_CLOCK, 0x000);
	snd_soc_write(codec, WM8976_BEEP, 0x010);

	/*
	 * Set the update bit in all registers, that have one. This way all
	 * writes to those registers will also cause the update bit to be
	 * written.
	 */
    reg = snd_soc_read(codec, WM8976_DACVOLL);
    snd_soc_write(codec, WM8976_DACVOLL, reg | 0x0100);
    reg = snd_soc_read(codec, WM8976_DACVOLR);
    snd_soc_write(codec, WM8976_DACVOLR, reg | 0x0100);
    reg = snd_soc_read(codec, WM8976_ADCVOL);
    snd_soc_write(codec, WM8976_ADCVOL, reg | 0x01ff);
    reg = snd_soc_read(codec, WM8976_INPPGA);
    snd_soc_write(codec, WM8976_INPPGA, reg | 0x01B8);
    reg = snd_soc_read(codec, WM8976_HPVOLL);
    snd_soc_write(codec, WM8976_HPVOLL, reg | 0x0001);
    reg = snd_soc_read(codec, WM8976_HPVOLR);
    snd_soc_write(codec, WM8976_HPVOLR, reg | 0x0001);
    reg = snd_soc_read(codec, WM8976_SPKVOLL);
    snd_soc_write(codec, WM8976_SPKVOLL, reg | 0x0100);
    reg = snd_soc_read(codec, WM8976_SPKVOLR);
    snd_soc_write(codec, WM8976_SPKVOLR, reg | 0x0100);

    snd_soc_write(codec, WM8976_EQ5, 0x010D);
	snd_soc_write(codec, WM8976_INPUT, 0x03);

	wm8976_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_add_controls(codec, wm8976_snd_controls,
			     ARRAY_SIZE(wm8976_snd_controls));
	wm8976_add_widgets(codec);

	return 0;
}

/* power down chip */
static int wm8976_remove(struct snd_soc_codec *codec)
{
	wm8976_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_wm8976 = {
	.probe =	wm8976_probe,
	.remove =	wm8976_remove,
	.set_bias_level = wm8976_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(wm8976_reg),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = wm8976_reg,
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static __devinit int wm8976_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8976_priv *wm8976;
	int ret;

	wm8976 = kzalloc(sizeof(struct wm8976_priv), GFP_KERNEL);
	if (wm8976 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, wm8976);
	wm8976->control_data = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_wm8976, &wm8976_dai, 1);
	if (ret < 0)
		kfree(wm8976);
	return ret;
}

static __devexit int wm8976_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id wm8976_i2c_id[] = {
	{ "wm8976", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8976_i2c_id);

static struct i2c_driver wm8976_i2c_driver = {
	.driver = {
		.name = "wm8976",
		.owner = THIS_MODULE,
	},
	.probe =    wm8976_i2c_probe,
	.remove =   __devexit_p(wm8976_i2c_remove),
	.id_table = wm8976_i2c_id,
};
#endif

static int __init wm8976_modinit(void)
{
	int ret = 0;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&wm8976_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register WM8976 I2C driver: %d\n",
		       ret);
	}
#endif
	return ret;
}
module_init(wm8976_modinit);

static void __exit wm8976_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&wm8976_i2c_driver);
#endif
}
module_exit(wm8976_exit);

MODULE_DESCRIPTION("ASoC WM8976 codec driver");
MODULE_AUTHOR("Urbetter");
MODULE_LICENSE("GPL");

