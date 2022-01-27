/* hardware independent module */

#include <stdint.h>
#include <string.h>	/* memcpy, memset */
#include "arithm.h"
#include "basic.h"
#include "calc.h"
#include "common.h"
#include "editor.h"
#include "io.h"
#include "main.h"

extern uint8_t ram[RAMSIZE];
extern unsigned int pc;
extern unsigned int datasp;

/* private function prototypes */
void ram_init (void);
void item2str (unsigned int dst);
int l0d98 (unsigned int x);
int cmd_run (void);
int do_run (unsigned int x);
void l0e32 (void);
int cmd_list (void);
int do_list (unsigned int src);
int l0ed4 (void);
int cmd_auto (void);
void l0f8c (void);
int cmd_clear (void);
int cmd_test (void);
unsigned int ram_test (uint8_t pattern);
unsigned int find_prog_line (unsigned int n);
unsigned int str2int (void);
void clear_variables (void);
int eval2int (void);
int cmd_then (void);
void msg_errx (int error_code);
void msg_px (void);
int cmd_letc (void);
int cmd_defm (void);
int cmd_vac (void);
int cmd_mode (void);
int cmd_set (void);
void l1652 (void);
void l165c (void);
int cmd_end (void);
int cmd_print (void);
int cmd_stop (void);
void do_stop (void);
int cmd_csr (void);
int cmd_next (void);
int cmd_gosub (void);
int cmd_goto (void);
int cmd_return (void);
int cmd_if (void);
int cmd_for (void);
int cmd_input (void);
int do_input (void);
int cmd_drawc (void);
int cmd_draw (void);
int do_draw (int parameter);
int tokenize (unsigned int src, unsigned int dst);
int expand (unsigned int src, unsigned int dst, unsigned int pos);
unsigned int squeeze (void);
unsigned int tab822c_addr (void);
unsigned int prog_addr (unsigned int n);


/* 27BC: keyword table */
#define NKEYW 49
const char* keywords[NKEYW] = {
  "SIN ", "COS ", "TAN ", "ASN ",	/* codes 0xC0-0xC3 */
  "ACS ", "ATN ", "LOG ", "LN ",	/* codes 0xC4-0xC7 */
  "EXP ", "SQR ", "ABS ", "INT ",	/* codes 0xC8-0xCB */
  "SGN ", "FRAC ", "VAL ", "LEN ",	/* codes 0xCC-0xCF */
  "CHR ", "ASCI ", "RND ", "MID ",	/* codes 0xD0-0xD3 */
  "GETC ", "RAN#", "KEY", "CSR ",	/* codes 0xD4-0xD7 */
  "NEXT ", "GOTO ", "GOSUB ", "RETURN",	/* codes 0xD8-0xDB */
  "IF ", "FOR ", "PRINT ", "INPUT ",	/* codes 0xDC-0xDF */
  " THEN ", " TO ", " STEP ", "STOP",	/* codes 0xE0-0xE3 */
  "END", "LETC ", "DEFM ", "VAC",	/* codes 0xE4-0xE7 */
  "MODE ", "SET ", "DRAWC ", "DRAW ",	/* codes 0xE8-0xEB */
  "RUN ", "LIST ", "AUTO ", "CLEAR ",	/* codes 0xEC-0xEF */
  "TEST"				/* code 0xF0 */
};


/* 0626: RAM initialisation */
void ram_init (void)
{
  unsigned int x;

  write16 (0x8252 /*top of the RAM*/, RAMSTART+RAMSIZE);
  write16 (0x8250 /*number of variables*/, 26);
  clear_variables ();
/* 063C: initialise the user defined character */
  for (x=0x81ADu; x<0x81B4u; ++x)
  {
    ram[x-RAMSTART] = 0;
  }
/* 0648: clear the GOSUB and FOR stacks, and the ANS variable */
  for (x=0x81B4u; x<0x822Cu; ++x)
  {
    ram[x-RAMSTART] = 0;
  }
/* 0654: initialise the addresses of ends of BASIC programs 0-9 */
  for (x=0x822Cu; x<0x8240u; x+=2)
  {
    write16 (x, 0x826B);
  }
}


/* 0B40: reset */
void reset (void)
{
  unsigned int x;

  lcd (0xE8, 0);
  ram[0x00] = 0;
  lcd (0x80, 0);
  ram[0x08] = 0x11;
  lcd (0x88, 0x11);
  for (x=0x10; x<0x30; x+=8)
  {
    ram[x] = 0;
    lcd (0x80+x, 0);
  }
  ram[0x8268u /*selected program*/ - RAMSTART] = 0;
  ram[0x8267u /*number printing precision*/ - RAMSTART] = 0;
  ram[0x826Au-RAMSTART] = 0;
  ram[0x8256u /*mode*/ - RAMSTART] = 0;
  ram[0x8257u /*mode*/ - RAMSTART] = 0;
  mode0 ();
/* extra fix for the input line editor */
  write16 (0x8260, 0x816D);
  ram[0x816Du-RAMSTART] = 0;
/* variable 8258 stores the return point (return address in the original code)
 from the procedure 0E32=l0e32 to the procedure 166A=do_prog */
  ram[0x8258u-RAMSTART] = 0;
/* variable 8259 stores the code of not yet finished command PRINT or INPUT */
  ram[0x8259u-RAMSTART] = 0;
}


