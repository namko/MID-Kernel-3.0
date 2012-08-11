/* linux/arch/arm/mach-s5pv210/mid-gpio.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/string.h>
#include <linux/gpio.h>
#include <mach/mid-cfg.h>
#include <mach/gpio-mid.h>

MID_GPIO_DEFINE(GPIO_BATTERY_FULL)
MID_GPIO_DEFINE(GPIO_LCD_BACKLIGHT_POWER)
MID_GPIO_DEFINE(GPIO_LCD_BACKLIGHT_POWERB)
MID_GPIO_DEFINE(GPIO_USB_POWER)
MID_GPIO_DEFINE(GPIO_USB_RESET)
MID_GPIO_DEFINE(GPIO_USB_OTG_SWITCH)
MID_GPIO_DEFINE(GPIO_3G_POWER)
MID_GPIO_DEFINE(GPIO_GPS_POWER)
MID_GPIO_DEFINE(GPIO_ETHERNET_POWER)

void mid_gpio_init() {
    if (!strcmp(mid_pcb, "single")) {
        printk("* Selecting PCB \"single\"...\n");
        MID_GPIO_SET_ALL(SINGLE)
    } else {
        printk("* Selecting PCB \"cv04\"...\n");
        MID_GPIO_SET_ALL(CV04)
    }
}

