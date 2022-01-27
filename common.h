#ifndef _COMMON_H_
#define _COMMON_H_

#define ERR1 -1
#define ERR2 -2
#define ERR3 -3
#define ERR4 -4
#define ERR5 -5
#define ERR6 -6
#define ERR7 -7
#define ERR8 -8
#define ERR9 -9
#define ERR_EX -10
#define DONE -11		/* normal termination */

#define RAMSTART 0x8000u	/* starting address of the original RAM */
#define SCREEN_WIDTH 12
#define SCREEN_RAM_SIZE (SCREEN_WIDTH*8)

/* function prototypes */
unsigned int read16 (unsigned int addr);
void write16 (unsigned int addr, unsigned int data);
uint64_t read64 (unsigned int addr);
void write64 (unsigned int addr, uint64_t data);
unsigned int read_string (unsigned int from, unsigned int dst);

#endif