#define NDIR 12
/* 0BA6: direct mode main loop */
int do_direct (void)
{
/* 0DD2: */
  const int (*cmd_tab[NDIR]) (void) = {
    cmd_letc,	/* code 0xE5, command LETC */
    cmd_defm,	/* code 0xE6, command DEFM */
    cmd_vac,	/* code 0xE7, command VAC */
    cmd_mode,	/* code 0xE8, command MODE */
    cmd_set,	/* code 0xE9, command SET */
    cmd_drawc,	/* code 0xEA, command DRAWC */
    cmd_draw,	/* code 0xEB, command DRAW */
    cmd_run,	/* code 0xEC, command RUN */
    cmd_list,	/* code 0xED, command LIST */
    cmd_auto,	/* code 0xEE, command AUTO */
    cmd_clear,	/* code 0xEF, command CLEAR */
    cmd_test,	/* code 0xF0, command TEST */
  };
  int i;
  uint8_t x;
  unsigned int n;			/* R0, line number */
  unsigned int dst, len1, len2;		/* R1, R5, R3 */
  unsigned int addr;			/* 8248, address of the BASIC line */

/* 0x0BA6 */
  while (1)
  {
    datasp = 0x812C;
    ram[0x8256u-RAMSTART] &= ~0x30;
    ram[0x8257u-RAMSTART] &= ~0x02;
    ram[0x8000u-RAMSTART] &= ~0x06;
    lcd (0x80, ram[0x8000u-RAMSTART]);
    switch (input_line_editor ())
    {
      case 0:		/* [EXE] */
        break;

      case 1:		/* [S] and a digit */
/* 0454: */
        if ((ram[0x8256u-RAMSTART] & 0x80) != 0)
        {
          mode1 ();
        }
        else
        {
          i = do_run (1);
          if (i < 0 && i != DONE)
	  {
            write16 (0x8254, pc);
	    return i;
	  }
        }
        continue;

      case 2:		/* [MODE] [0] or [MODE] [1] */
        continue;

      case 3:		/* [init] */
        ram_init ();
        reset ();
        ram[0x8264u-RAMSTART] = 0;
        ram[0x8265u-RAMSTART] = 0;
        continue;

      default:
        write16 (0x8254, 0x816D);
        return ERR_EX;
    }

/* 0BC0: */
    if ((ram[0x8264u-RAMSTART] & 0x80) == 0)
    {
      if ((ram[0x8257u-RAMSTART] & 0x80) != 0)
      {
/* 0BCC: program execution mode, continue program execution after STOP */
        l0e32 ();
        i = do_prog ();
        write16 (0x825A, pc);
        if (i < 0 && i > DONE)
	{
	  return i;
	}
      }

      else if ((ram[0x8257u-RAMSTART] & 0x01) != 0)
      {
/* 0BD8: AUTO mode */
        l0f8c ();
      }

      continue;
    }

/* 0BE0: */
    clear_screen ();
    ram[0x824Cu-RAMSTART] = ram[0x8256u-RAMSTART];
    ram[0x824Du-RAMSTART] = ram[0x8257u-RAMSTART];
    ram[0x8264u-RAMSTART] = 0;
    i = tokenize (0x816D, 0x816D /*input line buffer*/);
    if (i < 0)
    {
      return i;
    }
    write16 (0x8240, pc);

/* 0BFA: */
    i = (int) (unsigned int) ram[pc-RAMSTART] - 0xE5;
    if (i >= 0)
    {
/* 0C02: */
      write16 (0x8254, pc);
      ++pc;
      if (i > NDIR-1)
      {
        write16 (0x8254, pc);
        return ERR_EX;
      }
      i = cmd_tab[i] ();
      if (i < 0)
      {
        return i;
      }
      continue;
    }

/* 0C0A: */
    if ((ram[0x8256u-RAMSTART] & 0x80) == 0)
    {
/* 0C10: result of calculation in run mode */
      i = eval (0);
      if (i != DONE)
      {
        return i;
      }
      if (ram[0x824Au-RAMSTART] != 0)
      {
        l165c ();
        continue;
      }

/* 0C1E: */
      item2str (0x816D /*input line buffer*/);
/* 0C60: */
      ram[0x8265u-RAMSTART] = 0;
      ram[0x8264u-RAMSTART] |= 0x40;	/* delay between scrollings required */
      if (display_string (0x816D) != 0)
      {
        ac ();
        continue;
      }
/* 0C72: */
      ram[0x8264u-RAMSTART] &= 0x20;
      continue;
    }

    else
    {
/* 0CA6: BASIC line number in wrt mode */
      n = str2int ();
      if (n == 0)
      {
/* 0CAC: number is equal 0 or has more than 4 digits */
        write16 (0x8254, pc);	/* position of an error */
        return ERR2;
      }

/* 0CB8: */
      if (ram[pc-RAMSTART] == 0)
      {
/* 0CBC: only the line number was typed - line deletion */
        ram[0x8257u-RAMSTART] &= ~0x01;	/* clear the AUTO mode */
        dst = find_prog_line (n);
        if (dst >= read16 (tab822c_addr ()) || read16 (dst) != n)
        {
/* 0CC8: line not found */
          l165c ();
          continue;
        }
/* 0CCC: delete the existing BASIC program line */
        len1 = 0;
/* 0CCE: len2 = length of the existing line */
        len2 = strlen (&ram[dst+2-RAMSTART]) + 3;
/* 0CDA: shrink the BASIC program */
        memmove (&ram[dst-RAMSTART], &ram[dst+len2-RAMSTART],
		read16 (0x823E /*end of BASIC programs*/) - dst - len2);
        len2 = -len2;
      }

      else
      {
/* 0CE8: line addition or change */
        len2 = strlen (&ram[pc-RAMSTART]) + 3;	/* length of the typed line */
        write16 (0x8246, n);			/* last line number */
        dst = find_prog_line (n);

        if (dst < read16 (tab822c_addr ()) && read16 (dst) == n)
        {
/* 0D10: line of the same number as the typed one already exists */
          addr = dst;
          dst += 2;		/* skip the line number */
          len1 = strlen (&ram[dst-RAMSTART]) + 3; /* length of the existing line */
/* 0D1E: is there enough RAM available? */
          if (len2 > len1 && len2 - len1 > free_ram ())
          {
            write16 (0x8254, 0x816D);
            return ERR1;	/* out of memory */
          }
          len1 -= len2;
/* 0D2A: */
          if (len1 == 0)
	  {
/* 0D2E: the typed line is of the same length as the existing one */
            strcpy (&ram[dst-RAMSTART], &ram[pc-RAMSTART]);
            i = l0d98 (addr);
	    if (i < 0)
            {
              write16 (0x8254, 0x816D);
              return i;
            }
            continue;
          }
/* 0D34: the typed line has a different length than the existing one */
          x = 1;		/* any non-zero value */
          while (ram[dst-RAMSTART] != 0)
          {
            x = ram[pc++-RAMSTART];
            ram[dst++-RAMSTART] = x;
            if (x == 0)
            {
/* 0D3C: the typed line is shorter than the existing one */
/* 0CCE: len2 = length of the existing line */
              len2 = strlen (&ram[dst-RAMSTART]) + 1;
/* 0CDA: shrink the BASIC program */
              memmove (&ram[dst-RAMSTART], &ram[dst+len2-RAMSTART],
			read16 (0x823E /*end of BASIC programs*/) - dst - len2);
              len2 = -len2;
              break;		/* with x=0 */
            }
          }
          if (x != 0)
          {
/* 0D54: the typed line is longer than the existing one */
            ram[dst++-RAMSTART] = ram[pc++-RAMSTART];
            n = 0;
            len2 = strlen (&ram[pc-RAMSTART]) + 1;
          }
        }

        else
	{
/* 0D3E: there's no existing line of the same number as the typed one */
          addr = dst;
          if (len2 > free_ram ())
          {
            write16 (0x8254 /*position of error*/, 0x816D /*input line buffer*/);
            return ERR1;
          }
        }

        if ((int) len2 > 0)
	{
/* 0D5E: expand the BASIC program */
          memmove (&ram[dst+len2-RAMSTART], &ram[dst-RAMSTART],
		read16 (0x823E) /*end of BASIC programs*/ - dst);
/* 0D6C: */
          if (n != 0)
          {
/* 0D70: copy the line number */
            write16 (dst, n);
            dst += 2;
          }
/* 0D76: copy the line body */
          strcpy (&ram[dst-RAMSTART], &ram[pc-RAMSTART]);
        }
      }

/* 0D7C: update the end addresses of BASIC programs */
      dst = tab822c_addr ();
      while (dst < 0x8240u)
      {
        write16 (dst, read16 (dst) + len2);
        dst += 2;
      }
/* 0D8E: */
      display_7seg (free_ram ());
      if (len1 == 0)
      {
/* 0CC8: */
        l165c ();
      }
      else
      {
        i = l0d98 (addr);
        if (i < 0)
        {
          write16 (0x8254, 0x816D);
          return i;
        }
      }
    }
  }
}


/* 0C1E: convert the item (floating point number or string) stored on the data
 stack to a printable form */
void item2str (unsigned int dst)
{
  int i;
  uint64_t x;

  if ((ram[datasp+1-RAMSTART] & 0x60) == 0)
  {
/* 0C24: calculated expression evaluated to a number */
/* store the number in the variable ANS at the address 0x8224 */
    x = read64 (datasp);
    write64 (0x8224, x);
/* 0C32 display the number */
    if ((ram[0x8257u-RAMSTART] & 0x80) == 0 /*direct mode*/ ||
	(ram[0x8256u-RAMSTART] & 0x01) != 0 /*STOP mode*/)
    {
      i = 0;
    }
    else
    {
      i = (int) (int8_t) ram[0x8267u-RAMSTART] /*number printing precision*/;
      if (i == 0)
      {
        i = -10;
      }
    }
    fp2str (x, i, &ram[dst-RAMSTART]);
    ram[0x8264u-RAMSTART] |= 0x20;	/* calculation result on the display */
  }
  else
  {
/* 0C7E: calculated expression evaluated to a string */
    ram[read_string (datasp, dst) - RAMSTART] = 0;
  }

  datasp += 8;
}


/* 0D98: */
int l0d98 (unsigned int x)
{
/* 0D9C: */
  if ((ram[0x8257u-RAMSTART] & 0x04) == 0)
  {
/* 0DCE: */
    return do_list (x);
  }

/* 0DA4: */
  if ((ram[0x824Du-RAMSTART] & 0x02) != 0)
  {
/* 0DAC: */
    x = read16 (0x8242);
    if (x != 0)
    {
      return do_list (find_prog_line (x));
    }
    mode1 ();
    return 0;
  }

/* 0DB8: */
  x += 2;		/* skip the line number */
/* 0DBA: search for the end of the line */
  while (ram[x++-RAMSTART] != 0)
  {
  }
/* 0DBE: test if address within selected program */
  if (x < read16 (tab822c_addr ()))
  {
/* 0DCE: */
    return do_list (x);
  }
/* 0DCA: */
  mode1 ();
  return 0;
}


/* 0DEC: code 0xEC, command RUN */
int cmd_run (void)
{
  unsigned int x;

  if (ram[pc-RAMSTART] == 0)
  {
/* 0E08: RUN from line 1 */
    x = 1;
  }
  else
  {
/* 0DF0: */
    x = str2int ();
    if (x == 0)
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
    if (ram[pc-RAMSTART] != 0)
    {
      write16 (0x8254, pc);
      return ERR2;
    }
    if (x > 9999)
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
  }
  return do_run (x);
}


/* 0E0C: run the program from the line x */
int do_run (unsigned int x)
{
  int i;

  pc = find_prog_line (x);
  if (pc >= read16 (tab822c_addr ()))
  {
/* 0E12: line not found */
    mode0 ();
    return 0;
  }

/* 0E16: clear the GOSUB and FOR stacks */
  for (x=0x81B4u-RAMSTART; x<0x81ECu-RAMSTART; ++x)
  {
    ram[x] = 0;
  }

/* 0E22: */
  write16 (0x825C /*address of current BASIC line*/, pc);
  pc +=2;			/* skip the line number */
  write16 (0x825A,pc);
  l0e32 ();
  i = do_prog ();
  write16 (0x825A,pc);
  return i;
}


/* 0E32: prepare the screen and the mode/flags system variables for RUN mode */
void l0e32 (void)
{
  unsigned int x;

  clear_screen ();
  ram[0x0008] &= ~0x03;		/* clear WRT segment */
  ++ram[0x0008];		/* show RUN segment */
  lcd (0x88, ram[0x0008]);
/* 0E46: clear the four 7-segment digits */
  for (x=0xB0; x<0xD0; x+=8)
  {
    lcd (x, 0);
  }
/* 0E56: display a 7-segment 'P' character */
  lcd (0xD0, 0x0E);
  lcd (0xD8, 0x06);
  ram[0x8256u-RAMSTART] &= ~0xB1;
  ram[0x8257u-RAMSTART] &= ~0x07;
  ram[0x8257u-RAMSTART] |= 0x80;	/* program execution mode */
  ram[0x8265u-RAMSTART] = 0x80;
  pc = read16 (0x825A);
}


