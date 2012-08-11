/* linux/drivers/media/video/hi704.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/midcam_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "hi704.h"

#define HI704_DRIVER_NAME           "hi704"
#define DEFAULT_FREQ                24000000

/* Camera functional setting values configured by user concept */
struct hi704_userset {
    int wb;             /* V4L2_CID_CAMERA_WHITE_BALANCE */
    int effect;         /* V4L2_CID_CAMERA_EFFECT */
};

struct hi704_state {
    struct v4l2_subdev sd;
    struct v4l2_mbus_framefmt fmt;
    struct hi704_userset userset;

    int freq;
    int is_mipi;
    int framesize_index;
    int check_previewdata;
};

struct hi704_enum_framesize {
    unsigned int index;
    unsigned int width;
    unsigned int height;
};

enum {
    HI704_PREVIEW_VGA,
};

struct hi704_enum_framesize hi704_framesize_list[] = {
    { HI704_PREVIEW_VGA, 640, 480 },
};

static const struct v4l2_mbus_framefmt capture_fmts[] = {
	{
		.code		= V4L2_MBUS_FMT_FIXED,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

static inline struct hi704_state *to_state(struct v4l2_subdev *sd) {
    return container_of(sd, struct hi704_state, sd);
}

/*
 * HI704 register structure : 1bytes address, 1bytes value
 * retry on write failure up-to 3 times
 */
static inline int hi704_write(struct v4l2_subdev *sd, u8 addr, u8 val) {
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct i2c_msg msg[1];
    unsigned char reg[2];
    int err = 0;
    int retry = 0;

    if (!client->adapter)
        return -ENODEV;

again:
    msg->addr = client->addr;
    msg->flags = 0;
    msg->len = 2;
    msg->buf = reg;

    reg[0] = addr & 0xff;
    reg[1] = val & 0xff;

    err = i2c_transfer(client->adapter, msg, 1);
    if (err >= 0)
        return err;    /* Returns here on success */

    /* abnormal case: retry 3 times */
    if (retry < 3) {
        dev_err(&client->dev, "%s: address: 0x%02x, "
            "value: 0x%02x; err = %d\n", __func__,
            reg[0], reg[1], err);
        retry++;
        goto again;
    }

    return err;
}

static int hi704_write_regs(struct v4l2_subdev *sd, struct hi704_reg regs[], int size) {
    int i, err;
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    for (i = 0; i < size; i++)
        if ((err = hi704_write(sd, regs[i].addr, regs[i].value)) < 0)
            v4l_info(client, "%s: register set failed\n", __func__);

    return 0;
}

static int hi704_s_config(struct v4l2_subdev *sd, struct midcam_platform_data *pdata) {
    struct hi704_state *state = to_state(sd);

    printk(KERN_INFO "fetching platform data\n");

    if (!pdata) {
        printk(KERN_ERR "%s: no platform data\n", __func__);
        return -ENODEV;
    }

    /*
     * Assign default format and resolution
     * Use configured default information in platform data
     * or without them, use default information in driver
     */
    if (!(pdata->default_width && pdata->default_height)) {
        state->fmt.width = hi704_framesize_list->width;
        state->fmt.height = hi704_framesize_list->height;
    } else {
        state->fmt.width = pdata->default_width;
        state->fmt.height = pdata->default_height;
    }

    state->fmt.colorspace = V4L2_PIX_FMT_JPEG;

    if (!pdata->freq)
        state->freq = DEFAULT_FREQ;
    else
        state->freq = pdata->freq;

    if (!pdata->is_mipi) {
        printk(KERN_INFO "parallel mode\n");
        state->is_mipi = 0;
    } else
        state->is_mipi = pdata->is_mipi;

    return 0;
}

static int hi704_init(struct v4l2_subdev *sd, u32 val) {
    int err = -EINVAL;

    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct hi704_state *state = to_state(sd);

    v4l_info(client, "%s: camera initialization start\n", __func__);

    if ((err = hi704_write_regs(sd, hi704_init_reg, ARRAY_SIZE(hi704_init_reg)))) {
        state->check_previewdata = 100;
        v4l_err(client, "%s: camera initialization failed. err(%d)\n",
            __func__, state->check_previewdata);
        return -EIO;
    }

    state->check_previewdata = 0;
    return 0;
}

static int hi704_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl) {
    int err = 0;
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct hi704_state *state = to_state(sd);
    struct hi704_userset *userset = &state->userset;

    switch (ctrl->id) {
    case V4L2_CID_CAMERA_WHITE_BALANCE:
        ctrl->value = userset->wb;
        break;
    case V4L2_CID_CAMERA_EFFECT:
        ctrl->value = userset->effect;
        break;
    case V4L2_CID_CAMERA_BRIGHTNESS:
    case V4L2_CID_WHITE_BALANCE_PRESET:
    case V4L2_CID_CAMERA_CONTRAST:
    case V4L2_CID_CAMERA_SATURATION:
    case V4L2_CID_CAMERA_SHARPNESS:
    case V4L2_CID_CAM_JPEG_MAIN_SIZE:
    case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
    case V4L2_CID_CAM_JPEG_THUMB_SIZE:
    case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
    case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
    case V4L2_CID_CAM_JPEG_MEMSIZE:
    case V4L2_CID_CAM_JPEG_QUALITY:
    case V4L2_CID_CAM_DATE_INFO_YEAR:
    case V4L2_CID_CAM_DATE_INFO_MONTH:
    case V4L2_CID_CAM_DATE_INFO_DATE:
    case V4L2_CID_CAM_SENSOR_VER:
    case V4L2_CID_CAM_FW_MINOR_VER:
    case V4L2_CID_CAM_FW_MAJOR_VER:
    case V4L2_CID_CAM_PRM_MINOR_VER:
    case V4L2_CID_CAM_PRM_MAJOR_VER:
    case V4L2_CID_ESD_INT:
    case V4L2_CID_CAMERA_GET_ISO:
    case V4L2_CID_CAMERA_GET_SHT_TIME:
    case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
    case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
    case V4L2_CID_EXPOSURE:
        break;
    default:
        dev_err(&client->dev, "%s: no such ctrl\n", __func__);
        break;
    }

    return err;
}

static int hi704_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl) {
    int err = 0;
    int value = ctrl->value;

    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct hi704_state *state = to_state(sd);
    struct hi704_userset *userset = &state->userset;

    switch (ctrl->id) {
    case V4L2_CID_CAMERA_WHITE_BALANCE:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_WHITE_BALANCE\n", __func__);
        switch (value) {
        default:
            dev_err(&client->dev, "Invalid value for camera control 0x%08x.\n", ctrl->id);
            value = WHITE_BALANCE_AUTO;
        case WHITE_BALANCE_AUTO:
            err = hi704_write_regs(sd, hi704_white_balance_auto, ARRAY_SIZE(hi704_white_balance_auto));
            break;
        case WHITE_BALANCE_TUNGSTEN:
            err = hi704_write_regs(sd, hi704_white_balance_incandescence, ARRAY_SIZE(hi704_white_balance_incandescence));
            break;
        case WHITE_BALANCE_SUNNY:
            err = hi704_write_regs(sd, hi704_white_balance_daylight, ARRAY_SIZE(hi704_white_balance_daylight));
            break;
        case WHITE_BALANCE_FLUORESCENT:
            err = hi704_write_regs(sd, hi704_white_balance_fluorescent, ARRAY_SIZE(hi704_white_balance_fluorescent));
            break;
        case WHITE_BALANCE_CLOUDY:
            err = hi704_write_regs(sd, hi704_white_balance_cloud, ARRAY_SIZE(hi704_white_balance_cloud));
            break;
        }

        if (!err)
            userset->wb = value;

        break;
    case V4L2_CID_CAMERA_EFFECT:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT\n", __func__);

        switch (value) {
        default:
            dev_err(&client->dev, "Invalid value for camera control 0x%08x.\n", ctrl->id);
            value = IMAGE_EFFECT_NONE;
        case IMAGE_EFFECT_NONE:
            err = hi704_write_regs(sd, hi704_effect_normal, ARRAY_SIZE(hi704_effect_normal));
            break;
        case IMAGE_EFFECT_BNW:
            err = hi704_write_regs(sd, hi704_effect_grayscale, ARRAY_SIZE(hi704_effect_grayscale));
            break;
        case IMAGE_EFFECT_SEPIA:
            err = hi704_write_regs(sd, hi704_effect_sepia, ARRAY_SIZE(hi704_effect_sepia));
            break;
        case IMAGE_EFFECT_NEGATIVE:
            err = hi704_write_regs(sd, hi704_effect_color_inv, ARRAY_SIZE(hi704_effect_color_inv));
            break;
        }

        if (!err)
            userset->effect = value;

        break;

    case V4L2_CID_CAM_PREVIEW_ONOFF:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAM_PREVIEW_ONOFF\n", __func__);

        if (state->check_previewdata == 0)
            err = 0;
        else
            err = -EIO;

        break;

    case V4L2_CID_CAMERA_RESET:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_RESET\n", __func__);
        err = hi704_init(sd, 0);

        break;

    case V4L2_CID_CAMERA_BRIGHTNESS:
    case V4L2_CID_CAMERA_ISO:
    case V4L2_CID_CAMERA_METERING:
    case V4L2_CID_CAMERA_CONTRAST:
    case V4L2_CID_CAMERA_SATURATION:
    case V4L2_CID_CAMERA_SHARPNESS:
    case V4L2_CID_CAMERA_WDR:
    case V4L2_CID_CAMERA_FACE_DETECTION:
    case V4L2_CID_CAMERA_FOCUS_MODE:
    case V4L2_CID_CAMERA_FLASH_MODE:
    case V4L2_CID_CAMERA_SCENE_MODE:
    case V4L2_CID_CAMERA_GPS_LATITUDE:
    case V4L2_CID_CAMERA_GPS_LONGITUDE:
    case V4L2_CID_CAMERA_GPS_TIMESTAMP:
    case V4L2_CID_CAMERA_GPS_ALTITUDE:
    case V4L2_CID_CAMERA_OBJECT_POSITION_X:
    case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
    case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
    case V4L2_CID_CAMERA_FRAME_RATE:
    case V4L2_CID_CAMERA_CHECK_DATALINE:
    case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
    case V4L2_CID_CAM_JPEG_QUALITY:
    case V4L2_CID_EXPOSURE:
        break;
    default:
        dev_err(&client->dev, "%s: no such control 0x%08x\n", __func__, ctrl->id);
        break;
    }

    if (err < 0)
        dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);

    return err;
}

