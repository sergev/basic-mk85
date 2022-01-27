/* BASIC functions, hardware independent module */

#include "func.h"

#include <stdint.h>
#include <string.h> /* strlen */

#include "arithm.h"
#include "common.h"
#include "main.h" /* ram */

extern uint8_t ram[RAMSIZE];
extern unsigned int pc;
extern unsigned int datasp;

/* private function prototypes */
int func_sin(void);
int func_cos(void);
int func_tan(void);
int func_asn(void);
int func_acs(void);
int func_atn(void);
int func_log(void);
int func_ln(void);
int func_exp(void);
int func_sqr(void);
uint64_t fudge(uint64_t x);
int func_abs(void);
int func_int(void);
int func_sgn(void);
int func_frac(void);
int func_rnd(void);
int concatenate(void);
int func_val(void);
int func_len(void);
int func_mid(void);
int func_chr(void);
int func_asci(void);
int func_getc(void);
uint8_t is_fp_number(void);
uint8_t two_fp_numbers(void);
unsigned int angle_mode(void);

/* 2236: execute an operation, returns a negative error code if something went
 wrong, otherwise zero */
int exec_op(unsigned int opcode)
{
    /* 2280: */
    const int (*func_tab[])(void) = {
        func_sin,  /* code 0xC0, function SIN */
        func_cos,  /* code 0xC1, function COS */
        func_tan,  /* code 0xC2, function TAN */
        func_asn,  /* code 0xC3, function ASN */
        func_acs,  /* code 0xC4, function ACS */
        func_atn,  /* code 0xC5, function ATN */
        func_log,  /* code 0xC6, function LOG */
        func_ln,   /* code 0xC7, function LN */
        func_exp,  /* code 0xC8, function EXP */
        func_sqr,  /* code 0xC9, function SQR */
        func_abs,  /* code 0xCA, function ABS */
        func_int,  /* code 0xCB, function INT */
        func_sgn,  /* code 0xCC, function SGN */
        func_frac, /* code 0xCD, function FRAC */
        func_val,  /* code 0xCE, function VAL */
        func_len,  /* code 0xCF, function LEN */
        func_chr,  /* code 0xD0, function CHR */
        func_asci, /* code 0xD1, function ASCI */
        func_rnd,  /* code 0xD2, function RND */
        func_mid,  /* code 0xD3, function MID */
        func_getc  /* code 0xD4, function GETC */
    };

    uint64_t x;

    if (opcode < 0xC0) {
        switch (opcode) {
        case '+':
            /* 240E: */
            if (two_fp_numbers() != 0) {
                return concatenate();
            }
            x = fp_add(read64(datasp + 8), read64(datasp));
            datasp += 8;
            if (x == NOT_A_NUMBER) {
                return ERR3;
            }
            write64(datasp, fudge(x));
            return 0;

        case '-':
            /* 23EA: */
            if (two_fp_numbers() != 0) {
                return ERR2;
            }
            x = fp_sub(read64(datasp + 8), read64(datasp));
            datasp += 8;
            if (x == NOT_A_NUMBER) {
                return ERR3;
            }
            write64(datasp, fudge(x));
            return 0;

        case '*':
            /* 23F6: */
            if (two_fp_numbers() != 0) {
                return ERR2;
            }
            x = fp_mul(read64(datasp + 8), read64(datasp));
            datasp += 8;
            if (x == NOT_A_NUMBER) {
                return ERR3;
            }
            write64(datasp, fudge(x));
            return 0;

        case '/':
            /* 2402: */
            if (two_fp_numbers() != 0) {
                return ERR2;
            }
            x = fp_div(read64(datasp + 8), read64(datasp));
            datasp += 8;
            if (x == NOT_A_NUMBER) {
                return ERR3;
            }
            write64(datasp, fudge(x));
            return 0;

        case '^':
            /* 236E: */
            if (two_fp_numbers() != 0) {
                return ERR2;
            }
            x = fp_pwr(read64(datasp + 8), read64(datasp));
            datasp += 8;
            if (x == NOT_A_NUMBER) {
                return ERR3;
            }
            write64(datasp, fudge(x));
            return 0;

        case '%': /* unary minus */
                  /* 23D2: */
            if (is_fp_number() != 0) {
                return ERR2;
            }
            write64(datasp, fp_neg(read64(datasp)));
            return 0;

        default:
            return ERR_EX;
        }
    }

    if (opcode <= 0xD4) {
        return func_tab[opcode - 0xC0]();
    }

    return ERR_EX;
}

