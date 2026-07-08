#ifndef DOSBOX_INT_H
#define DOSBOX_INT_H

//#include "../dosbox/stdint.h"
#include <stdint.h>
// manejador de interrupciones DOS
extern void handle_int(uint8_t intnum);
void dosbox_run_until_exit(void);


#endif