static int hi704_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc) {
    int i;

    for (i = 0; i < ARRAY_SIZE(hi704_controls); i++)
        if (hi704_controls[i].id == qc->id) {
            memcpy(qc, &hi704_controls[i], sizeof(struct v4l2_queryctrl));
            return 0;
        }

    return -EINVAL;
}

static int hi704_s_crystal_freq(struct v4l2_subdev *sd, u32  freq, u32 flags) {
    int err = -EINVAL;
    return err;
}

static int hi704_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				  enum v4l2_mbus_pixelcode *code) {
    struct i2c_client *client = v4l2_get_subdevdata(sd);
	dev_dbg(&client->dev, "%s: index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(capture_fmts))
		return -EINVAL;

	*code = capture_fmts[index].code;

	return 0;
}

static int hi704_g_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt) {
    struct hi704_state *state = to_state(sd);
    *fmt = state->fmt;
    return 0;
}

static int hi704_try_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt) {
    struct i2c_client *client = v4l2_get_subdevdata(sd);
	int num_entries;
	int i;

	num_entries = ARRAY_SIZE(capture_fmts);

	dev_err(&client->dev, "%s: code = 0x%x, colorspace = 0x%x, num_entries = %d\n",
		__func__, fmt->code, fmt->colorspace, num_entries);

	for (i = 0; i < num_entries; i++) {
		if (capture_fmts[i].code == fmt->code &&
		    capture_fmts[i].colorspace == fmt->colorspace) {
			dev_dbg(&client->dev, "%s: match found, returning 0\n", __func__);
			return 0;
		}
	}

	dev_dbg(&client->dev, "%s: no match found, returning -EINVAL\n", __func__);
	return -EINVAL;
}