/* 0E7A: code 0xED, command LIST */
int cmd_list (void)
{
  unsigned int x;

  x = 1;			/* default line number */
  if (ram[pc-RAMSTART] != 0)
  {
/* 0E82: */
    x = str2int ();
    if (x == 0)
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
    if (ram[pc-RAMSTART] != 0)
    {
      write16 (0x8254, pc);
      return ERR2;
    }
    if (x > 9999)
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
  }
  ram[0x8257u-RAMSTART] |= 0x04;
  ram[0x8257u-RAMSTART] &= ~0x01;	/* clear the AUTO mode */
  x = find_prog_line (x);
  if (x < read16 (tab822c_addr ()))
  {
    return do_list (x);
  }
/* 0EAC: line not found */
  if ((ram[0x8256u-RAMSTART] & 0x80) == 0)
  {
/* run mode */
    mode0 ();
  }
  else
  {
/* write mode */
    mode1 ();
  }
  return 0;
}


/* 0EB6: */
int do_list (unsigned int src)
{
  unsigned int dst;
  int i;

  do {
    dst = (uint8_t*) int2str (read16 (src), &ram[0x816Du-RAMSTART]) - ram;
    dst += RAMSTART;
    src += 2;
    ram[dst++-RAMSTART] = ' ';
    i = expand (src, dst, src);
    if (i < 0)
    {
      return i;
    }
    while (ram[src++-RAMSTART] != 0)
    {
    }
    if (l0ed4 () == 0)
    {
      return 0;
    }
/* 0F34: */
    if ((ram[0x8256u-RAMSTART] & 0x80) != 0)
    {
/* 0F2A: write mode */
      ram[0x8265u-RAMSTART] |= 0x80;
      return 0;
    }
/* 0F3A: run mode */
    delay_ms (2000);
  } while (src < read16 (tab822c_addr ()));
  mode0 ();
  return 0;
}


/* 0ED4: */
int l0ed4 (void)
{
  unsigned int x, dst;
  int i, cnt;

  dst = 0x816D;		/* input line buffer */
  ram[0x8265u-RAMSTART] = 0;
  cnt = SCREEN_WIDTH;
  if ((ram[0x8256u-RAMSTART] & 0x80) == 0)
  {
/* 0EE6: run mode */
    cnt = 0;
    ram[0x8264u-RAMSTART] = 0x40;
    ram[0x8265u-RAMSTART] = 0x80;
  }
/* 0EF8: */
  do {
    i = keys_stop_exe_ac ();
    if (i != 0)
    {
      ac ();
      return 0;
    }
    x = (unsigned int) ram[dst++-RAMSTART];
    if (x == 0)
    {
      break;
    }
    display_char (x);
  } while (--cnt != 0);
/* 0F06: */
  if (x != 0)
  {
    --ram[0x8269u /*cursor position*/ - RAMSTART];
  }
  write16 (0x8260, --dst);
/* 0F10: */
  if ((ram[0x8257u-RAMSTART] & 0x04) == 0)
  {
/* 0F18: 0B9C: */
    ram[0x8265u-RAMSTART] = 0;
    ram[0x8264u-RAMSTART] &= 0x20;
    return 0;
  }
/* 0F1C: */
  if ((ram[0x8257u-RAMSTART] & 0x01) != 0)
  {
/* 0F24: AUTO mode */
    ram[0x8257u-RAMSTART] &= ~0x04;
    ram[0x8265u-RAMSTART] |= 0x80;
    return 0;
  }
  return 1;
}


/* 0F52: code 0xEE, command AUTO */
int cmd_auto (void)
{
  unsigned int x;

  write16 (0x8254, read16 (0x8240));
  if ((ram[0x8256u-RAMSTART] & 0x80) == 0)
  {
    return ERR2;		/* run mode */
  }

/* 0F5C: get the line number increment step */
  x = 10;			/* default */ 
  if (ram[pc-RAMSTART] != 0)
  {
/* 0F64: */
    x = str2int ();
    if (x == 0)
    {
      return ERR5;
    }
    if (ram[pc-RAMSTART] != 0)
    {
      return ERR2;
    }
    if (x > 9999)
    {
      return ERR5;
    }
  }
  write16 (0x8244, x);

/* 0F80: get the last line number */
  (void) find_prog_line (10000);
  ram[0x8246u-RAMSTART] = ram[0x8242u-RAMSTART];
  ram[0x8247u-RAMSTART] = ram[0x8243u-RAMSTART];

  l0f8c ();
  return 0;
}


/* 0F8C: */
void l0f8c (void)
{
  *int2str (read16 (0x8246) + read16 (0x8244), &ram[0x816Du-RAMSTART]) = 0;
  ram[0x8264u-RAMSTART] = 0;
  ram[0x8257u-RAMSTART] |= 0x05;
/* 0FAA: */
  (void) l0ed4 ();
}


/* 0FAC: code 0xEF, command CLEAR */
int cmd_clear (void)
{
  unsigned int x, addr1, addr2, size;

  if ((ram[0x8256u-RAMSTART] & 0x80) == 0)
  {
    write16 (0x8254, read16 (0x8240));
    return ERR2;		/* run mode */
  }
  if (ram[pc-RAMSTART] == 0)
  {
/* 0FD4: erase the selected program */
    x = tab822c_addr ();
/* begin address of the selected BASIC program */
    addr1 = prog_addr (ram[0x8268u /*selected program*/ - RAMSTART]);
/* end address of the selected BASIC program */
    addr2 = read16 (x);
/* number of bytes to move */
    size = read16 (0x823E /*end of BASIC programs*/) - addr2;
    memmove (&ram[addr1-RAMSTART], &ram[addr2-RAMSTART], (size_t) size);
    addr2 -= addr1;
/* 0FFA: update the end addresses of BASIC programs */
    while (x < 0x8240u)
    {
      write16 (x, read16 (x) - addr2);
      x += 2;
    }
    mode1 ();
    return 0;
  }
/* 0FBA: */
  if (ram[pc-RAMSTART] == 'A' && ram[pc+1-RAMSTART] == 0)
  {
/* 0FC4: CLEARA - erase all programs */
    for (x=0x822Cu; x<0x8240u; x+=2)
    {
      write16 (x, 0x826B);
    }
    mode1 ();
    return 0;
  }
  write16 (0x8254, pc);
  return ERR2;
}


#define NTEST 10
/* 1006: code 0xF0, command TEST */
int cmd_test (void)
{
  const struct {
    uint8_t addr;		/* LCD controller port address */
    uint8_t data;		/* data byte */
  } tab114f[NTEST] = {
    { 0x80, 0x01 },		/* EXT */
    { 0x80, 0x02 },		/* S */
    { 0x80, 0x04 },		/* F */
    { 0x88, 0x01 },		/* RUN */
    { 0x88, 0x02 },		/* WRT */
    { 0x88, 0x10 },		/* DEG */
    { 0x98, 0x01 },		/* RAD */
    { 0xA0, 0x01 },		/* GRA */
    { 0xA8, 0x01 },		/* TR */
    { 0xD8, 0x08 }		/* STOP */
  };
  unsigned int x, y;
  uint8_t z;

/* clear the screen */
  for (x=0x80; x<0xE0; ++x)
  {
    lcd (x, 0);
  }
  lcd (x, SCREEN_WIDTH);

/* 103C: test the RAM */
  if (	(ram_test (0xFF) != RAMSIZE) ||
	(ram_test (0xAA) != RAMSIZE) ||
	(ram_test (0x55) != RAMSIZE) ||
	(ram_test (0x00) != RAMSIZE)	)
  {
    ram [0x8264u-RAMSTART] = 0;
    ram [0x8265u-RAMSTART] = 0;
/* "Defekt OZU" */
    display_char (0xA4);
    display_char (0x85);
    display_char (0x86);
    display_char (0x85);
    display_char (0x8B);
    display_char (0x94);
    display_char (' ');
    display_char (0xAF);
    display_char (0xBA);
    display_char (0xB5);
/* wait until EXE key pressed */
    do {
      key_wait ();
    } while (keyb0770 () != 4);
  }

/* 1066: test the mode signs of the LCD */
  for (x=0; x<NTEST; ++x)
  {
    lcd (tab114f[x].addr, tab114f[x].data);
    delay_ms (1250);
    lcd (tab114f[x].addr, 0);
  }

/* 1084: test the four 7-segment digits by displaying 1111, 2222, ... 9999 */
  for (x=1111; x<10000; x+=1111)
  {
    display_7seg (x);
    delay_ms (1250);
  }

/* 109A: test the rows of the dot matrix area */
  for (y=0x80; y<0x88; ++y)
  {
/* 10A2: activate all segments of the selected row */
    for (x=y; x<0xE0; x+=8)
    {
      lcd (x, 0x1F);
    }
    delay_ms (1250);
/* 10B4: clear all segments of the selected row */
    for (x=y; x<0xE0; x+=8)
    {
      lcd (x, 0);
    }
  }

/* 10C4: test the columns of the LCD */
  for (y=0x81; y<0xE0; y+=8)
  {
/* patterns 0x01, 0x02, 0x04, 0x08, 0x10 activate consecutive columns,
 pattern 0x20 clears the character field (equivalent to 0x00, because the three
 most significant bits are ignored) */
    z = 0x01;
    while (1)
    {
      for (x=y; (x & 0x07) != 0; ++x)
      {
        lcd (x, z);
      }
      if (z == 0x20)
      {
        break;
      }
      z <<= 1;
      delay_ms (350);
    }
  }

  ram_init ();
  reset ();
  ram[0x8264u-RAMSTART] = 0;
  ram[0x8265u-RAMSTART] = 0;
  return 0;
}


