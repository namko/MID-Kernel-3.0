#ifndef __GPIO_MID_H_
#define __GPIO_MID_H_

/* Declare a dynamic GPIO */
#define MID_GPIO_DECLARE(x)                     \
int mid_##x(void);

/* Define a dynamic GPIO */
#define MID_GPIO_DEFINE(x)                      \
static int val_##x;                             \
int mid_##x(void) {                             \
    return val_##x;                             \
}

/* Read a dynamic GPIO */
#define MID_GPIO_GET(x)                         \
mid_##x()

/* Set a dynamic GPIO for a particular PCB */
#define MID_GPIO_SET(x, pcb)                    \
val_##x = x##_##pcb;

/* Set all dynamic GPIOs for a particular PCB
 * Requires definition of all GPIOs for all PCBs */
#define MID_GPIO_SET_ALL(pcb)                   \
{                                               \
MID_GPIO_SET(GPIO_BATTERY_FULL, pcb)            \
MID_GPIO_SET(GPIO_LCD_BACKLIGHT_POWER, pcb)     \
MID_GPIO_SET(GPIO_LCD_BACKLIGHT_POWERB, pcb)    \
MID_GPIO_SET(GPIO_USB_POWER, pcb)               \
MID_GPIO_SET(GPIO_USB_RESET, pcb)               \
MID_GPIO_SET(GPIO_USB_OTG_SWITCH, pcb)          \
MID_GPIO_SET(GPIO_3G_POWER, pcb)                \
MID_GPIO_SET(GPIO_GPS_POWER, pcb)               \
MID_GPIO_SET(GPIO_ETHERNET_POWER, pcb)          \
}

/* Late-bound GPIOs (depending on selected PCB). */
MID_GPIO_DECLARE(GPIO_BATTERY_FULL)
MID_GPIO_DECLARE(GPIO_LCD_BACKLIGHT_POWER)
MID_GPIO_DECLARE(GPIO_LCD_BACKLIGHT_POWERB)
MID_GPIO_DECLARE(GPIO_USB_POWER)
MID_GPIO_DECLARE(GPIO_USB_RESET)
MID_GPIO_DECLARE(GPIO_USB_OTG_SWITCH)
MID_GPIO_DECLARE(GPIO_3G_POWER)
MID_GPIO_DECLARE(GPIO_GPS_POWER)
MID_GPIO_DECLARE(GPIO_ETHERNET_POWER)

/* (Fixed) GPIOs for PCB: "cv04" */
#define GPIO_BATTERY_FULL_CV04                  S5PV210_GPH0(2)
#define GPIO_LCD_BACKLIGHT_POWER_CV04           S5PV210_GPH1(6)
#define GPIO_LCD_BACKLIGHT_POWERB_CV04          S5PV210_GPH1(7)
#define GPIO_USB_POWER_CV04                     S5PV210_GPH2(5)
#define GPIO_USB_RESET_CV04                     S5PV210_GPH2(0)
#define GPIO_USB_OTG_SWITCH_CV04                S5PV210_GPH1(1)
#define GPIO_3G_POWER_CV04                      S5PV210_GPH0(7)
#define GPIO_GPS_POWER_CV04                     S5PV210_GPH1(1)
#define GPIO_ETHERNET_POWER_CV04                S5PV210_GPB(2)

/* (Fixed) GPIOs for PCB: "single" */
#define GPIO_BATTERY_FULL_SINGLE                S5PV210_GPJ2(0)
#define GPIO_LCD_BACKLIGHT_POWER_SINGLE         S5PV210_GPJ4(0)
#define GPIO_LCD_BACKLIGHT_POWERB_SINGLE        S5PV210_GPJ4(1)
#define GPIO_USB_POWER_SINGLE                   S5PV210_GPJ2(1)
#define GPIO_USB_RESET_SINGLE                   S5PV210_GPJ2(2)
#define GPIO_USB_OTG_SWITCH_SINGLE              S5PV210_GPJ2(7)
#define GPIO_3G_POWER_SINGLE                    S5PV210_GPJ3(0)
#define GPIO_GPS_POWER_SINGLE                   S5PV210_GPJ2(6)
#define GPIO_ETHERNET_POWER_SINGLE              S5PV210_GPJ4(3)

// Main power
#define GPIO_MAIN_POWER                         S5PV210_GPH0(5)

// Audio
#define GPIO_AUDIO_JACK_CONN                    S5PV210_GPH0(0)
#define GPIO_SPEAKER_POWER                      S5PV210_GPH3(2)
#define GPIO_AUDIO_POWER                        S5PV210_GPH3(3)

// Battery
#define GPIO_AC_CONNECTED                       S5PV210_GPH3(6)
#define GPIO_BATTERY_FULL                       MID_GPIO_GET(GPIO_BATTERY_FULL)

// Backlight
#define GPIO_BACKLIGHT                          S5PV210_GPH2(4)

// LCD
#define GPIO_LCD_BACKLIGHT_POWER                MID_GPIO_GET(GPIO_LCD_BACKLIGHT_POWER)
#define GPIO_LCD_BACKLIGHT_POWERB               MID_GPIO_GET(GPIO_LCD_BACKLIGHT_POWERB)

// Touchscreen: ft5406
#define GPIO_FT5406_POWER                       S5PV210_GPH2(3)
#define GPIO_FT5406_WAKE                        S5PV210_GPH2(1)
#define GPIO_FT5406_INTERRUPT                   S5PV210_GPH1(0)

// Buttons
#define GPIO_KEY_BACK                           S5PV210_GPH0(4)
#define GPIO_KEY_END                            S5PV210_GPH0(1)
#define GPIO_KEY_MENU                           S5PV210_GPH3(4)
#define GPIO_KEY_HOME                           S5PV210_GPH3(5)
#define GPIO_KEY_VOLUME_UP                      S5PV210_GPH1(3)
#define GPIO_KEY_VOLUME_DOWN                    S5PV210_GPH1(4)

// Camera
#define GPIO_CAMERA_POWER                       S5PV210_GPH2(2)
#define GPIO_CAMERA_POWERB                      S5PV210_GPJ4(4)
#define GPIO_CAMERA_RESET                       S5PV210_GPE1(4)

// WiFi
#define GPIO_POWER_WIFI                         S5PV210_GPH2(6)
#define GPIO_POWER_WIFI_PD                      S5PV210_GPH2(7)
#define GPIO_POWER_SDIO                         S5PV210_GPH2(6)

// USB
#define GPIO_USB_POWER                          MID_GPIO_GET(GPIO_USB_POWER)
#define GPIO_USB_RESET                          MID_GPIO_GET(GPIO_USB_RESET)
#define GPIO_USB_OTG_SWITCH                     MID_GPIO_GET(GPIO_USB_OTG_SWITCH)

// G-Sensor
#define GPIO_SENSOR_POWER                       S5PV210_GPH3(1)

// 3G
#define GPIO_3G_POWER                           MID_GPIO_GET(GPIO_3G_POWER)

// GPS
#define GPIO_GPS_POWER                          MID_GPIO_GET(GPIO_GPS_POWER)

// Ethernet
#define GPIO_ETHERNET_POWER                     MID_GPIO_GET(GPIO_ETHERNET_POWER)

// Determine current PCB and initialize GPIO mappings.
void mid_gpio_init(void);

#endif
