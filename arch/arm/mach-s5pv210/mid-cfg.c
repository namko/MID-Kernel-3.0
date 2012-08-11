/* linux/arch/arm/mach-s5pv210/mid-cfg.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <mach/mid-cfg.h>

char mid_manufacturer[16];
char mid_model[16];
char mid_camera[16];
char mid_pcb[16];
char mid_lcd[16];

static void getCmdLineEntry(char *name, char *out, unsigned int size) {
    const char *cmd = saved_command_line;
    const char *ptr = name;

    for (;;) {
        for (; *cmd && *ptr != *cmd; cmd++) ;
        for (; *cmd && *ptr && *ptr == *cmd; ptr++, cmd++) ;

        if (!*cmd) {
            *out = 0;
            return;
        } else if (!*ptr && *cmd == '=') {
            cmd++;
            break;
        } else {
            ptr = name;
            continue;
        }
    }

    for (; --size && *cmd && *cmd != ' '; cmd++, out++)
        *out = *cmd;

    *out = 0;
}

#define GET_CMDLINE(name, locn) getCmdLineEntry(name, locn, sizeof(locn))

void mid_init_cfg(void) {
    GET_CMDLINE("man", mid_manufacturer);
    GET_CMDLINE("utmodel", mid_model);
    GET_CMDLINE("pcb", mid_pcb);
    GET_CMDLINE("lcd", mid_lcd);
    GET_CMDLINE("camera", mid_camera);

    if (!strcmp(mid_manufacturer, "coby")) {
        // For all Coby models, "utmodel" entry may be absent, and "ts"
        // may be present in it's place.
        if (strlen(mid_model) == 0)
            getCmdLineEntry("ts", mid_model, sizeof(mid_model));

        // For 1024N, make some modifications to the model.
        if (!strcmp(mid_model, "1024n"))
            strcpy(mid_model, "1024");

        // If we are still unable to detect the model, detect from LCD.
        if (strlen(mid_model) == 0) {
            if (!strcmp(mid_lcd, "ut7gm"))
                strcpy(mid_model, "7024");
            else if (!strcmp(mid_lcd, "ut08gm"))
                strcpy(mid_model, "8024");
            else if (!strcmp(mid_lcd, "lp101"))
                strcpy(mid_model, "1024");
            else
                printk("*** WARNING cannot determine Coby model ***\n");
        }
    }

    printk("Got man=%s, utmodel=%s, pcb=%s, lcd=%s, camera=%s...\n", 
        mid_manufacturer, mid_model, mid_pcb, mid_lcd, mid_camera);
}