/* 1050: test the RAM with a specified pattern, returns the last tested address,
 RAMSIZE if test passed */
unsigned int ram_test (uint8_t pattern)
{
  unsigned int addr; 
/* 1050: fill the RAM with the test pattern */
  for (addr=0; addr<RAMSIZE; ++addr)
  {
    ram[addr] = pattern;
  }
/* 1056: compare all RAM locations against the test pattern */
  for (addr=0; addr<RAMSIZE; ++addr)
  {
    if (ram[addr] != pattern)
    {
      break;
    }
  }
  return addr;
}


/* 1182: returns an address of a BASIC line with the nearest line number (which
 may be equal or greater than the requested one), or the end address of the
 BASIC program if not found,
 also returns in the RAM location 0x8242 the number of the previous line */
unsigned int find_prog_line (unsigned int n /*requested BASIC line number, R0*/)
{
  unsigned int addr1 /*R1*/, addr2 /*8248*/, x;

/* 1190: begin address of the selected BASIC program */
  addr1 = prog_addr (ram[0x8268u /*selected program*/ - RAMSTART]);
/* 119A: end address of the selected BASIC program */
  addr2 = read16 (tab822c_addr ());
  write16 (0x8242, 0);
/* 11A4: */
  while (addr1 < addr2)
  {
    x = read16 (addr1);
    if (x >= n)
    {
      break;		/* found a line number greater or equal requested */
    }
/* 11B2: search for the end of the line */
    addr1 += 2;		/* skip the line number */
    while (ram[addr1++-RAMSTART] != 0)
    {
    }
    write16 (0x8242, x);
  }
/* 11BE: line not found */
  return addr1;
}


/* 11D6: convert to binary an integer decimal number in ASCII format pointed to
 by pc */
unsigned int str2int (void)
{
  int cnt;
  unsigned int x, y;

  y = 0;
  for (cnt=0; cnt<4; ++cnt)
  {
    x = (unsigned int) ram[pc-RAMSTART];
    if (x < (unsigned int) '0' || x > (unsigned int) '9')
    {
      return y;
    }
    ++pc;
    y *= 10;
    y += x - (unsigned int) '0';
  }
  return 32767;		/* more than 4 digits encountered */
}


/* 1208: clear all variables */
void clear_variables (void)
{
  unsigned int x;

  x = read16 (0x8250) /*number of variables*/ * 8;
  memset (&ram[read16 (0x8252) /*top of the RAM*/ - x - RAMSTART], 0, x);
  ram[0x814Eu-RAMSTART] = 0;	/* clear the special string variable $ */
}


/* 1220: evaluate an expression to a positive integer number or return
 a negative error code */
int eval2int (void)
{
  int i;

  i = eval (0x10);
  if (i != DONE)
  {
    return i;
  }
  write16 (0x8254, read16 (0x8240));
  if ((ram[datasp+1-RAMSTART] & 0x60) != 0)
  {
    return ERR5;	/* not a number */
  }
  i = (int) fp2int (read64 (datasp));
  datasp += 8;
  if (i == -32768)
  {
    return ERR5;
  }
  return (i < 0) ? DONE : i;
}


/* 12B4: codes 0xE0..0xE2, statements THEN/TO/STEP, handled elsewhere */
int cmd_then (void)
{
  write16 (0x8254, read16 (0x8240));
  return ERR2;
}


/* 12D4: error handler,
 not used in the INPUT mode, unlike in the original code */
void error_handler (int error_code)
{
  int i;
  unsigned int src;	/* R5 */
  unsigned int dst;	/* = input line buffer 0x816D, R4 */
  unsigned int pos;	/* R1, position of an error/cursor in src when invoked
			 by the ANS key */
  uint8_t x, y;

  msg_errx (error_code);

/* 12F0: */
  if ( (ram[0x8257u-RAMSTART] & 0x80) != 0 &&
	(ram[0x8256u-RAMSTART] & 0x01) == 0)
  {
/* 12FA: direct mode and not STOP mode */
    ram[0x50] = 0;
    ram[0x58] = 0;
    lcd (0xD0, 0);
    lcd (0xD8, 0);
    ++ram[0x8269u /*cursor position*/ - RAMSTART];
    msg_px ();
  }

/* 1340: */
  while (1)
  {
    i = keyb0770 ();
    if (i == 7 /*AC*/)
    {
/* 134A: */
      ram[0x8256u-RAMSTART] &= ~0x31;
      ram[0x8257u-RAMSTART] &= ~0x87;
      ac ();
/* 0BA0: */
      ram[0x8264u-RAMSTART] &= 0x20;
      return;
    }
/* 1354: */
    if (i == 9 /*ANS*/)
    {
/* 137E: show the position of the error */
      ram[0x8264u-RAMSTART] = 0;
      ram[0x8269u-RAMSTART] = 0;
      dst = 0x816D;		/* input line buffer */
      pos = read16 (0x8254 /*position of the error*/);
      if ( (ram[0x8257u-RAMSTART] & 0x80) == 0 ||
	(ram[0x8256u-RAMSTART] & 0x01) != 0)
      {
/* 13CC: program execution mode or STOP mode */
        src = 0x8060;
        strcpy (&ram[src-RAMSTART], &ram[dst-RAMSTART]);
        pos -= dst;
      }
      else
      {
/* 1394: direct mode and not STOP mode */
        src = read16 (0x825C /*address of current BASIC line*/);
        dst = (uint8_t*) int2str (read16 (src), &ram[dst-RAMSTART]) - ram;
        dst += RAMSTART;
        ram[dst++-RAMSTART] = ' ';
        src += 2;			/* skip the line number */
        pos -= src;
        ram[0x08] &= ~0x01;		/* segment RUN */
        ram[0x08] |= 0x02;		/* segment WRT */
	lcd (0x88, ram[0x08]);
        ram[0x8256u-RAMSTART] &= ~0xB1;
        ram[0x8257u-RAMSTART] &= ~0x87;
        ram[0x8256u-RAMSTART] |= 0x80;	/* select write mode */
      }
      break;
    }
  }

/* 13DE: */
  i = expand (src, dst, pos);
  if (i >= 0)
  {
    pos = (unsigned int) i;
  }
/* 13E2: display the characters up to the error position */
  dst = 0x816D;			/* input line buffer */
  do {
    x = ram[dst++-RAMSTART];
    if (x == 0)
    {
      break;
    }
    display_char (x);
  } while (pos-- != 0);
/* 141E: */
  write16 (0x8260, dst-1);

  if (x != 0)
  {
/* 13FC: there are some remaining characters left,
 save the position of the error */
    y = ram[0x8269u-RAMSTART];
/* 1404: display the remaining characters up to the end of the screen */
    while (ram[0x8269u-RAMSTART] < SCREEN_WIDTH &&
	(x = ram[dst++-RAMSTART]) != 0)
    {
      display_char (x);
    }
/* 1424: cursor at the position of the error */
    ram[0x8269u-RAMSTART] = y;
  }
  else
  {
/* 1416: */
    display_char (' ');
  }

/* 142E: */
  --ram[0x8269u-RAMSTART];
  ram[0x8265u-RAMSTART] |= 0x80;
  if ((ram[0x8256u-RAMSTART] & 0x80) != 0)
  {
/* 1442: write mode */
    display_7seg (free_ram ());
  }
}


/* 12D4: */
void msg_errx (int error_code)
{
  ram[0x8264u-RAMSTART] = 0;
  ram[0x8265u-RAMSTART] = 0;
  display_char ('E');
  display_char ('R');
  display_char ('R');
  display_char ((error_code == ERR_EX) ? '!' : '0' - error_code);
}


/* 130E: */
void msg_px (void)
{
  display_char ('P');
  display_char ('0' + ram[0x8268u /*selected program*/ - RAMSTART]);
  display_char ('-');
  *int2str (read16 (read16 (0x825C /*current BASIC line address*/)),
		&ram[0x816Du-RAMSTART]) = 0;
  display_short_string (0x816D);
}


/* 144E: code 0xE5, statement LETC */
int cmd_letc (void)
{
  int i;
  unsigned int src, dst;
  uint8_t x;

  i = eval (0x10);
  if (i != DONE)
  {
    return i;
  }
  write16 (0x8254, read16 (0x8240));
  if ((ram[datasp+1-RAMSTART] & 0x40) == 0)
  {
    return ERR5;		/* not a string */
  }
  dst = 0x81AD;			/* user defined character */
  src = read16 (datasp+2);	/* address of the string */
  i = (int) read16 (datasp+4);	/* length of the string */
  if (i != 0)
  {
    if (i > 7)
    {
      return ERR5;
    }
/* 1476: special treatment of the first byte */
    if (ram[datasp-RAMSTART] == 0)
    {
/* 147A: string variable */
      src -= 8;
      x = ram[src++-RAMSTART];
      ++src;			/* skip the identifier of a string variable */
    }
    else
    {
/* 1484: string literal or a special string variable '$' */
      x = ram[src++-RAMSTART];
    }
/* 1486: remaining bytes */
    do {
      if (x < '0')
      {
        return ERR5;
      }
      x -= '0';
      if (x > 9)
      {
        if (x < 'A' - '0' || x > 'V' - '0')
        {
          return ERR5;
        }
        x -= 7;
      }
/* 14A2: reverse the order of bits */
      ram[dst++-RAMSTART] = ((x & 0x01) << 4) | ((x & 0x02) << 2) | (x & 0x04) |
		((x & 0x08) >> 2) | ((x & 0x10) >> 4);
      if (--i != 0)
      {
        x = ram[src++-RAMSTART];
      }
    } while (i != 0);
  }
/* 14B2: clear the uninitialised rows of the user defined character */
  while (dst < 0x81B4u)
  {
    ram[dst++-RAMSTART] = 0;
  }
  datasp += 8;
  l1652 ();
  return 0;
}


