/* expression evaluator, hardware independent module */

#include "calc.h"

#include <stdint.h>
#include <string.h> /* strlen */

#include "arithm.h"
#include "common.h"
#include "func.h"
#include "io.h"   /* function "keyb0770" */
#include "main.h" /* ram */

extern uint8_t ram[RAMSIZE];
extern unsigned int pc;
extern unsigned int datasp;

unsigned int evsp; /* expression evaluator stack pointer, R5 in the
                      original code */

/* private function prototypes */
int is_func1(uint8_t x);
int parse_pwr(void);
int parse_relational(void);
int parse_addsub();
int parse_muldiv();
int parse_variable(void);
int parse_literal(void);
int l1f80(void);
int parse_number(void);
int l1fb2(void);
int unparse_var(void);
int parse_cp(void);
int l20c4(void);
int is_term1(uint8_t x);
int l20de(void);
int l21d8(void);
int l21f2(void);
int l220c(void);
int l260c(int error_code);
int unary_op(void);

/* 1D50: evaluate an expression, requires a parameter:
 0x10 - expected a single expression
 0x20 - expected an expression or a variable, optional with assignment of
 a value
 0x80 - expected a pair of expressions */
int eval(uint8_t parameter)
{
    int st;
    /* st=2 - parsed OK, loop to 1E38
       st=1 - parsed OK, loop to 1E4C
       st=0 - parse fail, try the next "if" clause
       st<0 - an error of code -st occurred, terminate the function */

    ram[0x824Au - RAMSTART] = 0;
    ram[0x824Bu - RAMSTART] = parameter;
    /* 1D58: initialise the expression evaluator stack */
    evsp = 0x814E;
    ram[--evsp - RAMSTART] = 0;
    st = 2;

    /* 1E38: main loop of the expression evaluator */
    while (1) {
        /* 1E40: parse for unary operators '+' and '-' */
        if (st > 1 && (st = (unary_op() & 1)) > 0) {
            ram[--evsp - RAMSTART] = '%'; /* unary minus operator */
        }

        /* 1E4C: */
        /* 1E50 then 1E14: parse for a function */
        else if (ram[pc - RAMSTART] >= 0xC0 && ram[pc - RAMSTART] <= 0xD4) {
            if (is_func1(ram[pc - RAMSTART]) != 0 && ram[pc + 1 - RAMSTART] != '(') {
                /* function RND, MID or GET not followed by an opening parenthesis */
                write16(0x8254, ++pc);
                return ERR2;
            }
            ram[--evsp - RAMSTART] = ram[pc - RAMSTART];
            ++pc;
            st = 2;
        }

        /* 1E52: parse for a variable */
        /* 1E90: */
        else if ((st = parse_variable()) != 0) {
            if (st < 0) {
                return st;
            }
        }

        /* 1E54: parse for a literal */
        /* 1EFE: */
        else if ((st = parse_literal()) != 0) {
            if (st < 0) {
                return st;
            }
        }

        /* 1E56: parse for an opening parenthesis */
        /* 1E60: */
        else if (ram[pc - RAMSTART] == '(') {
            ram[--evsp - RAMSTART] = ram[pc - RAMSTART];
            ++pc;
            st = 2;
        }

        /* 1E58: */
        /* 263E: */
        else {
            write16(0x8254, pc);
            return ERR2;
        }
    }
}

/* 1D6A: */
int parse_comma(void)
{
    int x;
    uint8_t y;

    if (ram[pc - RAMSTART] != ',') {
        return 0;
    }
    ++pc;

    /* 1D72: */
    while ((y = ram[evsp - RAMSTART]) != '(') {
        ++evsp;
        if (y == 0) {
            --pc;
            return l20de();
        }
        x = exec_op((unsigned int)y);
        if (x < 0) {
            return l260c(x);
        }
    }

    /* 1D88: comma separating arguments of the functions RND, MID, GETC */
    if (is_func1(ram[evsp + 1 - RAMSTART]) != 0) {
        return 2;
    }
    write16(0x8254, --pc);
    return ERR2;
}

/* returns not zero if 'x' is a code of a function requiring multiple arguments
 enclosed within parentheses */
int is_func1(uint8_t x)
{
    /* 1D9C: */ const uint8_t tab1[] = { 0xD2 /* RND */, 0xD3 /* MID */, 0xD4 /* GETC */, 0 };
    int i;
    i = 0;
    while (tab1[i] != x) {
        if (tab1[i] == 0) {
            return 0;
        }
        ++i;
    }
    return 2;
}