static int hi704_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt) {
    struct hi704_state *state = to_state(sd);
    struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s: code = 0x%x, field = 0x%x,"
		" colorspace = 0x%x, width = %d, height = %d\n",
		__func__, fmt->code, fmt->field,
		fmt->colorspace,
		fmt->width, fmt->height);

	if (fmt->code == V4L2_MBUS_FMT_FIXED &&
    		fmt->colorspace != V4L2_COLORSPACE_JPEG) {
		dev_err(&client->dev,
			"%s: mismatch in pixelformat and colorspace\n",
			__func__);
		return -EINVAL;
	}

    if (!(fmt->width == 640 && fmt->height == 480)) {
		dev_err(&client->dev,
			"%s: unsupported resolution\n",
			__func__);
        return -EINVAL;
    }

	state->fmt = *fmt;
    return 0;
}

static int hi704_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param) {
    int err = 0;
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    
    dev_dbg(&client->dev, "%s\n", __func__);
    
    return err;
}

static int hi704_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param) {
    int err = 0;
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n",
        __func__, param->parm.capture.timeperframe.numerator,
        param->parm.capture.timeperframe.denominator);

    return err;
}

static int hi704_enum_framesizes(struct v4l2_subdev *sd,
        struct v4l2_frmsizeenum *fsize) {

    struct hi704_state *state = to_state(sd);
    struct hi704_enum_framesize *elem;

    int i = 0;
    int index = 0;
    int num_entries = sizeof(hi704_framesize_list) / 
        sizeof(struct hi704_enum_framesize);

    /* The camera interface should read this value, this is the resolution
     * at which the sensor would provide framedata to the camera i/f
     *
     * In case of image capture,
     * this returns the default camera resolution (VGA)
     */
    fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

    index = state->framesize_index;

    for (i = 0; i < num_entries; i++) {
        elem = &hi704_framesize_list[i];
        if (elem->index == index) {
            fsize->discrete.width = hi704_framesize_list[index].width;
            fsize->discrete.height = hi704_framesize_list[index].height;
            return 0;
        }
    }

    return -EINVAL;
}