/* 14C4: code 0xE6, statement DEFM */
int cmd_defm (void)
{
  int i;

  if ((ram[0x8257u /*mode*/ - RAMSTART] & 0x80) != 0 && ram[pc-RAMSTART] == ',')
  {
/* 14D0: undocumented syntax in the program execution mode: DEFM,variable_name
 returns the maximal number of additional variables */
    ++pc;
    i = eval (0x20);
    if (i != DONE)
    {
      return i;
    }
    write16 (0x8254, read16 (0x8240));
    if ((ram[datasp+1-RAMSTART] & 0x20) == 0)
    {
      return ERR5;	/* not a numberic variable */
    }
/* 14E0: write the maximal number of additional variables to the specified
 variable */
    write64 (read16 (datasp+2) - 8,
	int2fp ((int16_t) (uint16_t) (free_ram () / 8)) );
    datasp += 8;
  }

  else
  {
/* 1522: */
    if ((ram[0x8257u /*mode*/ - RAMSTART] & 0x80) != 0 || ram[pc-RAMSTART] != 0)
    {
/* 1526: DEFM with an argument (obligatory in the program execution mode) */
      i = eval2int ();
      if (i < 0)
      {
        return (i == DONE) ? ERR5 : i;
      }
      write16 (0x8254, read16 (0x8240));
      if (i > 4121)
      {
        return ERR5;
      }
/* 153C: test if there's enough memory for variables */
      i += 26;
      if ((unsigned int) i * 8 > read16 (0x8252 /*top of the RAM*/) -
	read16 (0x823E /*end of BASIC programs*/))
      {
        return ERR1;
      }
      write16 (0x8250 /*number of variables*/, (unsigned int) i);
    }
/* 1556: */
    if ((ram[0x8257u /*mode*/ - RAMSTART] & 0x80) == 0)
    {
/* 155C: */
      ram[0x8264u-RAMSTART] = 0;
      ram[0x8265u-RAMSTART] = 0;
      memcpy (&ram[0x816Du /*input line buffer*/ - RAMSTART], "***VAR: ", 8);
      *int2str (read16 (0x8250), &ram[0x8175-RAMSTART]) = 0;
      display_short_string (0x816D);
      if ((ram[0x8256u /*mode*/ - RAMSTART] & 0x80) != 0)
      {
/* 1584: write mode */
        display_7seg (free_ram ());
      }
      ram[0x8265u-RAMSTART] = 0;
    }
  }
  l1652 ();
  return 0;
}


/* 1598: code 0xE7, statement VAC */
int cmd_vac (void)
{
  clear_variables ();
  l1652 ();
  return 0;
}


/* 15A0: code 0xE8, statement MODE */
int cmd_mode (void)
{
  switch (ram[pc-RAMSTART])
  {
    case '4':		/* DEG */
      ++pc;
      mode4 ();
      l1652 ();
      return 0;

    case '5':		/* RAD */
      ++pc;
      mode5 ();
      l1652 ();
      return 0;

    case '6':		/* GRA */
      ++pc;
      mode6 ();
      l1652 ();
      return 0;

    default:		/* invalid mode */
      write16 (0x8254, read16 (0x8240));
      return ERR5;
  }
}


/* 1630: code 0xE9, statement SET */
int cmd_set (void)
{
  int i;

  if (ram[pc-RAMSTART] == 'N')
  {
    ++pc;
    i = 0;
  }
  else
  {
    i = eval2int ();
    if (i < 0)
    {
      return (i == DONE) ? ERR5 : i;
    }
    if (i > 10)
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
  }
  ram[0x8267u-RAMSTART] = (uint8_t) (unsigned int) -i;
  l1652 ();
  return 0;
}


/* 1652: */
void l1652 (void)
{
  if ((ram[0x8257u-RAMSTART] & 0x80) == 0 ||
	(ram[0x8256u-RAMSTART] & 0x01) != 0)
  {
/* 165C: direct or STOP mode */
    l165c ();
  }
}


/* 165C: */
void l165c (void)
{
/* initialise the input line buffer */
  write16 (0x8160, 0x816D);
  ram[0x816Du-RAMSTART] = 0;
/* 0BA0: */
  ram[0x8264u-RAMSTART] &= 0x20;
}


#define NCMD 21
/* 166A: main loop of the BASIC program execution, returns a negative error
 code if something went wrong, otherwise zero */
int do_prog (void)
{
/* 16E2: */
  const int (*cmd_tab[NCMD]) (void) = {
    cmd_csr,	/* code 0xD7, statement CSR */
    cmd_next,	/* code 0xD8, statement NEXT */
    cmd_goto,	/* code 0xD9, statement GOTO */
    cmd_gosub,	/* code 0xDA, statement GOSUB */
    cmd_return,	/* code 0xDB, statement RETURN */
    cmd_if,	/* code 0xDC, statement IF */
    cmd_for,	/* code 0xDD, statement FOR */
    cmd_print,	/* code 0xDE, statement PRINT */
    cmd_input,	/* code 0xDF, statement INPUT */
    cmd_then,	/* code 0xE0, statement THEN */
    cmd_then,	/* code 0xE1, statement TO */
    cmd_then,	/* code 0xE2, statement STEP */
    cmd_stop,	/* code 0xE3, statement STOP */
    cmd_end,	/* code 0xE4, statement END */
    cmd_letc,	/* code 0xE5, statement LETC */
    cmd_defm,	/* code 0xE6, statement DEFM */
    cmd_vac,	/* code 0xE7, statement VAC */
    cmd_mode,	/* code 0xE8, statement MODE */
    cmd_set,	/* code 0xE9, statement SET */
    cmd_drawc,	/* code 0xEA, statement DRAWC */
    cmd_draw	/* code 0xEB, statement DRAW */
  };
  int i;

  while (1)
  {
    if (ram[0x8258u-RAMSTART] == 0)
    {
      if (ram[pc-RAMSTART] == ':')
      {
        ++pc;
      }
      if (ram[pc-RAMSTART] == '!')
      {
/* 167C: comment encountered, skip the rest of the BASIC line */
        while (ram[++pc-RAMSTART] != 0)
        {
        }
      }
      if (ram[pc-RAMSTART] == 0)
      {
        ++pc;
/* 1680: next BASIC line */
        write16 (0x825C, pc);	/* address of current BASIC line */
        pc += 2;		/* skip the line number */
      }
/* 1688: STOP or EXE key pressed? */
      if ((*(volatile uint8_t*) &ram[0x8265u-RAMSTART] & 0x03) != 0)
      {
        ram[0x8258u-RAMSTART] = 1;
        return cmd_stop ();
      }
      else if ((ram[0x8265u-RAMSTART] & 0x04) != 0)
      {
/* suspend the program execution until the [EXE] key is pressed, for example
 after the PRINT statement without a semicolon */
        ram[0x8258u-RAMSTART] = 1;
        do_stop ();
        return 0;
      }
    }
    else
    {
      --ram[0x8258u-RAMSTART];
    }

    if (ram[0x8258u-RAMSTART] == 0)
    {
/* 169A: test for the end of the program */
      if (pc >= read16 (tab822c_addr ()))
      {
        return cmd_end ();
      }
/* 16A6: trace mode? */
      if ((ram[0x8256u-RAMSTART] & 8) != 0)
      {
        ram[0x8258u-RAMSTART] = 2;
        return cmd_stop ();
      }
    }
    else
    {
      ram[0x8258u-RAMSTART] = 0;
    }

/* 16B6: */
    write16 (0x8240, pc);
    i = (int) (unsigned int) ram[0x8259u-RAMSTART];
    ram[0x8259u-RAMSTART] = 0;
    if (i == 0)
    {
      i = (int) (unsigned int) ram[pc-RAMSTART];
    }
    else
    {
      --pc;
    }
    i -= 0xD7;
    if (i < 0)
    {
/* 16D4: functions (codes C0 to D6) and expressions */
      i = eval (0);
      if (i != DONE)
      {
        return i;
      }
      if (ram[0x824Au-RAMSTART] == 0)
      {
        write16 (0x8254, pc);
        return ERR2;
      }
    }
    else if (i < NCMD)
    {
/* 16CC: commands (codes D7 to EB) */
      ++pc;
      i = cmd_tab[i] ();
      if (i < 0)
      {
        return i;
      }
    }
    else
    {
/* 16C8: */
      write16 (0x8254, pc);
      return ERR2;
    }
  }
}