/* 1DA0: dyadic operator '^' */
int parse_pwr(void)
{
    int x;

    if (ram[pc - RAMSTART] != '^') {
        return 0;
    }
    /* 1DA6: */
    ram[--evsp - RAMSTART] = ram[pc++ - RAMSTART];
    x = l220c();
    return (x < 0) ? x : 2;
}

/* 1DAE: relational/assignment operators */
int parse_relational(void)
{
    unsigned int x;
    int i;

    /* 1E0C: */ const uint8_t tab2[] = {
        (uint8_t)'=', 0x5C /* <> */, (uint8_t)'<', (uint8_t)'>', 0x5F /* <= */, 0x7E /* >= */, 0
    };
    i = 0;
    while (tab2[i] != ram[pc - RAMSTART]) {
        if (tab2[i] == 0) {
            return 0;
        }
        ++i;
    }
    /* 1DBC: */
    if (ram[0x824Au - RAMSTART] != 0 || (ram[0x824Bu - RAMSTART] & 0x40) != 0) {
        write16(0x8254, pc);
        return ERR2;
    }
    /* 1DCC: execute the operators from the expression evaluator stack */
    while (ram[evsp - RAMSTART] != 0) {
        i = exec_op((unsigned int)ram[evsp - RAMSTART]);
        if (i < 0) {
            return l260c(i);
        }
        ++evsp;
    }
    /* 1DD2: */
    ram[0x824Au - RAMSTART] = ram[pc - RAMSTART];
    ++pc;

    /* 1DD8: */
    if ((ram[0x824Bu - RAMSTART] & 0x80) != 0) {
        /* 1DDA: expected a numeric variable */
        if ((ram[datasp + 1 - RAMSTART] & 0x20) != 0) {
            /* 1DE0: */
            x = read16(datasp + 2); /* address of the variable */
            datasp += 8;
            /* 1DE8: copy the contents of the variable on the data stack */
            for (i = 8; i != 0; --i) {
                ram[--datasp - RAMSTART] = ram[--x - RAMSTART];
            }
            /* 1DF0: */
            if ((ram[datasp + 1 - RAMSTART] & 0x60) != 0) {
                write16(0x8254, --pc /*unparse the relational operator*/);
                return ERR6;
            }
        }
    }

    else {
        /* 1DFC: expected a string variable */
        if (ram[0x824Au - RAMSTART] != '=' || (ram[datasp + 1 - RAMSTART] & 0x60) == 0) {
            write16(0x8254, --pc /*unparse the relational operator*/);
            return ERR2;
        }
    }

    return 2;
}

/* 1E62: dyadic operators '+' and '-' */
int parse_addsub(void)
{
    int x;
    x = unary_op();
    if ((x & 2) == 0) {
        return 0; /* none found */
    }
    ram[--evsp - RAMSTART] = ((x & 1) == 0) ? '+' : '-';
    x = l21d8();
    return (x < 0) ? x : 1 /* loop to 1E4C */;
}

/* 1E7C: dyadic operators '*' and '/' */
int parse_muldiv(void)
{
    int x;
    if (ram[pc - RAMSTART] != '*' && ram[pc - RAMSTART] != '/') {
        return 0; /* none found */
    }
    /* 1E88: */
    ram[--evsp - RAMSTART] = ram[pc++ - RAMSTART];
    x = l21f2();
    return (x < 0) ? x : 2;
}

