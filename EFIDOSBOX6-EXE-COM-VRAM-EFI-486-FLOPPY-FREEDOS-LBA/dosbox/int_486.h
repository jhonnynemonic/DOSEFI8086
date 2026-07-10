#ifndef DOSBOX_INT_H
#define DOSBOX_INT_H

//#include "../dosbox/stdint.h"
#include <stdint.h>
#include <stdbool.h>
// manejador de interrupciones DOS
extern void handle_int(uint8_t intnum);
void dosbox_run_until_exit(void);



// ======================================================
// Geometría del disco (floppy o HDD)
// ======================================================
typedef struct {
    bool     is_floppy;            // true = floppy, false = HDD
    uint16_t heads;                // número de cabezas
    uint16_t sectors_per_track;    // sectores por pista
    uint16_t cylinders;            // cilindros
    uint32_t hidden_sectors;       // 0 en floppy, 63 en HDD
} disk_geometry_t;
extern disk_geometry_t gDiskGeom;
extern void detect_geometry(uint32_t img_size);
#endif