/* 170C: code 0xE4, statement END */
int cmd_end (void)
{
  ram[0x50] = 0;
  ram[0x58] = 0;
  lcd (0xD0, 0);
  lcd (0xD8, 0);
  ram[0x8256u-RAMSTART] &= ~0xB1;
  ram[0x8257u-RAMSTART] &= ~0x87;
  ram[0x8264u-RAMSTART] = 0;
  l165c ();
  return DONE;
}


/* 1728: code 0xDE, statement PRINT */
int cmd_print (void)
{
  int i;
  uint8_t x;

  i = eval (0x10);
  if (i != DONE)
  {
    return i;
  }

/* 0C1E: */
  item2str (0x816D /*input line buffer*/);
/* 0C60: */
  ram[0x8265u-RAMSTART] = 0;
  ram[0x8264u-RAMSTART] |= 0x40;	/* delay between scrollings required */
  if (display_string (0x816D) != 0)
  {
    ac ();
    return DONE;
  }

/* 174C: */
  if (ram[pc-RAMSTART] == ';')
  {
/* 1752: */
    ++pc;
    x = ram[pc-RAMSTART];
    if (x != 0 && x != ':')
    {
      ram[0x8259u-RAMSTART] = 0xDE;	/* code of the PRINT statement */
    }
  }
  else if (ram[pc-RAMSTART] == ',')
  {
/* 1788: */
    ++pc;
    ram[0x8264u-RAMSTART] &= ~0x80;	/* display should be cleared before printing */
    ram[0x8259u-RAMSTART] = 0xDE;
    ram[0x8265u-RAMSTART] |= 0x04;	/* force stop */
  }
  else
  {
    ram[0x8264u-RAMSTART] &= ~0x80;	/* display should be cleared before printing */
    ram[0x8265u-RAMSTART] |= 0x04;	/* force stop */
  }
  return 0;
}


/* 1794: code 0xE3, statement STOP */
int cmd_stop (void)
{
  ram[0x8264u-RAMSTART] = 0;
  if ((ram[0x8265u-RAMSTART] & 0x80) != 0)
  {
/* 17A8: */
    ram[0x8265u-RAMSTART] = 0;
    msg_px ();
  }
/* 17DE: */
  do_stop ();
  return 0;
}


void do_stop (void)
{
  ram[0x50] = 0;
  ram[0x58] = 0x08;		/* show the STOP segment */
  lcd (0xD0, 0);
  lcd (0xD8, 0x08);
  ++ram[0x8256u-RAMSTART];	/* set STOP mode (set bit 0) */
/* 0B9C: */
  ram[0x8265u-RAMSTART] = 0;
  ram[0x8264u-RAMSTART] &= 0x20;
}


/* 17FA: code 0xD7, statement CSR */
int cmd_csr (void)
{
  int i;
  int flag;	/* boolean variable, 8258 in the original code */
  unsigned int dst;

  flag = (ram[pc-RAMSTART] == ',');
  if (flag)
  {
    ++pc;
    i = (int) (unsigned int) ram[0x8269u /*cursor position*/ - RAMSTART];
/* 180E: */
    if (ram[pc-RAMSTART] == 0 || ram[pc-RAMSTART] == ':')
    {
/* 1838: case CSR , */
      dst = (unsigned int) i << 3;
      i = SCREEN_WIDTH - i;
    }
  }

  if (!flag || (ram[pc-RAMSTART] != 0 && ram[pc-RAMSTART] != ':'))
  {
/* 1818: */
    i = eval2int ();
    if (i < 0)
    {
      return (i == DONE) ? ERR5 : i;
    }
    if (i >= SCREEN_WIDTH)
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
    ram[0x8269u /*cursor position*/ - RAMSTART] = (uint8_t) (unsigned int) i;

/* 1828: */
    if (ram[pc-RAMSTART] == ',')
    {
/* 182E: */
      ++pc;
      if (flag)
      {
/* 186A: case CSR ,i, */
        i = 0;
      }
/* 1838: case CSR i, */
      dst = (unsigned int) i << 3;
      i = SCREEN_WIDTH - i;
    }
    else
    {
/* 1864: */
      dst = 0;
      if (!flag)
      {
/* 186A: case CSR ,i */
        i = 0;
      }
/* case CSR i */
    }
  }

/* 184A: clear 'i' characters starting from the LCD address 'dst' */
  while (i != 0)
  {
    ++dst;
    do {
      ram[dst] = 0;
      lcd (dst+0x80, 0);
    } while ((++dst & 7) != 0);
    --i;
  }

/* 185C: */
  ram[0x8264u-RAMSTART] |= 0x80;
  return 0;
}


/* 1874: code 0xD8, statement NEXT */
int cmd_next (void)
{
  int i;
  unsigned int x;	/* address of the FOR stack entry */
  unsigned int y;	/* address of the control variable */
  uint64_t v, step;

  i = eval (0x20);
  if (i != DONE)
  {
    return i;
  }
  write16 (0x8254, read16 (0x8240));
  if ((ram[datasp+1-RAMSTART] & 0x20) == 0)	/* numeric variable expected */
  {
    return ERR5;		/* invalid control variable */
  }
  y = read16 (datasp+2);	/* address of the control variable */
  datasp += 8;

/* 188E: scan the FOR stack for matching control variable */
  for (x=0x81E6u; x<0x8236u; x+=20)
  {
    if (y == read16 (x))
    {
/* 18A4: matching variable found */
      y -= 8;
      step = read64 (x-18);
      v = fp_add (read64 (y) /*control variable*/, step);
      if (v == NOT_A_NUMBER)
      {
        return ERR3;
      }
      write64 (y, v);		/* update the control variable */
      v = fp_sub (v, read64 (x-10) /*TO*/);
      if (v == NOT_A_NUMBER)
      {
        return ERR3;
      }
      if (v == 0 || ((v ^ step) & 0x8000000000000000) != 0)
      {
/* 18DA: perform the next FOR-NEXT iteration */
        pc = read16 (x-2);	/* address of the NEXT loop */
      }
      else
      {
/* 18F0: free the current FOR stack entry and leave the FOR-NEXT loop */
        ram[x-RAMSTART] = 0;
        ram[x+1-RAMSTART] = 0;
      }
      return 0;
    }
  }
/* 18A0: no matching NEXT statement found */
  return ERR7;
}


/* 1910: code 0xDA, statement GOSUB */
int cmd_gosub (void)
{
  int i;
  unsigned int x;

/* look for a free entry on the first GOSUB stack (for return addresses) */
  for (x=0x81B4u-RAMSTART; x<0x81C4u-RAMSTART; x+=2)
  {
    if (ram[x] == 0 && ram[x+1] == 0)
    {
/* 1928: push the address of current BASIC line on the second GOSUB stack */
      ram[x+16] = ram[0x825Cu-RAMSTART];
      ram[x+17] = ram[0x825Du-RAMSTART];
/* 192E: execute GOTO */
      i = cmd_goto ();
      if (i < 0)
      {
        return i;
      }
/* 1932: push the old BASIC program counter on the first GOSUB stack */
      ram[x] = ram[0x825Au-RAMSTART];
      ram[x+1] = ram[0x825Bu-RAMSTART];
      return 0;
    }
  }
  write16 (0x8254, read16 (0x8240));
  return ERR7;		/* GOSUB stack oferflow */
}


/* 193C: code 0xD9, statement GOTO */
int cmd_goto (void)
{
  int i;
  unsigned int x;

  x = 0;
  if (ram[pc-RAMSTART] == '#')
  {
/* 1946: go to another program */
    ++pc;
    i = eval2int ();
    if (i < 0)
    {
      return (i == DONE) ? ERR5 : i;
    }
    if (i > 9)
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
    ram[0x8268u-RAMSTART] = (uint8_t) (unsigned int) i;	/* selected program */
    if (ram[pc++-RAMSTART] != ',')
    {
/* 1978: no line number specified, go to the beginning of another program */
      --pc;
      x = prog_addr (i);
    }
  }

  if (x == 0)
  {
/* 1962: get the line number */
    i = eval2int ();
    if (i < 0)
    {
      return (i == DONE) ? ERR5 : i;
    }
    write16 (0x8254, read16 (0x8240));
    if (i > 9999)
    {
      return ERR5;
    }
    x = find_prog_line ((unsigned int) i);
    if (x >= read16 (tab822c_addr ()) /*line not found*/ ||
		read16 (x) != (unsigned int) i /*no exact match*/)
    {
      return ERR4;
    }
  }

/* 1986: */
  write16 (0x825A, pc);		/* old BASIC program counter */
  pc = x;			/* new BASIC program counter */
/* The original procedure returns to the address 1680 in the program execution
 loop. This can be achieved by decrementing the pc, so that it points to the
 terminating zero byte of the previous BASIC line. This method requires the
 first BASIC program to be preceded by a zero byte (at the otherwise unused RAM
 location 826A). */
  --pc;
  return 0;
}