/* 1E90: parse for a variable */
int parse_variable(void)
{
    unsigned int i;

    /* 1E90: special string variable $ ? */
    if (ram[pc - RAMSTART] == '$') {
        ++pc;
        i = 0x814E; /* address of the $ variable buffer */
        if ((ram[0x824Bu - RAMSTART] & 0x80) != 0 || ram[pc - RAMSTART] != '=') {
            i = strlen(&ram[0x814Eu - RAMSTART]);
        }
        write16(datasp -= 2, 0);
        write16(datasp -= 2, i);      /* length of the string */
        write16(datasp -= 2, 0x814E); /* address of the $ variable buffer */
        write16(datasp -= 2, 0x4001); /* "special string variable" mark */
        return l1f80();
    }
    /* 1EBE: variable name A-Z ? */
    if (ram[pc - RAMSTART] >= 'A' && ram[pc - RAMSTART] <= 'Z') {
        write16(datasp -= 2, 0);
        write16(datasp -= 2, 0);
        /* 1ECA: address of the variable */
        write16(datasp -= 2, read16(0x8252) - 8 * (unsigned int)(ram[pc++ - RAMSTART] - 'A'));
        /* 1EDC: numeric or string variable? */
        i = 0x2000; /* "numeric variable" mark */
        if (ram[pc - RAMSTART] == '$') {
            ++pc;
            i = 0x4000; /* "string variable" mark */
        }
        write16(datasp -= 2, i);
        /* 1EF0: indexed variable? */
        if (ram[pc - RAMSTART] == '(') {
            ++pc;
            ram[--evsp - RAMSTART] = '[';
            return 2;
        }
        return l1fb2();
    }
    return 0; /* not a variable */
}

/* 1EFE: parse for a literal or a function without an argument (RAN#, KEY) */
int parse_literal(void)
{
    unsigned int i;

    /* string? */
    if (ram[pc - RAMSTART] == '\"') {
        i = ++pc;
        /* 1F0A: look for the end of the string */
        while (ram[pc++ - RAMSTART] != '\"')
            ;
        write16(datasp -= 2, 0);
        write16(datasp -= 2, pc - i - 1); /* length of the string */
        write16(datasp -= 2, i);          /* address of the string */
        write16(datasp -= 2, 0x4080);     /* "string" mark */
    }

    /* 1F1E: constant PI? */
    else if (ram[pc - RAMSTART] == 0x7C /* code of the token PI */) {
        ++pc;
        /* 1F24: push the PI constant on the stack */
        write16(datasp -= 2, (unsigned int)PI);
        write16(datasp -= 2, (unsigned int)(PI >> 16));
        write16(datasp -= 2, (unsigned int)(PI >> 32));
        write16(datasp -= 2, (unsigned int)(PI >> 48));
    }

    /* 1F36: numeric literal? */
    else if ((ram[pc - RAMSTART] >= '0' && ram[pc - RAMSTART] <= '9') ||
             ram[pc - RAMSTART] == '.') {
        return parse_number();
    }

    /* 1F50: function RAN# ? */
    else if (ram[pc - RAMSTART] == 0xD5) /* code of the token RAN# */
    {
        ++pc;
        write64(datasp -= 8, fp_ran());
    }

    /* 1F5C: function KEY? */
    else if (ram[pc - RAMSTART] == 0xD6) /* code of the token KEY */
    {
        ++pc;
        write16(datasp -= 2, (unsigned int)keyb0770());
        /* code of the pressed key */
        write16(datasp - 2, 1);       /* length of the string */
        write16(datasp - 4, datasp);  /* address of the string */
        write16(datasp -= 6, 0x4080); /* "string" mark */
    }

    /* 1F62: */
    else {
        return 0;
    }

    return l1f80();
}

/* 1F80: */
int l1f80(void)
{
    int st;

    /* 1E5A: end marker? */
    if (ram[pc - RAMSTART] == 0) {
        return l20c4();
    }

    /* 1F84: dyadic operators '+' and '-' */
    if ((st = parse_addsub()) != 0) {
    }

    /* 1F86: dyadic operators '*' and '/' */
    else if ((st = parse_muldiv()) != 0) {
    }

    /* 1F88: closing parenthesis */
    else if ((st = parse_cp()) != 0) {
    }

    /* 1F8A: dyadic operator '^' */
    else if ((st = parse_pwr()) != 0) {
    }

    /* 1F8C: parse for a comma */
    else if ((st = parse_comma()) != 0) {
    }

    /* 1F8E: relational operators */
    else if ((st = parse_relational()) != 0) {
    }

    /* 1F90: end of expression expected */
    else if ((st = l20c4()) != 0) {
    }

    /* 1F92: */
    else {
        write16(0x8254, pc);
        return ERR2;
    }

    return st;
}

/* 1F94: numeric literal */
int parse_number(void)
{
    uint64_t x;
    x = str2fp();
    write64(datasp -= 8, x);
    if (x != NOT_A_NUMBER) {
        return l1f80();
    }
    /* 1FA2: value out of range, unparse the exponent */
    do {
        --pc;
    } while (ram[pc - RAMSTART] != 0x7B /*E*/ && ram[pc - RAMSTART] != 0x7D /*E-*/);
    write16(0x8254, pc);
    return ERR2;
}

