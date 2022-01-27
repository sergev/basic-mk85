/* hardware independent module */

#include <stdint.h>
#include <string.h>	/* strcpy */
#include "arithm.h"
#include "common.h"
#include "editor.h"
#include "io.h"
#include "main.h"

extern uint8_t ram[RAMSIZE];

/* private function prototypes */
void mode2 (void);
void l03c6 (void);
void mode3 (void);
void l03fc (void);
void arrow_left (void);
void arrow_right (void);
void delete (void);
void insert (void);


#define TAB0021_SIZE 51

/* 0108 input line editor,
 returned information what was this function terminated with:
0 - [EXE]
1 - [S] and a digit
2 - [MODE] [0] or [MODE] [1]
3 - [init] */
int input_line_editor (void)
{
  const struct {
    char keychar;
    char* keyword;
  } tab0021[TAB0021_SIZE] = {
/* index 0: empty table */
    { 0, "" },
/* index 1: table of keywords mapped to keys in mode [S] */
    { ' ', "LETC " },
    { 'A', "GOSUB " },
    { 'B', "RUN " },
    { 'C', "END" },
    { 'D', " TO " },
    { 'F', " STEP " },
    { 'G', "NEXT " },
    { 'H', "GOTO " },
    { 'J', "IF " },
    { 'K', " THEN " },
    { 'L', "PRINT " },
    { 'M', "INPUT " },
    { 'N', "LIST " },
    { 'S', "FOR " },
    { 'V', "DEFM " },
    { 'X', "STOP" },
    { 'Z', "RETURN" },
    { 0, "" },
/* index 19: table of keywords mapped to keys in mode [F] */
    { ' ', "GETC(" },
    { '=', "ASCI " },
    { 0x7B, "CHR " },
    { 'A', "SIN " },
    { 'B', "FRAC " },
    { 'C', "SGN " },
    { 'D', "TAN " },
    { 'E', "VAL " },
    { 'F', "ASN " },
    { 'G', "ACS " },
    { 'H', "ATN " },
    { 'I', "VAC" },
    { 'J', "LOG " },
    { 'K', "LN " },
    { 'L', "EXP " },
    { 'M', "CSR " },
    { 'N', "RAN#" },
    { 'O', "RND(" },
    { 'P', "AUTO " },
    { 'Q', "SET " },
    { 'R', "MID(" },
    { 'S', "COS " },
    { 'T', "LEN " },
    { 'U', "CLEAR " },
    { 'V', "INT " },
    { 'W', "MODE " },
    { 'X', "ABS " },
    { 'Y', "KEY" },
    { 'Z', "SQR " },
    { 0, "" },
/* index 49, keywords assigned to the key [ANS] */
    { 0, "DRAW " },
    { 0, "DRAWC " }
  };

  uint8_t x, y;
  unsigned int mode;		/* R2 in the original code */
  unsigned int bufpos;		/* variable 8260 in the original code */
  int i;
  unsigned int j;

  while (1)
  {
    if ((ram[0x8265u-RAMSTART] & 0x80) == 0)
    {
/* 010E: hide the cursor */
      lcd (0xE0, SCREEN_WIDTH);
    }
    else
    {
/* 0116: show the cursor */
      if (ram[0x8269u /*cursor position*/ - RAMSTART] > SCREEN_WIDTH-1)
      {
        ram[0x8269u-RAMSTART] = SCREEN_WIDTH-1;
      }
      x = ram[0x8269u-RAMSTART];
      j = read16 (0x8260);
      while (ram[j-RAMSTART] != 0)	/* 012C: find the end of the line */
      {
        ++j;
      }
/* 0130: 7 bytes or more to the end of the buffer? */
      if (j < 0x81A7u)
      {
/* 0136: cursor shaped as underscore */
        x += 0x10;
      }
      lcd (0xE0, x);
    }

/* 013E: */
    do {
      key_wait ();
      mode = read16 (0x8256);
      i = keyb0770 ();
    } while (i == 0);

    j = 0;
    x = 0;
/* 0172: */
    if (i <= 0x0A)
    {
/* 0178: control keys with scan codes 0x04 to 0x0A */
      switch (i)
      {
        case 5:
          arrow_left ();
          l03c6 ();
          continue;

        case 6:
          arrow_right ();
          l03c6 ();
          continue;

        case 7:			/* AC */
          ac ();
          continue;

        case 8:			/* 050E: DEL/INS */
          if ((mode & 0x0020) == 0)
          {
            delete ();		/* [S] off */
          }
          else
          {
            insert ();		/* [S] on */
          }
          l03c6 ();
          continue;

        case 9:			/* 05D8: ANS */
          if ((mode & 0x0030) != 0)
          {
/* 05DE: [S] or [F] on, keywords DRAW/DRAWC */
            j = ((mode & 0x0020) == 0) ? 49 /*[S] on*/ : 50 /*[F] on*/;
          }
          else
          {
/* 05EE: [S] off, [F] off, contents of the variable ANS */
            fp2str (read64 (0x8224 /*variable ANS*/), -10,
		&ram[0x8060u-RAMSTART]);
            ram[0x8264u-RAMSTART] &= 0x80;
            l03c6 ();
            j = TAB0021_SIZE;	/* or more */
          }
	  break;

        case 10:		/* RAM initialisation */
          return 3;

        default:		/* EXE (key scan code = 4) */
          return 0;
      }
    }

/* 0180: */
    else if (i >= '0' && i <= '9')
    {
/* 018C: */
      if ((mode & 0x0220) != 0)
      {
        if ((mode & 0x0200) == 0)
        {
/* 0198: 044C: [S] key followed by number 0-9, select program */
          ram[0x8268u-RAMSTART] = (uint8_t) (unsigned int) (i - '0');
          return 1;
        }
        else
        {
/* 019C: [MODE] key followed by number 0-9 */
          i -= '0';
          switch (i)
          {
            case 0:		/* run */
              mode0 ();
              return 2;

            case 1:		/* write */
              mode1 ();
              return 2;

            case 2:		/* trace on */
              mode2 ();
              continue;

            case 3:		/* trace off */
              mode3 ();
              continue;

            case 4:		/* DEG */
              mode4 ();
              continue;

            case 5:		/* RAD */
              mode5 ();
              continue;

            case 6:		/* GRA */
              mode6 ();
              continue;

            default:
              continue;
          }
        }
      }
    }

/* 01A6: */
    else if ((mode & 0x0040) == 0)
    {
/* 01AC: [EXT] off */
      if ((mode & 0x0030) != 0)
      {
/* 022C: [S] or [F] on */
        if ((mode & 0x0010) != 0)
        {
          j = 18;	/* j = 18 when [F] on, j = 0 when [S] on */
        }
/* 0242: scan the table for the character i */
        y = (uint8_t) (unsigned int) i;
        do {
          x = tab0021[++j].keychar;
        } while (x != y && x != 0);
      }

      if (x == 0)
      {
        j = 0;
/* 01B2: [S] and [F] off */
        if ((ram[0x8264u /*flags*/ - RAMSTART] & 0x80) != 0)
        {
/* 0216: test for the "E-" sequence which will be replaced with a single
 character 7D */
          bufpos = read16 (0x8260);
          if (i == '-' && ram[bufpos-1-RAMSTART] == 0x7B)
          {
/* 02BA: replace the "E-" sequence with a single character 7D */
            ram[bufpos-1-RAMSTART] = 0x7D;
            --ram[0x8269u /*cursor position*/ - RAMSTART];
            lcd_char (0x7D);
            ++ram[0x8269u /*cursor position*/ - RAMSTART];
            continue;
          }
        }

/* 01B8: */
        else if ((ram[0x8264u-RAMSTART] & 0x20) != 0)
        {
/* 01C0: the display shows a result of previous calculation, which can be used
 as first argument of subsequent calculations */
          ram[0x8264u-RAMSTART] &= 0x80;
/* 01C6: any binary operator which would use this argument? */
          if (i == '+' || i == '-' || i == '*' || i == '/' || i == '^')
          {
/* 01E6: display the previous calculation result again using full precision */
            fp2str (read64 (0x8224 /*variable ANS*/), -10,
		&ram[0x816Du-RAMSTART]);
            ram[0x8265u-RAMSTART] |= 0x80;
            display_short_string (0x816D);
            bufpos = 0x816D;
            while (ram[bufpos-RAMSTART] != 0)	/* find the end of the line */
            {
              ++bufpos;
            }
            write16 (0x8260, bufpos);
          }
        }
      }
    }

/* 0258: copy a keyword from the table (if index j != 0) or a single
 character i (if j == 0) to the input line buffer */
    if (j == 0)
    {
      ram[0x8060u-RAMSTART] = (uint8_t) (unsigned int) i;
      ram[0x8061u-RAMSTART] = 0;
    }
    else if (j < TAB0021_SIZE)
    {
      strcpy (&ram[0x8060u-RAMSTART], tab0021[j].keyword);
    }

    j = 0;
    while ((x = ram[j+0x8060u-RAMSTART]) != 0)
    {
      ++j;
/* 0260: */
      if ((ram[0x8264u-RAMSTART] & 0x80) == 0)
      {
/* 0266: initialise the input line buffer */
        write16 (0x8260, 0x816D);
        ram[0x816Du-RAMSTART] = 0;
      }
/* 0270: put the character x to the input line buffer */
      bufpos = read16 (0x8260);
      if (bufpos >= 0x81ACu /*end of the input line buffer*/)
      {
/* 0276: out of room */
        break;
      }
      ram[0x8265u-RAMSTART] |= 0x80;
      ram[0x8269u /*cursor position*/ - RAMSTART] = (uint8_t) (bufpos-0x816Du);
      y = ram[bufpos-RAMSTART];	/* previous char. in the input line buffer */
      ram[bufpos-RAMSTART] = x;	/* new character */
      write16 (0x8260, ++bufpos);
      if (y == 0)
      {
/* 029A: terminate the line */
        ram[bufpos-RAMSTART] = 0;
      }
      display_char (x);
      if (ram[0x8269u /*cursor position*/ - RAMSTART] >= SCREEN_WIDTH-1)
      {
/* 02AA: */
        x = ram[bufpos-RAMSTART];
        if (x == 0)
        {
/* 02B0: */
          x = ' ';
        }
        lcd_char (x);
      }
    }
  }
}