/* 198E: code 0xDB, statement RETURN */
int cmd_return (void)
{
  unsigned int x;

/* search for the first non-zero entry on the first GOSUB stack */
  for (x=0x81C2u; x>=0x81B4u; x-=2)
  {
    pc = read16 (x);
    if (pc != 0)
    {
/* 19A0: free the first GOSUB stack entry */
      ram[x-RAMSTART] = 0;
      ram[x+1-RAMSTART] = 0;
/* 19A2: get the address of current BASIC line from the second GOSUB stack */
      ram[0x825Cu-RAMSTART] = ram[x+16-RAMSTART];
      ram[0x825Du-RAMSTART] = ram[x+17-RAMSTART];
/* 19A8: return may occur to another program */
      x = 0x822C;	/* table of end addresses of BASIC programs */
/* 19AC: find the program the pc belongs to */
      while (pc > read16 (x))
      {
        x += 2;
      }
/* 19B0: set the selected program */
      ram[0x8268u-RAMSTART] = (uint8_t) (x - 0x822Cu) / 2;
      return 0;
    }
  }
  write16 (0x8254, read16 (0x8240));
  return ERR7;		/* stack underflow, "RETURN without GOSUB" error */
}


/* 19BE: code 0xDC, statement IF */
int cmd_if (void)
{
  int i;
  unsigned int str1, str2;
  uint8_t op;
  int64_t x, y;

  i = eval (0x80);
  if (i != DONE)
  {
    return i;
  }
  op = ram[0x824Au-RAMSTART];

  write16 (0x8254, read16 (0x8240));
  if ((ram[datasp+1-RAMSTART] & 0x40) == 0)	/* type of the second operand */
  {
/* 19D4: compare numbers */
    if ((ram[datasp+9-RAMSTART] & 0x40) != 0)
    {
      return ERR2;		/* the first operand is not a number */
    }
    y = (int64_t) read64 (datasp);
    x = (int64_t) read64 (datasp+8);
    switch (op)
    {
      case '=':
        i = (x == y);
        break;
      case 0x5C:	/* not equal */
        i = (x != y);
        break;
      case '<':
        i = (x < y);
        break;
      case '>':
        i = (x > y);
        break;
      case 0x5F:	/* <= */
        i = (x <= y);
        break;
      case 0x7E:	/* >= */
        i = (x >= y);
        break;
    }
  }

  else
  {
/* 1A60: compare strings */
    if ((ram[datasp+9-RAMSTART] & 0x40) == 0)
    {
      return ERR2;		/* the first operand is not a string */
    }
    if (op != '=' && op != 0x5C /*not equal*/)
    {
/* 1A6C: */
      while (ram[--pc-RAMSTART] != op)
      {
      }
      return ERR2;		/* illegal operator */
    }
/* 1A74: compare the lenghts */
    i = (int) read16 (datasp+4);
    if (i != (int) read16 (datasp+12))
    {
/* 1A7E: strings of different length */
      i = (op != '=');
    }
    else
    {
/* 1A8A: strings of equal length */
      if (i == 0)
      {
/* 1A8E: empty strings */
        i = (op == '=');
      }
      else
      {
/* 1A9A: strings not empty */
        str2 = read16 (datasp+2);	/* address */
        if (ram[datasp-RAMSTART] == 0)	/* identifier of the second string */
        {
          str2 -= 8;		/* address in case of a string variable */
        }
        str1 = read16 (datasp+10);	/* address */
        if (ram[datasp+8-RAMSTART] == 0) /* identifier of the first string */
        {
          str1 -= 8;		/* address in case of a string variable */
        }
/* compare the contents of the strings */
        if (ram[str2++-RAMSTART] == ram[str1++-RAMSTART])
        {
          if (ram[datasp-RAMSTART] == 0)
          {
            ++str2;		/* skip the identifier of a string variable */
          }
          if (ram[datasp+8-RAMSTART] == 0)
          {
            ++str1;		/* skip the identifier of a string variable */
          }
/* remaining characters */
          while (--i != 0)
          {
            if (ram[str2++-RAMSTART] != ram[str1++-RAMSTART])
            {
	      break;
            }
          }
        }
        if (op == '=')
        {
          i = !i;
        }
      }
    }
  }

  datasp += 16;
  if (i == 0)
  {
/* 1A0E: condition false */
    if (ram[pc-RAMSTART] == 0xE0 /*THEN*/)
    {
      ++pc;
      while (ram[pc-RAMSTART] != ':' && ram[pc-RAMSTART] != 0)
      {
        ++pc;
      }
      return 0;
    }
    if (ram[pc-RAMSTART] == ';')
    {
      ++pc;
      while (ram[pc-RAMSTART] != 0)
      {
        ++pc;
      }
      return 0;
    }
    return ERR2;
  }

/* 1A38: condition true */
  if (ram[pc-RAMSTART] == 0xE0 /*THEN*/)
  {
    ++pc;
    return cmd_goto ();
  }
  if (ram[pc-RAMSTART] == ';')
  {
    ++pc;
    return 0;
  }
  return ERR2;
}


/* 1AFC: code 0xDD, statement FOR */
int cmd_for (void)
{
  int i;
  unsigned int x;	/* address of the FOR stack entry */
  unsigned int y;	/* address of the control variable */

  i = eval (0x20);
  if (i != DONE)
  {
    return i;
  }
  if (ram[pc-RAMSTART] != 0xE1)	/* TO statement expected */
  {
    write16 (0x8254, pc);
    return ERR2;		/* missing TO statement */
  }
  write16 (0x8254, read16 (0x8240));
  if ((ram[datasp+1-RAMSTART] & 0x20) == 0)	/* numeric variable expected */
  {
    return ERR5;		/* invalid control variable */
  }
  y = read16 (datasp+2);	/* address of the control variable */
  datasp += 8;

/* 1B20: test if the variable wasn't already used by some other FOR-NEXT loop */
  for (x=0x8222u; x>0x81D2u; x-=20)
  {
    if (y == read16 (x))
    {
      break;
    }
  }
  if (x == 0x81D2u)
  {
/* 1B32: scan the FOR stack for a free entry */
    for (x=0x81E6u; x<0x8236u; x+=20)
    {
      if (read16 (x) == 0)
      {
        break;
      }
    }
    if (x == 0x8236u)
    {
      return ERR7;		/* FOR stack overflow */
    }
    write16 (x, y);
  }

/* 1B46: evaluate the TO value */
  write16 (0x8240, pc);
  ++pc;
  i = eval (0x10);
  if (i != DONE)
  {
    return i;
  }
  if ((ram[datasp+1-RAMSTART] & 0x60) != 0)	/* numeric value expected */
  {
    write16 (0x8254, read16 (0x8240));
    return ERR5;
  }
/* 1B62: move the TO value from the system stack to the FOR stack entry */
  x -= 10;
  for (i=8; i!=0; --i)
  {
    ram[x++-RAMSTART] = ram[datasp++-RAMSTART];
  }

/* 1B6E: */
  if (ram[pc-RAMSTART] != 0xE2)		/* STEP statement? */
  {
/* 1B74: missing STEP statement, use default STEP value of 1 */
    write16 (x, pc);	/* destination address of the NEXT loop */
    x -= 8;
    write16 (x-=2, (unsigned int) ONE);
    write16 (x-=2, (unsigned int) (ONE>>16));
    write16 (x-=2, (unsigned int) (ONE>>32));
    write16 (x-=2, (unsigned int) (ONE>>48));
  }
  else
  {
/* 1B88: evaluate the STEP value */
    write16 (0x8240, pc);
    ++pc;
    i = eval (0x10);
    if (i != DONE)
    {
      return i;
    }
    if ((ram[datasp+1-RAMSTART] & 0x60) != 0)	/* numeric value expected */
    {
      write16 (0x8254, read16 (0x8240));
      return ERR5;
    }
    write16 (x, pc);		/* destination address of the NEXT loop */
/* 1BA6: move the STEP value from the system stack to the FOR stack entry */
    x -= 16;
    for (i=8; i!=0; --i)
    {
      ram[x++-RAMSTART] = ram[datasp++-RAMSTART];
    }
  }
  return 0;
}


/* 1BB6: code 0xDF, statement INPUT */
int cmd_input (void)
{
  uint8_t x;
  int i;

  do {
/* 1BCC: */
    while (ram[pc-RAMSTART] == '\"')
    {
      write16 (0x825A, pc);
/* 1BD2: print characters until the next quotation mark */
      ++pc;
      while ((x = ram[pc++ - RAMSTART]) != '\"')
      {
/* 1BD6: */
        display_char (x);
      }
/* 1BE6: */
      if (ram[pc-RAMSTART] != ',')
      {
        write16 (0x8254, pc);
        return ERR2;
      }
      ++pc;
    }

/* 1BEE: */
    write16 (0x8254, pc);
    i = eval (0x20);
    if (i != DONE)
    {
      return i;
    }
    display_char ('?');
    ram[0x8264u-RAMSTART] = 0;
    ram[0x8265u-RAMSTART] = 0;
    do {
      i = input_line_editor ();
    } while (i != 0 /*[EXE]*/ || (ram[0x8264u-RAMSTART] & 0x80) == 0);
    clear_screen ();

    i = do_input ();
    if (i < 0)
    {
      msg_errx (i);
/* 135C: */
      while (keyb0770 () != 7 /*AC*/)
      {
      }
      clear_screen ();
      pc = read16 (0x825A);
    }
  } while (i < 0);

/* 1C4C: */
  if (ram[pc-RAMSTART] == ',')
  {
    ++pc;
    ram[0x8259u-RAMSTART] = 0xDF;	/* code of the INPUT statement */
  }
  return 0;
}


/* 1C24: assignment of the value of an expression in the input line buffer to
 the variable on the data stack */
