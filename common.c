/* global variables, common routines, hardware independent module */

#include <stdint.h>
#include "common.h"
#include "main.h"


/* global variables */
extern uint8_t ram[RAMSIZE];
			/* little endian order of bytes in a 16-bit word,
			   big endian order of 16-bit words in a 64-bit word */
unsigned int pc;	/* BASIC program counter (index to the "ram" buffer),
			   R2 in the original code */
unsigned int datasp;	/* index to the data stack, SP in the original code */


unsigned int read16 (unsigned int addr)
{
  return (unsigned int) ram[addr-RAMSTART] |
	((unsigned int) ram[addr+1-RAMSTART] << 8); 
}


void write16 (unsigned int addr, unsigned int data)
{
  ram[addr-RAMSTART] = (uint8_t) data;
  ram[addr+1-RAMSTART] = (uint8_t) (data >> 8);
}


uint64_t read64 (unsigned int addr)
{
   return ((uint64_t) read16 (addr) << 48)
	| ((uint64_t) read16 (addr+2) << 32)
	| ((uint64_t) read16 (addr+4) << 16)
	| (uint64_t) read16 (addr+6);
}


void write64 (unsigned int addr, uint64_t data)
{
  write16 (addr, (unsigned int) (data >> 48));
  write16 (addr+2, (unsigned int) (data >> 32));
  write16 (addr+4, (unsigned int) (data >> 16));
  write16 (addr+6, (unsigned int) data);
}


/* read a string on the data stack to the address dst */
unsigned int read_string (unsigned int from, unsigned int dst)
{
  unsigned int src, len;

  src = read16 (from+2);	/* address of the string */
  len = read16 (from+4);	/* length of the string */
  if (len != 0)
  {
    if (ram[from-RAMSTART] == 0)
    {
/* first character of an ordinary string variable */
      src -= 8;
      ram[dst++-RAMSTART] = ram[src++-RAMSTART];
      ++src;			/* skip the identifier of a string variable */
      --len;
    }
/* remaining characters */
    while (len != 0)
    {
      ram[dst++-RAMSTART] = ram[src++-RAMSTART];
      --len;
    }
  }
  return dst;
}
