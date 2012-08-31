/*
 * Driver for keys on MID703-based tablets.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/pm.h>
#include <linux/gpio.h>

#include <mach/mid-cfg.h>
#include <mach/gpio-mid.h>

#define DEVICE_NAME                     "s3c-button"
#define POLL_INTERVAL_IN_MS             80
#define BUTTON_COUNT                    6

static const unsigned int nGPIOs[BUTTON_COUNT] = {
    GPIO_KEY_BACK,
    GPIO_KEY_END,
    GPIO_KEY_MENU,
    GPIO_KEY_HOME,
    GPIO_KEY_VOLUME_UP,
    GPIO_KEY_VOLUME_DOWN
};

static int nButtonCodes[BUTTON_COUNT] = {
    KEY_BACK,
    KEY_END,
    KEY_MENU,
    KEY_HOME,
    KEY_VOLUMEUP,
    KEY_VOLUMEDOWN
};

static bool bButtonWasPressed[BUTTON_COUNT] = {false};

static struct timer_list timer;
static struct input_dev *input_dev;

static void mid_button_timer_handler(unsigned long arg) {
    int i;

    for (i = 0; i < BUTTON_COUNT; i++) {
        bool pressed = !gpio_get_value(nGPIOs[i]);

        if (pressed != bButtonWasPressed[i])
            input_report_key(input_dev, nButtonCodes[i], pressed);

        bButtonWasPressed[i] = pressed;
    }

    input_sync(input_dev);
    mod_timer(&timer, jiffies + msecs_to_jiffies(POLL_INTERVAL_IN_MS));
}

static int mid_button_probe(struct platform_device *pdev) {
    int i, j, ret;
    printk("== %s ==\n", __func__);

    for (i = 0; i < BUTTON_COUNT; i++)
        if ((ret = gpio_request(nGPIOs[i], DEVICE_NAME))) {
            printk("*** ERROR coudn't request GPIO 0x%x for %s! ***",
                nGPIOs[i], DEVICE_NAME);

            for (j = i - 1; j >= 0; j--)
                gpio_free(nGPIOs[j]);

            return ret;
        }

    for (i = 0; i < BUTTON_COUNT; i++) {
        if ((ret = gpio_direction_input(nGPIOs[i]))) {
            printk("*** ERROR coudn't set direction of GPIO 0x%x for %s! ***",
                nGPIOs[i], DEVICE_NAME);
            goto err_gpio_config;
        }

        if ((ret = s3c_gpio_setpull(nGPIOs[i], S3C_GPIO_PULL_UP))) {
            printk("*** ERROR coudn't set pull of GPIO 0x%x for %s! ***",
                nGPIOs[i], DEVICE_NAME);
            goto err_gpio_config;
        }
    }

    if (!(input_dev = input_allocate_device())) {
        printk("*** ERROR coudn't allocate input device for " DEVICE_NAME "! ***");
        ret = -ENOMEM;
        goto err_input_allocate_device;
    }

	input_dev->name = DEVICE_NAME;
	input_dev->phys = DEVICE_NAME "/input0";
	input_dev->dev.parent = &pdev->dev;

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

    for (i = 0; i < BUTTON_COUNT; i++)
        set_bit(nButtonCodes[i], input_dev->keybit);

    set_bit(EV_KEY, input_dev->evbit);

    if ((ret = input_register_device(input_dev))) {
        printk("*** ERROR coudn't register input device for " DEVICE_NAME "! ***");
        goto err_input_register_device;
    }

    init_timer(&timer);
    timer.function = mid_button_timer_handler;
    timer.expires = jiffies + msecs_to_jiffies(POLL_INTERVAL_IN_MS);
    add_timer(&timer);

    // Switch HOME\BACK on Coby.
    if (!strcmp(mid_manufacturer, "coby"))
        for (i = 0; i < BUTTON_COUNT; i++)
            switch (nButtonCodes[i]) {
                case KEY_BACK:
                    nButtonCodes[i] = KEY_HOME;
                    break;
                case KEY_HOME:
                    nButtonCodes[i] = KEY_BACK;
                    break;
            }

    printk("== %s initialized! ==\n", DEVICE_NAME);
    return 0;

err_input_register_device:
    input_free_device(input_dev);

err_input_allocate_device:
err_gpio_config:
    for (i = 0; i < BUTTON_COUNT; i++)
        gpio_free(nGPIOs[i]);

    return ret;
}

static int __devexit mid_button_remove(struct platform_device *pdev) {
    int i;
    printk("== %s ==\n", __func__);

    del_timer(&timer);
    input_unregister_device(input_dev);

    for (i = 0; i < BUTTON_COUNT; i++)
        gpio_free(nGPIOs[i]);

    return 0;
}

#ifdef CONFIG_PM
static int mid_button_suspend(struct platform_device *dev, pm_message_t state) {
    printk("== %s ==\n", __func__);
    return 0;
}

static int mid_button_resume(struct platform_device *dev) {
    printk("== %s ==\n", __func__);
    return 0;
}
#else
#define s3c_keypad_suspend NULL
#define s3c_keypad_resume  NULL
#endif /* CONFIG_PM */

static struct platform_driver mid_button_device_driver = {
	.probe		= mid_button_probe,
	.remove		= mid_button_remove,
	.suspend	= mid_button_suspend,
	.resume		= mid_button_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DEVICE_NAME,
	},
};

static int __init mid_button_init(void) {
	int ret;
    printk("== %s ==\n", __func__);

    memset(bButtonWasPressed, 0, sizeof(bButtonWasPressed));

    if ((ret = platform_driver_register(&mid_button_device_driver))) {
        printk("*** ERROR [%d] coudn't register %s driver! ***\n", ret, DEVICE_NAME);
        return ret;
    }

	return 0;
}

static void __exit mid_button_exit(void) {
    printk("== %s ==\n", __func__);
    platform_driver_unregister(&mid_button_device_driver);
}

module_init(mid_button_init);
module_exit(mid_button_exit);

MODULE_DESCRIPTION("s3c-button interface for MID703");
MODULE_AUTHOR("namko");
MODULE_LICENSE("GPL");