/* 02EC: MODE 0 - run */
void mode0 (void)
{
  unsigned int addr;

  ram[0x8256u-RAMSTART] &= ~0xB1;
  ram[0x8257u-RAMSTART] &= ~0xC7;
  ram[0x08] &= ~0x02;
  ram[0x08] |= 0x01;
  lcd (0x88, ram[0x08]);
  for (addr=0x30; addr<0x60; ++addr)
  {
    ram[addr] = 0;
    lcd (addr+0x80, 0);
  }
  ram [0x8264u-RAMSTART] = 0;
  ram [0x8265u-RAMSTART] = 0;
  display_char ('R');
  display_char ('E');
  display_char ('A');
  display_char ('D');
  display_char ('Y');
  display_char (' ');
  display_char ('P');
  display_char ('0' + ram[0x8268u /*selected program*/ - RAMSTART]);
/* 0336: 0B9C: */
  ram[0x8265u-RAMSTART] = 0;
  ram[0x8264u-RAMSTART] &= 0x20;
}


/* 033A: MODE 1 - write */
void mode1 (void)
{
  unsigned int x, addr1, addr2, addr3;

  ram[0x8256u-RAMSTART] &= ~0x31;
  ram[0x8256u-RAMSTART] |= 0x80;	/* write mode */
  ram[0x8257u-RAMSTART] &= ~0xC7;
  ram[0x08] &= ~0x01;
  ram[0x08] |= 0x02;
  lcd (0x88, ram[0x08]);
  display_7seg (free_ram ());
/* 035C: display "P 0123456789" */
  ram [0x8264u-RAMSTART] = 0;
  ram [0x8265u-RAMSTART] = 0;
  display_char ('P');
  display_char (' ');
  x = '0';
  addr1 = 0x826B;
  for (addr2=0x822Cu; addr2<0x8240u; addr2+=2)
  {
    addr3 = read16 (addr2);
    display_char ((addr1 == addr3) ? x : 0x14 /*diamond*/);
    ++x;
    addr1 = addr3;
  }
  ram[0x8269u /*cursor position*/ - RAMSTART] =
	ram[0x8268u /*selected program*/ - RAMSTART] + 2;
  ram[0x8265u-RAMSTART] |= 0x80;
/* 03B0: 0BA0: */
  ram[0x8264u-RAMSTART] &= 0x20;
}