/* 1FB2: contents of the variable */
int l1fb2(void)
{
    unsigned int x;
    int i;

    /* 1FB2: */
    if ((ram[0x824Bu - RAMSTART] & 0x80) == 0 && ram[pc - RAMSTART] == '=') {
        return parse_relational(); /* relational operators */
    }

    /* 1FC2: */
    if ((ram[0x824Bu - RAMSTART] & 0x20) != 0 && ram[0x824Au - RAMSTART] == 0 &&
        (ram[pc - RAMSTART] == 0 || ram[pc - RAMSTART] == ':' || ram[pc - RAMSTART] == ',' ||
         ram[pc - RAMSTART] == 0xE1 /*TO*/ || ram[pc - RAMSTART] == '!')) {
        return l1f80();
    }

    /* 1FEC: */
    x = read16(datasp + 2); /* address of the variable */

    /* 1FF0: */
    if ((ram[datasp + 1 - RAMSTART] & 0x20) != 0) {
        /* expected a numeric variable */
        datasp += 8;
        /* 1FFA: copy the contents of the variable on the data stack */
        for (i = 8; i != 0; --i) {
            ram[--datasp - RAMSTART] = ram[--x - RAMSTART];
        }
        /* 2002: */
        if ((ram[datasp + 1 - RAMSTART] & 0x60) != 0) /* not a numeric variable? */
        {
            return unparse_var();
        }
    }

    else {
        /* 202C: expected a string variable */
        x -= 8;
        if ((ram[x + 1 - RAMSTART] & 0x60) == 0) /* not a string variable? */
        {
            return unparse_var();
        }
        /* 2036: determine the length of the string variable */
        i = 7;
        if (ram[x++ - RAMSTART] != 0) {
            ++x; /* skip the identifier of a string variable */
            do {
                ++ram[datasp + 4 - RAMSTART];
            } while (--i != 0 && ram[x++ - RAMSTART] != 0);
        }
    }

    return l1f80();
}

/* 2008: unparse a variable */
int unparse_var(void)
{
    int x;

    x = 0;
    do {
        if (ram[--pc - RAMSTART] == ')') {
            --x;
        }
        if (ram[pc - RAMSTART] == '(') {
            --pc;
            ++x;
        }
    } while (x < 0);
    if (ram[pc - RAMSTART] == '$') {
        --pc;
    }
    write16(0x8254, pc);
    return ERR6;
}

/* 204E: parse for a closing parenthesis */
int parse_cp(void)
{
    uint8_t x;
    int y, z;

    /* 204E: */
    if (ram[pc - RAMSTART] != ')') {
        return 0;
    }
    ++pc;
    /* 2058: the closing parenthesis can be either an index of a variable or a part
     of an expression, depending on what is on the expression evaluator stack */
    while ((x = ram[evsp++ - RAMSTART]) != '[' /*index of a variable*/) {
        if (x == 0) {
            write16(0x8254, pc);
            return ERR2;
        }
        if (x == '(') /* opening bracket of an expression? */
        {
            return l1f80();
        }
        y = exec_op((unsigned int)x);
        if (y < 0) {
            return l260c(y);
        }
    }

    /* 2072: indexed variable */
    y = (int)fp2int(read64(datasp));
    datasp += 8;
    if (y < -4095 || y > 4095) {
        return unparse_var();
    }
    /* 208C: subtract the index from the address of the variable */
    y = (int)read16(datasp + 2) - 8 * y;
    /* 20A0: check the boundaries... */
    z = (int)read16(0x8252 /*top of the RAM*/);
    if (y > z) /* ... against the top of the RAM */
    {
        return unparse_var();
    }
    z -= 8 * (int)read16(0x8250 /*number of variables*/);
    if (y < z) /* ... against the beginning of the variable area */
    {
        return unparse_var();
    }
    write16(datasp + 2, (unsigned int)y);
    return l1fb2();
}

/* 20C4: */
int l20c4(void)
{
    int i;

    if (ram[pc - RAMSTART] == 0 || is_term1(ram[pc - RAMSTART]) != 0) {
        /* 20D6: execute the operators from the expression evaluator stack */
        while (ram[evsp - RAMSTART] != 0) {
            i = exec_op((unsigned int)ram[evsp - RAMSTART]);
            if (i < 0) {
                return l260c(i);
            }
            ++evsp;
        }
        return l20de();
    }
    return 0;
}