/* 22B6: code 0xC0, function SIN */
int func_sin(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_sin(read64(datasp), angle_mode());
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 22CE: code 0xC1, function COS */
int func_cos(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_cos(read64(datasp), angle_mode());
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 22E6: code 0xC2, function TAN */
int func_tan(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_tan(read64(datasp), angle_mode());
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 22FE: code 0xC3, function ASN */
int func_asn(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_asn(read64(datasp), angle_mode());
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 2316: code 0xC4, function ACS */
int func_acs(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_acs(read64(datasp), angle_mode());
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 232E: code 0xC5, function ATN */
int func_atn(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_atn(read64(datasp), angle_mode());
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 2346: code 0xC6, function LOG */
int func_log(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_log(read64(datasp));
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 2350: code 0xC7, function LN */
int func_ln(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_ln(read64(datasp));
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 235A: code 0xC8, function EXP */
int func_exp(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_exp(read64(datasp));
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 2364: code 0xC9, function SQR */
int func_sqr(void)
{
    uint64_t x;
    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp_sqr(read64(datasp));
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, fudge(x));
    return 0;
}

/* 2376: "integer cheating" of a floating point number
 If the last four digits of the mantissa are in the range 0001-0009 then they
 are replaced with 0000. Similarly, if the last four mantissa digits are in
 the range 9950-9999, then the number is rounded up until they are 0000. */
uint64_t fudge(uint64_t x)
{
    uint64_t y;
    if ((x & 0xFFFF) < 0x0010) {
        x &= ~0xFFFF;
    } else if ((x & 0xFFFF) >= 0x9950) {
        y = fp_add(x, (x & 0xFFFF000000000000) | 0x0050);
        if (y != NOT_A_NUMBER) {
            x = y & ~0xFFFF;
        }
    }
    return x;
}

/* 23A2: code 0xCA, function ABS */
int func_abs(void)
{
    if (is_fp_number() != 0) {
        return ERR2;
    }
    write64(datasp, fp_abs(read64(datasp)));
    return 0;
}

/* 23AE: code 0xCB, function INT */
int func_int(void)
{
    if (is_fp_number() != 0) {
        return ERR2;
    }
    write64(datasp, fp_int(read64(datasp)));
    return 0;
}

/* 23BA: code 0xCC, function SGN */
int func_sgn(void)
{
    if (is_fp_number() != 0) {
        return ERR2;
    }
    write64(datasp, fp_sgn(read64(datasp)));
    return 0;
}

/* 23C6: code 0xCD, function FRAC */
int func_frac(void)
{
    if (is_fp_number() != 0) {
        return ERR2;
    }
    write64(datasp, fp_frac(read64(datasp)));
    return 0;
}

/* 23DE: code 0xD2, function RND */
int func_rnd(void)
{
    uint64_t x;
    if (two_fp_numbers() != 0) {
        return ERR2;
    }
    x = fp_rnd(read64(datasp + 8), read64(datasp));
    datasp += 8;
    if (x == NOT_A_NUMBER) {
        return ERR3;
    }
    write64(datasp, x);
    return 0;
}

/* 2426: concatenation of two strings */
int concatenate(void)
{
    unsigned int len;

    /* 2426: two strings expected */
    if ((ram[datasp + 1 - RAMSTART] & ram[datasp + 9 - RAMSTART] & 0x40) == 0) {
        return ERR2;
    }
    /* 2434: the length cannot exceed 30 characters */
    len = read16(datasp + 4) + read16(datasp + 12);
    if (len > 30) {
        return ERR8;
    }
    /* 2446: */
    (void)read_string(datasp, read_string(datasp + 8, 0x812F));
    datasp += 8;
    /* 248C: */
    write16(datasp, 0x4080);     /* string identifier */
    write16(datasp + 2, 0x812F); /* address */
    write16(datasp + 4, len);    /* length */
    return 0;
}

/* 2498: code 0xCE, function VAL */
int func_val(void)
{
    uint64_t x;
    unsigned int y;

    y = read16(datasp + 4); /* string length */
    if ((ram[datasp + 1 - RAMSTART] & 0x40) == 0 || y == 0) {
        return ERR2; /* not a string or an empty string */
    }
    if (y + 0x8060u /*begin of the data stack area*/ + 1 > datasp) {
        return ERR5; /* string too long */
    }
    /* 24AC: the input string is copied to the free space on the data stack */
    ram[read_string(datasp, 0x8060) - RAMSTART] = 0;
    y = pc;
    pc = 0x8060;
    x = str2fp();
    if (x == NOT_A_NUMBER || ram[pc - RAMSTART] != 0) {
        pc = y;
        return ERR2; /* value out of range or some unprocessed characters left */
    }
    pc = y;
    write64(datasp, x);
    return 0;
}

/* 24EA: code 0xCF, function LEN */
int func_len(void)
{
    if (is_fp_number() == 0) {
        return ERR2;
    }
    write64(datasp, int2fp((int16_t)(uint16_t)read16(datasp + 4)));
    return 0;
}

/* 2500: code 0xD3, function MID */
int func_mid(void)
{
    int pos, len1, len2;

    if (two_fp_numbers() != 0) {
        return ERR2;
    }
    /* 2504: length of the substring */
    len1 = fp2int(read64(datasp));
    if (len1 < 0) {
        return ERR5;
    }
    datasp += 8;
    /* 2516: starting position of the substring */
    pos = fp2int(read64(datasp));
    if (--pos < 0) {
        return ERR5;
    }
    /* 2526: length of the string variable $ */
    len2 = strlen(&ram[0x814Eu /*address of the variable $*/ - RAMSTART]);
    /* 2534: */
    if (pos + len1 > len2) {
        return ERR5; /* the string is too short */
    }
    write16(datasp, 0x4080);                          /* string mark */
    write16(datasp + 2, 0x814Eu + (unsigned int)pos); /* address */
    write16(datasp + 4, (unsigned int)len1);          /* length of the substring */
    return 0;
}

/* 254E: code 0xD0, function CHR */
int func_chr(void)
{
    int x;

    if (is_fp_number() != 0) {
        return ERR2;
    }
    x = fp2int(read64(datasp));
    if (x < 0 || x > 191) {
        return ERR5;
    }
    write16(datasp, 0x4080);         /* string identifier */
    write16(datasp + 2, datasp + 6); /* address of string */
    write16(datasp + 4, 1);          /* length of string = 1 character */
    ram[datasp + 6 - RAMSTART] = (uint8_t)(unsigned int)x;
    return 0;
}

/* 2576: code 0xD1, function ASCI */
int func_asci(void)
{
    unsigned int x;

    if ((ram[datasp + 1 - RAMSTART] & 0x40) == 0) {
        return ERR2; /* not a string */
    }
    x = read16(datasp + 4); /* length */
    if (x != 0) {
        if (--x != 0) {
            return ERR5; /* not a single character */
        }
        x = read16(datasp + 2);          /* address */
        if (ram[datasp - RAMSTART] == 0) /* string variable? */
        {
            x -= 8;
        }
        x = (unsigned int)ram[x - RAMSTART];
    }
    write64(datasp, int2fp((int16_t)(uint16_t)x));
    return 0;
}

/* 25A2: code 0xD4, function GETC */
int func_getc(void)
{
    int pos;
    unsigned int addr, len;

    /* 25A2: second operand */
    if (is_fp_number() != 0) {
        return ERR2; /* not a number */
    }
    pos = fp2int(read64(datasp));
    datasp += 8;
    if (pos <= 0) {
        return ERR5;
    }
    --pos;
    /* 25BE: first operand */
    if ((ram[datasp + 1 - RAMSTART] & 0x40) == 0) {
        return ERR2; /* not a string */
    }
    len = read16(datasp + 4); /* length */
    if (len != 0) {
        if ((unsigned int)pos > len) {
            return ERR5; /* input string too short */
        }
        addr = read16(datasp + 2);       /* address */
        if (ram[datasp - RAMSTART] == 0) /* string variable? */
        {
            addr -= 8;
            if (pos != 0) {
                ++pos; /* skip the string variable identifier */
            }
        }
        write16(datasp + 2, addr + (unsigned int)pos); /* address of the character */
        write16(datasp + 4, 1);                        /* truncate the length to 1 */
    }
    ram[datasp - RAMSTART] = 0x80; /* string identifier */
    return 0;
}

/* 266C: returns zero if there is a floating point number on the data stack */
uint8_t is_fp_number(void)
{
    return ram[datasp + 1 - RAMSTART] & 0x60;
}

/* 2676: returns zero if there are two floating point numbers on the data stack
 */
uint8_t two_fp_numbers(void)
{
    return (ram[datasp + 1 - RAMSTART] | ram[datasp + 9 - RAMSTART]) & 0x60;
}

unsigned int angle_mode(void)
{
    return ((unsigned int)ram[0x8256u - RAMSTART] >> 1) & 3;
}