/* 03B4: MODE 2 - trace on */
void mode2 (void)
{
  ram[0x8256u-RAMSTART] |= 0x08;
  ram[0x28] = 0x01;
  lcd (0xA8, 0x01);
  l03c6 ();
}


/* 03C6: */
void l03c6 (void)
{
  ram[0x8256u-RAMSTART] &= ~0x30;
  ram[0x8257u-RAMSTART] &= ~0x02;
  ram[0] &= ~0x06;
  lcd (0x80, ram[0]);
}


/* 03DC: MODE 3 - trace off */
void mode3 (void)
{
  ram[0x8256u-RAMSTART] &= ~0x08;
  ram[0x28] = 0;
  lcd (0xA8, 0);
  l03c6 ();
}

/* 03E8: MODE 4 - DEG */
void mode4 (void)
{
  ram[0x8256u-RAMSTART] &= ~0x06;
  ram[0x08] |= 0x10;
  ram[0x18] = 0;
  ram[0x20] = 0;
  l03fc ();
}


/* 03FC: */
void l03fc (void)
{
  lcd (0x88, ram[0x08]);
  lcd (0x98, ram[0x18]);
  lcd (0xA0, ram[0x20]);
  l03c6 ();
}


/* 0410: MODE 5 - RAD */
void mode5 (void)
{
  ram[0x8256u-RAMSTART] &= ~0x06;
  ram[0x8256u-RAMSTART] |= 0x02;
  ram[0x08] &= ~0x10;
  ram[0x18] = 0x01;
  ram[0x20] = 0;
  l03fc ();
}