/* 20C8: returns not zero if 'x' is a code of an expression terminating
 operator */
int is_term1(uint8_t x)
{
    /* 21D0: */ const uint8_t tab3[] = {
        ':', ';', '!', 0xE0 /*THEN*/, 0xE1 /*TO*/, 0xE2 /*STEP*/, 0
    };
    int i;
    i = 0;
    while (tab3[i] != x) {
        if (tab3[i] == 0) {
            return 0;
        }
        ++i;
    }
    return 2;
}

/* 20DE: */
int l20de(void)
{
    unsigned int src;  /* source address */
    unsigned int dst;  /* destination address */
    unsigned int size; /* size of the destination string variable */
    unsigned int len;  /* length of the source string */
    uint8_t x;
    int i;

    write16(0x8254, pc);
    x = ram[0x8242u - RAMSTART] = ram[0x824Au - RAMSTART];
    ram[0x8243u - RAMSTART] = ram[0x824Bu - RAMSTART];
    if ((ram[0x824Bu - RAMSTART] & 0x80) != 0) {
        return (x == 0) ? ERR2 : DONE;
    }
    if (x == 0) {
        return DONE;
    }
    if (x != '=') {
        return ERR2;
    }

    /* 2106: assignment of a value to a variable */
    if ((ram[datasp + 1 - RAMSTART] & 0x60) == 0) {
        /* 210C: assignment of a numeric value to a numeric variable */
        if ((ram[datasp + 9 - RAMSTART] & 0x20) == 0) {
            return ERR2; /* not a numeric variable */
        }
        dst = read16(datasp + 10); /* address of the variable */
        dst -= 8;
        for (i = 8; i != 0; --i) {
            ram[dst++ - RAMSTART] = ram[datasp++ - RAMSTART];
        }
        dst = datasp;
        datasp -= 8;

        /* 2126: */
        if ((ram[0x824Bu - RAMSTART] & 0x10) != 0) {
            for (i = 8; i != 0; --i) {
                ram[dst++ - RAMSTART] = ram[datasp++ - RAMSTART];
            }
        }
        /* 2138: */
        else {
            datasp += ((ram[0x824Bu - RAMSTART] & 0x20) != 0) ? 8 : 16;
        }
    }

    else {
        /* 2146: assignment of a string to a string variable */
        if ((ram[datasp + 9 - RAMSTART] & 0x40) == 0 || /* not a string variable? */
            (ram[datasp + 8 - RAMSTART] & 0x80) != 0) {
            return ERR2;
        }
        dst = read16(datasp + 10); /* address of the variable */
        size = 30;                 /* size of the special string variable '$' */
        if (ram[datasp + 8 - RAMSTART] == 0) {
            /* 215E: ordinary string variable */
            dst -= 8;
            ram[dst - RAMSTART] = 0;
            ram[dst + 1 - RAMSTART] = 0x60;
            size = 7; /* size of an ordinary string variable */
        }
        len = read16(datasp + 4); /* length of the string */
        if (size < len) {
            return l260c(ERR8); /* string too long */
        }

        /* 2176: copy the string unless it's empty */
        if (len != 0) {
            i = (int)len;
            src = read16(datasp + 2); /* address of the string */

            /* 217E: special treatment of the first character */
            if (ram[datasp - RAMSTART] == 0) {
                /* 2182: source = ordinary string variable */
                src -= 8;
                x = ram[src++ - RAMSTART];
                ++src; /* skip the identifier of a string variable */
            } else {
                /* 218C: source = string literal or the special string variable '$' */
                x = ram[src++ - RAMSTART];
            }

            /* 218E: remaining characters */
            do {
                if (ram[datasp + 8 - RAMSTART] == 0) {
                    /* 2194: destination = first character of an ordinary string variable */
                    ram[dst++ - RAMSTART] = x;
                    ++dst;                           /* skip the identifier of a string variable */
                    ram[datasp + 8 - RAMSTART] |= 2; /* make it non-zero */
                } else {
                    /* 21A0: destination = special string variable '$' or remaining characters of
                     an ordinary string variable */
                    ram[dst++ - RAMSTART] = x;
                }
                /* next character from the source string */
                if (--i != 0) {
                    x = ram[src++ - RAMSTART];
                }
            } while (i != 0);
        }

        datasp += 8;
        ram[datasp - RAMSTART] &= ~2;
        write16(datasp + 4, len);
        if (ram[datasp - RAMSTART] != 0 /*destination = '$'*/ || len < 7) {
            /* 21BA: append terminating zero */
            ram[dst - RAMSTART] = 0;
        }
        /* 21BC: */
        if ((ram[0x824Bu - RAMSTART] & 0x30) == 0) {
            datasp += 8;
        }
    }

    /* 21C8: */
    return DONE;
}