static const struct v4l2_subdev_core_ops hi704_core_ops = {
    .init = hi704_init,
    .queryctrl = hi704_queryctrl,
    .g_ctrl = hi704_g_ctrl,
    .s_ctrl = hi704_s_ctrl,
};

static const struct v4l2_subdev_video_ops hi704_video_ops = {
    .s_crystal_freq = hi704_s_crystal_freq,
    .enum_mbus_fmt = hi704_enum_mbus_fmt,
    .g_mbus_fmt = hi704_g_mbus_fmt,
    .try_mbus_fmt = hi704_try_mbus_fmt,
    .s_mbus_fmt = hi704_s_mbus_fmt,
    .g_parm = hi704_g_parm,
    .s_parm = hi704_s_parm,    
    .enum_framesizes = hi704_enum_framesizes,
};

static const struct v4l2_subdev_ops hi704_ops = {
    .core = &hi704_core_ops,
    .video = &hi704_video_ops,
};

/*
 * hi704_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int hi704_probe(struct i2c_client *client,
        const struct i2c_device_id *id) {

    struct hi704_state *state;
    struct v4l2_subdev *sd;
    struct midcam_platform_data *pdata = client->dev.platform_data;

    if ((state = kzalloc(sizeof(struct hi704_state), GFP_KERNEL)) == NULL)
        return -ENOMEM;

    sd = &state->sd;
    strcpy(sd->name, HI704_DRIVER_NAME);
    hi704_s_config(sd, pdata);

    /* Registering subdev */
    v4l2_i2c_subdev_init(sd, client, &hi704_ops);
    dev_info(&client->dev, "hi704 has been probed\n");
    return 0;
}

static int hi704_remove(struct i2c_client *client) {
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    v4l2_device_unregister_subdev(sd);
    kfree(to_state(sd));
    return 0;
}

static const struct i2c_device_id hi704_id[] = {
    { HI704_DRIVER_NAME, 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, hi704_id);

static struct i2c_driver v4l2_i2c_driver = {
    .driver.name = HI704_DRIVER_NAME,
    .probe = hi704_probe,
    .remove = hi704_remove,
    .id_table = hi704_id,
};

static int __init v4l2_i2c_drv_init(void)
{
	return i2c_add_driver(&v4l2_i2c_driver);
}

static void __exit v4l2_i2c_drv_cleanup(void)
{
	i2c_del_driver(&v4l2_i2c_driver);
}

module_init(v4l2_i2c_drv_init);
module_exit(v4l2_i2c_drv_cleanup);

MODULE_DESCRIPTION("Hynix HI704 VGA camera driver");
MODULE_AUTHOR("Urbetter");
MODULE_LICENSE("GPL");