/* 042E: MODE 6 - GRA */
void mode6 (void)
{
  ram[0x8256u-RAMSTART] &= ~0x06;
  ram[0x8256u-RAMSTART] |= 0x04;
  ram[0x08] &= ~0x10;
  ram[0x18] = 0;
  ram[0x20] = 0x01;
  l03fc ();
}


/* 0470: arrow left */
void arrow_left (void)
{
  uint8_t x;
  unsigned int bufpos;		/* variable 8260 in the original code */
  unsigned int j;

  bufpos = read16 (0x8260);
  if ((ram[0x8264u-RAMSTART] & 0x80) != 0 &&
	ram[0x8269u /*cursor position*/ - RAMSTART] != 0)
  {
/* 047C: */
    if (bufpos-- > 0x816Du+SCREEN_WIDTH-1)
    {
/* 048E: scroll the display right */
      j = 0x58;
      do {
        --j;
        do {
          x = ram[j];
          lcd (j+0x88, x);
          ram[j+8] = x;
        } while ((--j & 7) != 0);
      } while (j != 0);
      ram[0x8269u /*cursor position*/ - RAMSTART] = 0;
      lcd_char (ram[bufpos-(SCREEN_WIDTH-1)-RAMSTART]);
      ram[0x8269u-RAMSTART] = SCREEN_WIDTH;
    }
/* 04BE: */
    --ram[0x8269u /*cursor position*/ - RAMSTART];
    write16 (0x8260, bufpos);
  }
}