/* 21D8: */
int l21d8(void)
{
    uint8_t x, y;
    int i;

    while (1) {
        x = ram[evsp++ - RAMSTART];
        y = ram[evsp - RAMSTART];
        /* 21E4: */
        if (y != '+' && y != '-' &&
            /* 21FE: */
            y != '*' && y != '/' &&
            /* 2218: */
            y != '^' && y != '%' /*unary minus*/ && y < 0xC0 /*function*/) {
            ram[--evsp - RAMSTART] = x;
            return 0;
        }
        /* 2230: */
        ram[evsp - RAMSTART] = x;
        i = exec_op((unsigned int)y);
        if (i < 0) {
            return l260c(i);
        }
    }
}

/* 21F2: */
int l21f2(void)
{
    uint8_t x, y;
    int i;

    while (1) {
        x = ram[evsp++ - RAMSTART];
        y = ram[evsp - RAMSTART];
        /* 21FE: */
        if (y != '*' && y != '/' &&
            /* 2218: */
            y != '^' && y != '%' /*unary minus*/ && y < 0xC0 /*function*/) {
            ram[--evsp - RAMSTART] = x;
            return 0;
        }
        /* 2230: */
        ram[evsp - RAMSTART] = x;
        i = exec_op((unsigned int)y);
        if (i < 0) {
            return l260c(i);
        }
    }
}

/* 220C: */
int l220c(void)
{
    uint8_t x, y;
    int i;

    while (1) {
        x = ram[evsp++ - RAMSTART];
        y = ram[evsp - RAMSTART];
        /* 2218: */
        if (y != '^' && y != '%' /*unary minus*/ && y < 0xC0 /*function*/) {
            ram[--evsp - RAMSTART] = x;
            return 0;
        }
        /* 2230: */
        ram[evsp - RAMSTART] = x;
        i = exec_op((unsigned int)y);
        if (i < 0) {
            return l260c(i);
        }
    }
}

/* 260C: search backwards for the command/function/operator of the code stored
 at the RAM address 0x8242 */
int l260c(int error_code)
{
    unsigned int cnt;

    pc = read16(0x8254);
    cnt = 0;
    do {
        /* 2612: test for even number of quotes */
        do {
            if (ram[--pc - RAMSTART] == '\"') {
                cnt ^= 0x8000;
            }
        } while ((cnt & 0x8000) != 0);
        /* 2620: test for matching number of parentheses */
        if (ram[pc - RAMSTART] == ')') {
            ++cnt;
        } else if (ram[pc - RAMSTART] == '(') {
            --cnt;
        }
        if (cnt < 0) {
            /* 2634: unmatched opening parenthesis encountered */
            error_code = ERR_EX;
            break;
        }
    } while (cnt > 0 || ram[pc - RAMSTART] != ram[0x8242u - RAMSTART]);
    /* 265A: */
    write16(0x8254, pc);
    return error_code;
}

/* 268A: evaluate a chain of unary operators '+' and '-'
 returns bit 0 set if evaluated to '-'
 returns bit 1 set if at least one operator found */
int unary_op(void)
{
    int x;
    x = 0;
    while (ram[pc - RAMSTART] == '+' || ram[pc - RAMSTART] == '-') {
        if (ram[pc - RAMSTART] == '-') {
            x ^= 1;
        }
        x |= 2;
        ++pc;
    }
    return x;
}

/* 2FC6: function RAN#
 algorithm:
 seed = (12869*seed + 6925) and 32767
 RAN# = seed/32768 */
uint64_t fp_ran(void)
{
    uint16_t seed;

    seed = read16(0x824E);
    seed = (uint16_t)(((uint32_t)12869 * (uint32_t)seed + 6925) & 32767);
    write16(0x824E, seed);
    return fp_mul(int2fp((int16_t)seed), 0x0FFC305175781250);
    /* number 0.30517578125E-4 = 1/32768 */
}