int do_input (void)
{
  int i;
  unsigned int src, dst, size, len;

  if ((ram[datasp+1-RAMSTART] & 0x20) != 0)
  {
/* 1C2A: numeric INPUT variable */
    dst = pc;
    i = tokenize (0x816D, 0x816D);
    if (i < 0)
    {
      return i;
    }
    i = eval (0x10);
    pc = dst;
    if (i != DONE)
    {
      return i;
    }
    if ((ram[datasp+1-RAMSTART] & 0x60) != 0)
    {
      write16 (0x8254, pc);
      return ERR2;		/* not a number */
    }
    dst = read16 (datasp+10) - 8 - RAMSTART;
/* 1C40: copy the number to the variable */
    for (i=8; i!=0; --i)
    {
      ram[dst++] = ram[datasp++-RAMSTART];
    }
  }

  else if ((ram[datasp+1-RAMSTART] & 0x40) != 0)
  {
/* 1C62: string INPUT variable */
    len = strlen (&ram[0x816Du-RAMSTART]);
    size = (ram[datasp-RAMSTART] == 0) ? 7 : 30;
    if (len > size)
    {
      write16 (0x8254, pc);
      return ERR5;		/* string too long */
    }
/* 1C82: copy the string to the variable */
    if (len != 0)
    {
      i = (int) len;
      src = 0x816Du-RAMSTART;
      dst = read16 (datasp+2) - RAMSTART;
      if (ram[datasp-RAMSTART] == 0)
      {
/* first character of an ordinary string variable */
        dst -= 8;
        ram[dst++] = ram[src++];
        ram[dst++] = 0x60;	/* identifier of a string variable */
        --i;
      }
/* remaining characters */
      while (i != 0)
      {
        ram[dst++] = ram[src++];
        --i;
      }
    }
/* 1C94: terminate the string with a zero, unless it's an ordinary string
 variable and the string length is equal 7 */
    if (size != 7 || len != 7)
    {
      ram[dst++] = 0;
    }
  }

  else
  {
    write16 (0x8254, pc);
    return ERR2;		/* not a variable */
  }

/* 1C48: */
  datasp += 8;			/* drop the INPUT variable */
  return 0;
}


/* 1CB8: code 0xEA, statement DRAWC */
int cmd_drawc (void)
{
  return do_draw (0);
}


/* 1CBE: code 0xEB, statement DRAW */
int cmd_draw (void)
{
  return do_draw (1);
}


/* 1CC4: */
int do_draw (int parameter)
{
  int x, y;			/* coordinates */
  uint8_t m;

  x = eval2int ();
  if (x < 0 && x != DONE)
  {
    return x;
  }
  if (ram[pc-RAMSTART] != ',')
  {
    write16 (0x8254, pc);
    return ERR2;
  }
  ++pc;
  y = eval2int ();
  if (y < 0 && y != DONE)
  {
    return y;
  }
  if (y >= 0 && y <= 6 && x >=0 && x <= 59)
  {
/* 1CEC: */
    y = (x / 5) * 8 + 7 - y;	/* address */
    m = 0x01 << (x % 5);	/* bit mask */
    if (parameter == 0)
    {
      ram[y] &= ~m;
    }
    else
    {
      ram[y] |= m;
    }
    lcd ((unsigned int) y + 0x80, ram[y]);
  }
/* 1D20: */
  ram[0x8265u-RAMSTART] &= ~0x80;
  l1652 ();
  return 0;
}


/* 26AC: tokenize the input line, i.e. replace the keywords with their codes */
int tokenize (unsigned int src /*R4*/, unsigned int dst /*R5*/)
{
  unsigned int i;	/* copy of "src", R1 */
  unsigned int savedst;	/* copy of "dst", -2(sp) */
  int cnt;		/* counter of parentheses and quotation marks, R0 */
  unsigned int j, k;	/* indexes to the keyword table, R2 */
  uint8_t x;

  savedst = dst;
  cnt = 0;
  do {

/* 26B0: */
    if ((cnt & 1) != 0)
    {
/* 271C: string between quotation marks */
      x = ram[src++-RAMSTART];
      if (x == '\"')
      {
        cnt ^= 1;
      }
    }

    else
    {
/* 26B4: skip spaces */
      while (ram[src-RAMSTART] == ' ')
      {
        ++src;
      }
/* 26BC: */
      j = 0;

/* 26C4: compare a keyword loop */
      do {
        i = src;
        k = 0;
/* 26C6: skip the optional leading space */
        if (keywords[j][k] == ' ')
        {
          ++k;
        }

/* 26CE: compare the characters */
        do {
          x = keywords[j][k++];
        } while (x == ram[i++-RAMSTART] && x != 0);

/* 26DC: mismatch, try the next keyword */
      } while (x != 0 && x != ' ' && ++j < NKEYW);

      if (j < NKEYW)
      {
/* 26D6: matching keyword found */
        x = (uint8_t) (j + 0xC0);
	src = --i;
      }
      else
      {
/* 26F0: end of the keyword table reached, no matching keyword found */
        x = ram[src++-RAMSTART];
        switch (x)
        {
          case '!':
/* 26F4: the rest of the line is a comment */
            do {
	      ram[dst++-RAMSTART] = x;
	      x = ram[src++-RAMSTART];
	    } while (x != 0);
	    break;

          case '(':
/* 2702: */
            cnt += 2;
	    break;

          case ')':
/* 270C: */
            cnt -= 2;
	    if (cnt < 0)
/* 2712: unmatched closing parenthesis encountered */
            {
              pc = --src;
              write16 (0x8240, savedst);
              write16 (0x8254, pc);
              return ERR2;
            }
	    break;

          case '\"':
/* 2722: */
            cnt ^= 1;
	    break;
        }
      }
    }

/* 2724: */
    ram[dst++-RAMSTART] = x;

/* 2726: */
  } while (x != 0);

/* 2728: completed */
  if (cnt != 0)
  {
/* unmatched parentheses or quotation marks */
    pc = --src;
    write16 (0x8240, savedst);
    write16 (0x8254, pc);
    return ERR2;
  }
  pc = savedst;
  return 0;
}


/* 2732: copy a BASIC line expanding keywords,
 returns the position of an error/cursor in dst (positive) or an error code
 (negative) */
int expand (
 unsigned int src	/* R5 */,
 unsigned int dst	/* = input line buffer 0x816D, R4 */,
 unsigned int pos	/* position of an error/cursor in src when invoked by
			 the ANS key, R1 */)
{
  uint8_t x, y;
  int i;
  unsigned int j, k, n;

  i = (int) (0x81ADu - dst);	/* size of the destination buffer */
  pos += dst;

/* 273A: */
  do {
    x = ram[src++-RAMSTART];

    if (x >= 0xC0)
    {
/* 2744: keyword */
      j = (unsigned int) x - 0xC0;
      k = 0;
/* 2756: copy the string to the destination buffer */
      while ((y = keywords[j][k]) != 0)
      {
/* 2760: */
        if (--i < 0)		/* expanded text too long */
        {
          ram[dst-RAMSTART] = 0;	/* 278A: terminate the expanded text */
          n = squeeze ();
	  if (n == 0)
          {
            write16 (0x8254, pc);
            return ERR_EX;
          }
          if (pos >= n)		/* 27A6: adjust the error/cursor address */
          {
            --pos;
          }
	  --dst;
        }
/* 2768: */
        if (dst < pos)
        {
          ++pos;
        }
        ram[dst++-RAMSTART] = y;
        ++k;
      }
/* 275A: */
      if (dst < pos)
      {
        --pos;
      }
    }

    else
    {
/* 2772: single character */
      if (--i < 0)		/* expanded text too long */
      {
        ram[dst-RAMSTART] = 0;	/* 278A: terminate the expanded text */
        n = squeeze ();
        if (n == 0)
        {
          write16 (0x8254, pc);
          return ERR_EX;
        }
        if (pos >= n)		/* 27A6: adjust the error/cursor address */
        {
          --pos;
        }
	--dst;
      }
      ram[dst++-RAMSTART] = x;
    }

  } while (x != 0);
  return (int) (pos-0x816Du);
}


/* 2784: squeeze the expanded text by removing the first found space,
 returns the address of this space or zero if none could be found,
 some parts of the original code have been moved to the calling function */ 
unsigned int squeeze (void)
{
  uint8_t x;
  unsigned int i, j, cnt;

  cnt = 0;
  i = 0x816Du-RAMSTART;
/* 278C: find a space, but not enclosed by a pair of quotation marks */
  do {
    x = ram[i++];
    if (x == 0)
    {
      return 0;		/* no dispensable space found */
    }
    if (x == '\"')
    {
      cnt ^= 1;
    }
  } while (cnt != 0 || x != ' ');
  j = i;
/* 27AC: fill the gap by moving the remaining text down by one byte */
  do {
    x = ram[i];
    ram[i-1] = x;
    ++i;
  } while (x != 0);
  return j;
}


/* address of the table entry */
unsigned int tab822c_addr (void)
{
  return 0x822Cu /*table of end addresses of BASIC programs*/ +
	2 * (unsigned int) ram[0x8268u /*selected program*/ - RAMSTART];
}


/* begin address of a BASIC program n */
unsigned int prog_addr (unsigned int n)
{
  return (n == 0) ? 0x826Bu /*address of the program P0*/ :
	read16 (2 * n + 0x822Au) /*programs P1 to P9*/;
}