/* 04C4: arrow right */
void arrow_right (void)
{
  uint8_t x;
  unsigned int bp;		/* variable 8260 in the original code */
  unsigned int j;

  bp = read16 (0x8260) - RAMSTART;
  if ((ram[0x8264u-RAMSTART] & 0x80) != 0 && ram[bp] != 0)
  {
/* 04D0: */
    ++bp;
    if (ram[0x8269u /*cursor position*/ - RAMSTART] >= SCREEN_WIDTH-1)
    {
/* 04DC: scroll the display left */
      j = 0;
      do {
        ++j;
        do {
          x = ram[j+8];
          lcd (j+0x80, x);
          ram[j] = x;
        } while ((++j & 7) != 0);
      } while (j != 0x58);
/* 04F6: */
      x = ram[bp];
      lcd_char ((x != 0) ? x : ' ');
    }
/* 0506: */
    ++ram[0x8269u /*cursor position*/ - RAMSTART];
    write16 (0x8260, bp+RAMSTART);
  }
}


/* 050E: DEL */
void delete (void)
{
  uint8_t x;
  unsigned int bp;		/* variable 8260 in the original code */
  int i;
  unsigned int j, k;

  bp = read16 (0x8260) - RAMSTART;
  if ((ram[0x8264u-RAMSTART] & 0x80) != 0 && ram[bp] != 0)
  {
/* 0514: */
    i = (int) (int8_t) ram[0x8269u /*cursor position*/ - RAMSTART];
    j = bp;
/* 0526: shrink the contents of the input line buffer */
    do {
      x = ram[j+1];
      ram[j] = x;
      ++j;
    } while (x != 0);
/* 052C: */
    j = 8 * (unsigned int) i;
    i = SCREEN_WIDTH-1 - i;
    k = bp + (unsigned int) i;
    if (i > 0)
    {
/* 0538: shift the part of the display contents right from the deleted character
 to the left */
      do {
        ++j;
        do {
          x = ram[j+8];
          lcd (j+0x80, x);
          ram[j] = x;
        } while ((++j & 7) != 0);
      } while (--i != 0);
    }
/* 055A: find the end of the line */
    while (ram[bp] != 0)
    {
      ++bp;
    }
/* 0560: */
    x = ram[0x8269u /*cursor position*/ - RAMSTART];
    ram[0x8269u-RAMSTART] = SCREEN_WIDTH-1;
    lcd_char ((k < bp) ? ram[k] : ' ');
    ram[0x8269u-RAMSTART] = x;
  }
}


/* 057E: [S] on, INS */
void insert (void)
{
  uint8_t x;
  unsigned int bp;		/* variable 8260 in the original code */
  int i;
  unsigned int j;

  bp = read16 (0x8260) - RAMSTART;;
  if ((ram[0x8264u-RAMSTART] & 0x80) != 0 && ram[bp] != 0)
  {
/* 0514: */
    i = (int) (int8_t) ram[0x8269u /*cursor position*/ - RAMSTART];
    j = bp;
/* 057E: find the end of the line */
    while (ram[j++] != 0)
    {
    }
/* 0582: */
    if (j < 0x81ADu-RAMSTART)
    {
/* 0588: expand the contents of the input line buffer */
      do {
        --j;
        ram[j+1] = ram[j];
      } while (j > bp);
/* 0592: */
      ram[j] = ' ';
/* 0596: */
      i = SCREEN_WIDTH-1-i;
      if (i > 0)
      {
/* 05A0: shift the part of the display contents right from the inserted
 character to the right */
        j = 0x58;
        do {
          --j;
          do {
            x = ram[j];
            lcd (j+0x88, x);
            ram[j+8] = x;
          } while ((--j & 7) != 0);
        } while (--i != 0);
      }
/* 05B6: */
      lcd_char (' ');
    }
  }
}


/* 05C0: AC */
void ac (void)
{
/* 05C0: initialise the input line buffer */
  write16 (0x8260, 0x816D);
  ram[0x816Du-RAMSTART] = 0;
/* 05CA: */
  clear_screen ();
  ram[0x8265u-RAMSTART] = 0x80;
/* 0BA0: */
  ram[0x8264u-RAMSTART] &= 0x20;
}


/* 0A5E: amount of free RAM */
unsigned int free_ram (void)
{
  return read16 (0x8252 /*top of the RAM*/)
	- 8 * read16 (0x8250 /*number of variables*/)
	- read16 (0x823E /*end of BASIC programs*/);
}
