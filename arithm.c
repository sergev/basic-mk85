/* arithmetical routines, hardware independent module */

#include <stdint.h>
#include "arithm.h"
#include "common.h"
#include "main.h"	/* ram */

extern uint8_t ram[RAMSIZE];
extern unsigned int pc;


/* private function prototypes */
uint64_t bcd_add (uint64_t x1, uint64_t x2);
uint64_t bcd_sub (uint64_t minuend, uint64_t subtrahend);


/* 28CC: convert an FP number to a string */
void fp2str (uint64_t fpnumber, int spec_precision, char *destination)
{
  uint16_t sign;
  uint16_t exponent;
  char buffer[16];

  char *bufptr;		/* r5 in the original procedure */
  char *destptr;	/* r4 in the original procedure */
  int act_precision;	/* r4 in the original procedure */
  int i, j;
  uint8_t x;

  bufptr = buffer;
  *bufptr++ = '0';

  exponent = (uint16_t) (fpnumber>>48);
  sign = exponent & 0x8000;
  exponent &= 0x1FFF;
  exponent -= 0x1000;		/* remove the exponent bias */

/* 28F8: */
  act_precision = -spec_precision;
  if (spec_precision >= 0 || act_precision > 10)
  {
    act_precision = 10;		/* default printing precision */
  }

/* 290E: convert act_precision+1 digits of the mantissa to ASCII,
 done differently than in the original code, but gives the same result */
  j = 44 - 4*act_precision;
  for (i=44; i>=j; i-=4)
  {
    *bufptr++ = '0' + ((uint8_t) (fpnumber>>i) & 0x0F);
  }

/* 292C: rounding (the last digit will be dropped) */
  if (*--bufptr >= '5')
  {
    while (++(*--bufptr) > '9')
      ;
    ++bufptr;
  }

/* 293C: remove trailing zeros */
  do {
    *bufptr = '\0';	/* truncate the string */
    if (*--bufptr != '0')
    {
/* 294A: remove the leading zero */
      bufptr = buffer;
      if (*bufptr++ != '0')
      {
        --bufptr;
        ++exponent;
      }
      break;
    }
  } while (bufptr > buffer);

/* 2958: output the minus sign for negative numbers */
  destptr = destination;
  *destptr++ = '-';
  if ((sign &= 0x8000) == 0)
  {
/* 2966: space before a positive number, unless maximal precision set */
    *--destptr = ' ';
    if (spec_precision < 0)
    {
      ++destptr;
    }
  }

/* 2972: */
  i = (int) (int16_t) exponent;	/* i corresponds to r2 in the original code */

  if (i > 0)		/* is the absolute value of the number >= 1 ? */
  {
    if (i <= act_precision)
    {
      exponent = 0;
    }
  }

  else if (i == -0x1000) /* is the number = 0 ? */
  {
    exponent = 0;
  }

  else if ((i = -i) <= 2) /* is the absolute value of the number >= 0.001 ? */
  {
/* 2988: */
    if (spec_precision >= 0)
    {
      act_precision -= i;
      if (destptr != destination)
      {
        --act_precision;
      }
    }
/* 2998: fractional number in normal (not scientific) display format */
    *destptr++ = '0';
    *destptr++ = '.';
/* 29A0: zeros between the decimal point and the mantissa */
    while (--i >= 0)
    {
      *destptr++ = '0';
    }
    if (i <= act_precision)
    {
      exponent = 0;
    }
  }

/* otherwise scientific notation */

  if (exponent != 0)		/* scientific notation? */
  {
/* 29B4: */
    if (spec_precision >= 0)
    {
      act_precision = 3;
      if (destptr == destination)
      {
        ++act_precision;
      }
/* 29C6: count the digits of the exponent and adjust act_precision accordingly,
   original loop unrolled */
      if (i < 1000)
      {
        ++act_precision;
        if (i < 100)
        {
          ++act_precision;
          if (i < 10)
          {
            ++act_precision;
          }
        }
      }
    }

/* 29D6: */
    --exponent;
    i = 1;
  }
  
/* 29DE: this loop outputs act_precision digits of the the mantissa,
   i = number of digits before the decimal point */
  do {
    *destptr++ = *bufptr++;
    if (*bufptr == '\0')
    {
/* 29F0: this loop outputs trailing zeros */
      while (--i > 0)
      {
        *destptr++ = '0';
      }
      break;
    }
/* 29E4: */
    if (--i == 0)
    {
      *destptr++ = '.';
    }
/* 29EC: */
  } while (--act_precision != 0);

/* 29F8: output the exponent */
  if (exponent != 0)	/* scientific notation? */
  {
    x = 0x7B;			/* E */
    if ((exponent & 0x8000) != 0)
    {
      x = 0x7D;			/* E- */
      exponent = -exponent;
    }
    *destptr++ = x;
    destptr = int2str (exponent, destptr);
  }

/* 2A16: terminate the output string */
  *destptr = '\0';
}


/* 2A22: convert an unsigned 16-bit integer to decimal ASCII,
 done differently than in the original code, but gives the same result */
char* int2str (uint16_t number, char *destination)
{
  char buffer[5];
  char *bufptr = buffer;

  do {
    *bufptr++ = '0' + (uint8_t) (number % 10);
    number /= 10;
  } while (number != 0);
  do {
    *destination++ = *--bufptr;
  } while (bufptr != buffer);
  return destination;
}


/* 2A5C: evaluate a floating point numeric literal,
 returns NOT_A_NUMBER if out of range */
uint64_t str2fp (void)
{
  uint64_t mantissa = 0;
  uint16_t exponent /* and signs */ = 0x100C;
  unsigned int n;
  char c;
  int x;

/* 2A6A: parse the sign of the mantissa */
  while (ram[pc-RAMSTART] == '+' || ram[pc-RAMSTART] == '-')
  {
    if (ram[pc-RAMSTART] == '-')
    {
      exponent ^= 0x8000;	/* change the sign of the mantissa */
    }
    ++pc;
  }

/* 2A7E: parse the mantissa digits before the decimal point */
  while ((x = get_digit((int) ram[pc-RAMSTART])) >= 0)
  {
    ++pc;
    if ((mantissa & ~0xFFFFFFFFFFF) == 0) /* less than 12 digits parsed? */
    {
    /* insert the digit */
      mantissa <<= 4;
      mantissa |= (uint64_t) (int64_t) x;
    }
    else
    {
    /* ignore further digits, only increment the exponent */
      ++exponent;
    }
  }

/* 2A94: parse the decimal point */
  if (ram[pc-RAMSTART] == '.')
  {
    ++pc;
/* 2A9A: parse the mantissa digits after the decimal point */
    while ((x = get_digit((int) ram[pc-RAMSTART])) >= 0)
    {
      ++pc;
      if ((mantissa & ~0xFFFFFFFFFFF) == 0) /* less than 12 digits parsed? */
      {
      /* insert the digit */
        mantissa <<= 4;
        mantissa |= (uint64_t) (int64_t) x;
        --exponent;
      }
      /* otherwise ignore further digits */
    }
  }

/* 2AAE: test the mantissa for 0 */
  if (mantissa == 0)
  {
    return 0;		/* special case, value 0 */
  }

/* 2ABA: normalisation - shift the mantissa left until the first digit != 0 */
  while ((mantissa & ~0xFFFFFFFFFFF) == 0)
  {
    mantissa <<= 4;
    --exponent;
  }

/* 2AC4: scientific notation */
  n = 0; /* value of the number after 'E' or 'E-', r3 in the original code */
  c = ram[pc-RAMSTART];
  if (c == 0x7B /* 'E' */ || c == 0x7D /* 'E-' */)
  {
    ++pc;
/* 2AD6: parse the number after 'E' or 'E-' */
    while ((x = get_digit((int) ram[pc-RAMSTART])) >= 0)
    {
      ++pc;
      n *= 10;
      n += x;
      if (n > 16383)
      {
        return NOT_A_NUMBER;
      }
    }
/* 2AF2: */
    if (c == 0x7D /* 'E-' */)
    {
      if (exponent <= (uint16_t) n)
      {
        return NOT_A_NUMBER;
      }
      exponent -= (uint16_t) n;
    }
    else
    {
      exponent += (uint16_t) n;
      if ((exponent & 0x6000) != 0)
      {
        return NOT_A_NUMBER;
      }
    }
  }

  mantissa |= (((uint64_t) exponent) << 48);
  return mantissa;
}


/* 2B1C: value of an ASCII digit, NOT_A_NUMBER if not a digit */
int get_digit (int x)
{
  x -= 0x30;
  if (x < 0 || x > 9)
  {
    x = NOT_A_NUMBER;
  }
  return x;
}


/* 2B42: floating point subtraction */
uint64_t fp_sub (uint64_t x1, uint64_t x2)
{
  return fp_add (x1, fp_neg (x2));
}


/* 2B46: floating point addition */
uint64_t fp_add (uint64_t x1, uint64_t x2)
{
  uint16_t e1, e2;	/* biased exponents */
  uint16_t s1, s2;	/* signs */
  uint64_t x3;

/* if the absolute value of the first addend is less than the absolute value of
 the second addend, swap them */
  if (fp_abs (x1) < fp_abs (x2))
  {
    x3 = x1;
    x1 = x2;
    x2 = x3;
  }

/* if the second addend is equal 0, return the first one */
  if (x2 == 0)
  {
    return x1;
  }

  e1 = (uint16_t) (x1>>48);
  e2 = (uint16_t) (x2>>48);
  s1 = e1 & 0x8000;
  e1 &= 0x1FFF;
  s2 = e2 & 0x8000;
  e2 &= 0x1FFF;

  e2 = e1 - e2;
/* if the difference between exponents is more than 12, the second addend is
 insignificant */
  if (e2 > 12)
  {
    return x1;
  }

/* both addends are significant */
  x1 &= 0xFFFFFFFFFFFF;		/* separate the mantissa */
  x2 &= 0xFFFFFFFFFFFF;

/* align the smaller addend with the bigger one */
  while (e2 != 0)
  {
    x2 >>= 4;
    --e2;
  }

  if (s1 == s2)
  {
    x1 = bcd_add (x1, x2);
/* a BCD addition can result in 13 digits */
    if ((x1 & ~0xFFFFFFFFFFFF) != 0)
    {
/* normalisation to 12 digits */
      x1 >>= 4;
      ++e1;
    }
  }
  else
  {
    x1 = bcd_sub (x1, x2);
/* a BCD subtraction can result in 0 */
    if (x1 == 0)
    {
      return 0;
    }
/* normalisation */
    while ((x1 & ~0xFFFFFFFFFFF) == 0)
    {
      x1 <<= 4;
      --e1;
    }
  }

  if ((e1 & 0x6000) != 0 || e1 == 0)
  {
    return NOT_A_NUMBER;
  }

  return (((uint64_t) (s1 | e1)) << 48) | x1;
}


/* 2C7E: floating point multiplication */
uint64_t fp_mul (uint64_t x1, uint64_t x2)
{
  uint16_t e1;
  uint64_t product;
  int i;

/* 2C88: return 0 if either of the factors is equal zero */
  if (x1 == 0 || x2 == 0)
  {
    return 0;
  }

/* 2C90: calculate the exponent of the product - add the exponents of both
 factors */
  e1 = (uint16_t) (x1 >> 48) + (uint16_t) (x2 >> 48) - 0x1001;

  x1 &= 0xFFFFFFFFFFFF;
  x2 &= 0xFFFFFFFFFFFF;
  x1 <<= 4;
  product = 0;
  i = 12;
/* 2CB6: */
  while (1)
  {
    while ((x2 & 0xF) != 0)
    {
      --x2;
      product = bcd_add (product, x1);
    }
    if (--i == 0)
    {
      break;
    }
    x2 >>= 4;
    product >>= 4;
  }

/* 2CDA: rounding and normalisation */
  while (1)
  {
    product = bcd_add (product, 5) >> 4;
    if ((product & ~0xFFFFFFFFFFFF) == 0)
    {
      break;
    }
    ++e1;
  }

  if ((e1 & 0x6000) != 0 || e1 == 0)
  {
    return NOT_A_NUMBER;
  }

  return (((uint64_t) e1) << 48) | product;
}


/* 2CFC: floating point division */
uint64_t fp_div (uint64_t dividend, uint64_t divisor)
{
  uint64_t quotient;
  uint16_t e1;

  if (divisor == 0)
  {
    return NOT_A_NUMBER;
  }
  if (dividend == 0)
  {
    return 0;
  }

/* 2D14: calculate the exponent of the quotient */
  e1 = 0x100D + (uint16_t) (dividend >> 48) - (uint16_t) (divisor >> 48);

  dividend &= 0xFFFFFFFFFFFF;
  divisor &= 0xFFFFFFFFFFFF;
  quotient = 0;
  while (1)
  {
/* 2D2C: repeated subtraction of the divisor from the dividend, the quotient is
 incremented at every subtraction */
    while (dividend >= divisor)
    {
      dividend = bcd_sub (dividend, divisor);
      ++quotient;
    }

/* 2D50: the shift/subtraction loop is repeated until we get 13 digits of the
 quotient (i.e. the 13th digit is not equal 0) */
    if ((quotient & ~0xFFFFFFFFFFFF) != 0)
    {
      break;
    }
    --e1;
    quotient <<= 4;
    dividend <<= 4;
  }

/* 2DE8: rounding and normalisation */
  while (1)
  {
    quotient = bcd_add (quotient, 5) >> 4;
    if ((quotient & ~0xFFFFFFFFFFFF) == 0)
    {
      break;
    }
    ++e1;
  }

  if ((e1 & 0x6000) != 0 || e1 == 0)
  {
    return NOT_A_NUMBER;
  }

  return (((uint64_t) e1) << 48) | quotient;
}


/* 2CF6: remainder of a division, additionally returns the two least
 significant bits of an integer quotient on otherwise cleared bits 62..61
 equivalent of:
 remainder = divisor * FRAC (dividend / divisor)
 quotient = INT (dividend / divisor) */
uint64_t fp_mod (uint64_t dividend, uint64_t divisor)
{
  uint16_t e1, e2, s, q;

  if (divisor == 0)
  {
    return NOT_A_NUMBER;
  }
  if (dividend == 0)
  {
    return 0;
  }

  e1 = (uint16_t) (dividend >> 48);
  e2 = (uint16_t) (divisor >> 48);
  s = (e1 ^ e2) & 0x8000;
  e1 &= 0x1FFF;
  e2 &= 0x1FFF;
  if (e1 < e2)
  {
    return dividend;
  }
  if (e1 - e2 > 13)
  {
    return 0;		/* the calculation doesn't make sense */
  }
  dividend &= 0xFFFFFFFFFFFF;
  divisor &= 0xFFFFFFFFFFFF;
  q = 0;

  while (1)
  {
/* repeated subtraction of the divisor from the dividend */
    while (dividend >= divisor)
    {
      dividend = bcd_sub (dividend, divisor);
      ++q;
    }
    if (e1 == e2)
    {
      break;
    }
    dividend <<= 4;
    q *= 10;
    --e1;
  }

  q &= 3;
  q <<= 13;

  if (dividend == 0)
  {
    return (uint64_t) q << 48;
  }
  
/* normalisation */
  while ((dividend & ~0xFFFFFFFFFFF) == 0)
  {
    dividend <<= 4;
    --e1;
  }

  if ((e1 & 0xE000) != 0 || e1 == 0)
  {
    return NOT_A_NUMBER;
  }

  return (((uint64_t) (s | q | e1)) << 48) | dividend;
}


/* 2E12: floating point absolute value */
uint64_t fp_abs (uint64_t x)
{
  x &= 0x1FFFFFFFFFFFFFFF;	/* clear the sign */
  return x;
}


/* 2E18: change the sign of a floating point number */
uint64_t fp_neg (uint64_t x)
{
  if (x != 0)
  {
    x ^= 0x8000000000000000;
  }
  return x;
}


/* 2E22: sign of a floating point number */
uint64_t fp_sgn (uint64_t x)
{
  if (x != 0)
  {
    x &= 0x8000000000000000;	/* keep the original sign... */
    x |= ONE;			/* ...but replace the value with 1.0 */
  }
  return x;
}


/* 2E3A: floating point rounding */
uint64_t fp_rnd (uint64_t x, uint64_t y)
{
  int16_t n;
  uint16_t e;

  n = fp2int (y);
  if (n == -32768)
  {
    return NOT_A_NUMBER;
  }
  if (x == 0)
  {
    return 0;
  }
  e = (uint16_t) (x >> 48);
  x &= 0xFFFFFFFFFFFF;		/* expand the mantissa to 64 bits */
  n -= (int16_t) ((e & 0x1FFF) - 0x100C);
  if (n >= 0)
  {
    if (n > 0)
    {
/* 2E6C: */
      if (n > 11)
      {
        return 0;
      }
      x >>= 4*n;		/* drop the last n digits */
      e += (uint16_t) n;
    }
/* 2E82: rounding */
    x = bcd_add (x, 5);
    x &= ~0xF;
    if (x == 0)
    {
      return 0;
    }
/* 2E9E: a BCD addition can result in 13 digits */
    if ((x & ~0xFFFFFFFFFFFF) != 0)
    {
/* normalisation to 12 digits */
      x >>= 4;
      ++e;
    }
/* normalisation */
    while ((x & ~0xFFFFFFFFFFF) == 0)
    {
      x <<= 4;
      --e;
    }
    if ((e & 0x6000) != 0)
    {
      return NOT_A_NUMBER;
    }
  }
  return (((uint64_t) e) << 48) | x;
}


/* 2EA2: floating point fraction */
uint64_t fp_frac (uint64_t x)
{
  uint16_t e, s;

  e = (uint16_t) (x >> 48);
  s = e & 0x8000;	/* sign */
  e &= 0x1FFF;		/* clear the sign */
  if (e <= 0x1000)	/* do nothing when the exponent is negative or zero */
  {
    return x;
  }
  e -= 0x1000;		/* remove the exponent bias */
  if (e >= 12)		/* 12 or more digits before the decimal point? */
  {
    return 0;
  }
  x <<= 4*e;		/* drop the digits before the decimal point */
  x &= 0xFFFFFFFFFFFF;
  if (x == 0)
  {
    return 0;
  }
  e = 0x1000;
/* normalisation */
  while ((x & ~0xFFFFFFFFFFF) == 0)
  {
    x <<= 4;
    --e;
  }
  return (((uint64_t) (s | e)) << 48) | x;
}


/* 2EE4: floating point integer value */
uint64_t fp_int (uint64_t x)
{
  uint16_t e;

  e = (uint16_t) (x >> 48);
  e &= 0x1FFF;		/* clear the sign */
  if (e <= 0x1000)	/* return 0 when the exponent is negative or zero */
  {
    return 0;
  }
  e -= 0x1000;		/* remove the exponent bias */
  if (e >= 12)		/* do nothing when 12 or more digits before the decimal
			   point */
  {
    return x;
  }
  e = 4*(12-e);
  x >>= e;		/* drop the digits after the decimal point */
  x <<= e;		/* digits after the decimal point are replaced with
			   zeros */
  return x;
}


/* 2F18: convert a 16-bit signed integer to FP,
 done differently than in the original code, but gives the same result */
uint64_t int2fp (int16_t x)
{
  uint64_t y;
  uint16_t e;

  if (x == 0)
  {
    return 0;
  }
  e = 0x1000;
  if (x < 0)
  {
    x = -x;
    e = 0x9000;
  }

  y = 0;
  do {
    y |= ((uint64_t) (x % 10)) << 48;
    y >>= 4;
    ++e;
    x /= 10;
  } while (x != 0);

  return (((uint64_t) e) << 48) | y;
}


/* 2F72: convert a FP number a to 16-bit signed integer,
 allowed range -32767 to +32767,
 returns -32768 if the conversion failed */
int16_t fp2int (uint64_t x)
{
  uint16_t e, s, y;

  e = (uint16_t) (x >> 48);	/* biased exponent and sign */
  s = e & 0x8000;		/* sign */
  e &= 0x1FFF;			/* drop the sign */
  if (e <= 0x1000)		/* if the number is less than 1 */
  {
    return 0;
  }
  e -= 0x1000;			/* remove the bias */
  if (e > 5)
  {
    return -32768;		/* number too big, conversion failed */
  }

  y = 0;
  do {
    if ((y & 0xF000) != 0)
    {
      return -32768;		/* number too big, conversion failed */
    }
    x <<= 4;
    y *= 10;
    y += ((uint16_t) (x >> 48)) & 0xF;
    if ((y & 0x8000) != 0)
    {
      return -32768;		/* number too big, conversion failed */
    }
  } while (--e != 0);

  if (s != 0)
  {
    y = -y;
  }
  return (int16_t) y;
}


/* The following arithmetic functions use algorithms described by J.E.Meggitt
 in the IBM Journal (April 1962) in an article "Pseudo Division and Pseudo
 Multiplication processes", which can be treated as implementation of the
 CORDIC method developed by J.Volder. */

/* 300E: floating point square root */
uint64_t fp_sqr (uint64_t a)
{
  uint64_t b, d, q;
  uint16_t e;

  if (a == 0)
  {
    return 0;
  }
  e = (uint16_t) (a >> 48);
  if ((e & 0x8000) != 0)	/* negative radicand? */
  {
    return NOT_A_NUMBER;
  }
  a &= 0xFFFFFFFFFFFF;		/* 12 digits */
  if ((e & 1) == 0)		/* even exponent? */
  {
    a <<= 4;
    --e;
  }
  e /= 2;
  e += 0x0801;

  b = 0x1000000000000;		/* 1 << (4*12) */
  d = b;
  q = 0;
  a <<= 4;

/* 13 iterations to calculate 13 digits of q */
  while (d != 0)
  {
    while (a >= b)
    {
      a = bcd_sub (a, b);
      b = bcd_add (b, d);
      b = bcd_add (b, d);
      ++q;
    }
    if (q == 0 || (q & ~0xFFFFFFFFFFFF) != 0 /*if q has 13 digits*/)
    {
      break;
    }
    a <<= 4;
    q <<= 4;
    b = bcd_sub (b, d);
    d >>= 4;
    b = bcd_add (b, d);
  }

/* rounding and normalisation */
  q = bcd_add (q, 5) >> 4;
  if ((q & ~0xFFFFFFFFFFFF) != 0 /*if q has 13 digits*/)
  {
    q >>= 4;
    ++e;
  }

  return (((uint64_t) e) << 48) | q;
}


/* 3256: table of constants 10^j * ln (1+10^-j) */
const uint64_t ln_tab[] = {
  0x0006931471805600,	/* ln 2 = 0.6931471805600 */
  0x0009531017980432,	/* ln 1.1 = 0.09531017980432 */
  0x0009950330853168,	/* ln 1.01 = 0.009950330853168 */
  0x0009995003330835,	/* ln 1.001 = 0.0009995003330835 */
  0x0009999500033330,	/* ln 1.0001 = 0.00009999500033330 */
  0x0009999950000343,	/* ln 1.00001 = 0.000009999950000343 */
  0x0009999995000013	/* ln 1.000001 = 0.0000009999995000013 */
};


/* 30A2: function EXP */
uint64_t fp_exp (uint64_t x)
{
  uint64_t a, b, q, m;
  uint16_t e, s;
  int16_t n;
  int j;

/* the argument is converted to the form: n * LN 10 + a
   EXP (n * LN 10 + a) = 10^n * EXP (a)
   a = x mod (LN 10), argument scaled to range 0..LN 10
   n = x div (LN 10), scaling factor */
  a = fp_div (x, LN10);
  if (a == NOT_A_NUMBER)
  {
    return NOT_A_NUMBER;
  }
  n = fp2int (a);
  if (n == -32768)
  {
    return NOT_A_NUMBER;
  }
  a = fp_mod (x, LN10);		/* in the original code calculated with
				   a formula: LN10 * FRAC (x / LN10) */
  if (a == NOT_A_NUMBER)
  {
    return NOT_A_NUMBER;
  }
  e = (uint16_t) (a >> 48);	/* biased exponent and sign */
  a &= 0xFFFFFFFFFFFF;		/* mantissa */
  s = e & 0x8000;		/* sign */

/* 30CE: */
  if (e != 0)
  {
    e &= 0x1FFF;		/* biased exponent */
    if (e > 0x1000)
    {
      a <<= 4;
      --e;
    }
    e -= 0x1000;		/* remove the bias */
  }

/* 3102: */
  q = 0;
  a <<= 4;
  
/* 3164: First iteration:
 the scaled argument will be expressed as a sum of terms q[j]*ln(1+10^-j) */
  j = 0;
  while (1)
  {
    b = (j - (int) (int16_t) e < 7) ? ln_tab[j - (int) (int16_t) e] :
	0x0009999999500288;
/* ln 1.0000001 = 0.00000009999999500288 */
    while (a >= b)
    {
      a = bcd_sub (a, b);
      ++q;
    }
    if (j >= 12 && ((q == 0) || (q & ~0xFFFFFFFFFFFF) != 0))
    {
      break;
    }
    a <<= 4;
    q <<= 4;
    ++j;
  }

/* 3172: Second iteration:
 calculate EXP(X)-1 using pseudo multiplication */
  a = 0;
  b = 0x0001000000000000;
  while (1)
  {
    while ((q & 0xF) != 0)
    {
/* shifting a number of "uint64_t" type by more than 64 bits may be illegal */
      m = (j - (int) (int16_t) e < 16) ? b >> (4 * (j - (int) (int16_t) e)) : 0;
      a = bcd_add (a, b);
      b = bcd_add (b, m);
      --q;
    }
    if (q == 0)
    {
      break;
    }
    a >>= 4;
    q >>= 4;
    --j;
  }

/* 3188: */
  a = bcd_add (a >> (4 * (j - (int) (int16_t) e)), 0x0001000000000000);

/* 318E: rounding */
  a = bcd_add (a, 5);
  a >>= 4;

/* 310A: */
  if (n < 0)
  {
    n = -n;
  }
  e = 0x1001 + (uint16_t) n;
  if ((e & 0xE000) != 0)
  {
    return NOT_A_NUMBER;
  }
  a |= ((uint64_t) e) << 48;
  if (s != 0)
  {
/* reciprocal for negative arguments, EXP(-X) = 1/EXP(X) */
    a = fp_div (ONE, a);
  }

  return a;
}


/* 3196: function LOG (x) = LN (x) / LN (10) */
uint64_t fp_log (uint64_t x)
{
  x = fp_ln (x);
  if (x == NOT_A_NUMBER)
  {
    return NOT_A_NUMBER;
  }
  return fp_div (x, LN10);
}


/* 31AC: natural logarithm */
uint64_t fp_ln (uint64_t a)
{
  uint64_t b, q;
  uint16_t e1, e2;
  int j;

  e1 = (uint16_t) (a >> 48);	/* biased exponent and sign */
  if ((int16_t) e1 <= 0)
  {
    return NOT_A_NUMBER;
  }
  e1 -= 0x1001;

/* 31D6: */
  a &= 0xFFFFFFFFFFFF;
  q = 0;
  b = 0x0001000000000000;
  a = bcd_sub (a << 4, b);

/* 322A: First iteration:
 the mantissa will be expressed as a product of powers (1+10^-j)^q[j] using
 pseudo division */
  j = 0;
  while (1)  
  {
    while (a >= b)
    {
      a = bcd_sub (a, b);
      b = bcd_add (b, b >> (4*j));
      ++q;
    }
    if (j >= 12 && ((q == 0) || (q & ~0xFFFFFFFFFFFF) != 0))
    {
      break;
    }
    a <<= 4;
    q <<= 4;
    ++j;
  }

/* 323A: Second iteration:
 calculate LN(x) as a sum of terms q[j]*LN(1+10^-J) using pseudo multiplication
*/
  a = 0;
  while (1)
  {
    b = (j < 7) ? ln_tab[j] : 0x0009999999500288;
    while ((q & 0xF) != 0)
    {
      a = bcd_add (a, b);
      --q;
    }
    if (q == 0)
    {
      break;
    }
    a >>= 4;
    q >>= 4;
    --j;
  }

/* 324E: rounding */
  a = bcd_add (a, 5) >> 4;

/* 31E4: */
  e2 = (uint16_t) (unsigned int) (0x1000 - j);
/* 31F6: normalisation */
  if ((a & ~0xFFFFFFFFFFFF) != 0)
  {
    a >>= 4;
    ++e2;
  }
/* 3204: */
  if (a != 0)
  {
    a |= ((uint64_t) e2) << 48;
  }

/* 320A: LN(x*10^n) = LN(x) + n*LN(10) */
  return fp_add (a, fp_mul (int2fp ((int16_t) e1), LN10));
}


/* 3296: power function X ^ Y */
uint64_t fp_pwr (uint64_t x, uint64_t y)
{
  uint16_t e;

/* 32AA: */
  if (x == 0)
  {
    return 0;
  }

/* 32B0: negative values for X are allowed when Y is a positive integer */
  e = 0;
  if ((x & 0x8000000000000000) != 0)
  {
    if (fp_frac (y) != 0)
    {
      return NOT_A_NUMBER;
    }
/* 32CC: is Y even or odd? */
    e = (uint16_t) (y >> 48);
    e &= 0x0FFF;		/* remove the sign and bias */
    if (e != 0 && e <= 12)
    {
      e = ((y & (0x0001000000000000 >> 4*e)) == 0) ? 0 : 0x8000;
    }
  }

/* 32F6: X ^ Y = EXP(Y * LN(X)) */
  x = fp_mul (y, fp_ln (fp_abs (x)));
  if (x == NOT_A_NUMBER)
  {
    return NOT_A_NUMBER;
  }
  x = fp_exp (x);
  if (x == NOT_A_NUMBER)
  {
    return NOT_A_NUMBER;
  }
  return ((uint64_t) e << 48) | x;
}


/* 34EE: table of constants 10^j * atn (10^-j) */
const uint64_t atn_tab[] = {
  0x0007853981634000,	/* atn 1 = 0.7853981634000 */
  0x0009966865249117,	/* atn 0.1 = 0.09966865249117 */
  0x0009999666686666,	/* atn 0.01 = 0.009999666686666 */
  0x0009999996666669,	/* atn 0.001 = 0.0009999996666669 */
  0x0009999999966667,	/* atn 0.0001 = 0.00009999999966667 */
  0x0009999999999667,	/* atn 0.00001 = 0.000009999999999667 */
  0x0009999999999997	/* atn 0.000001 = 0.0000009999999999997 */
};


/* angle conversion tables */
const uint64_t tab1[] = { FORTYFIVE, QUARTERPI, FIFTY };
const uint64_t tab2[] = { NINETY, HALFPI, HUNDRED };
const uint64_t tab3[] = { HUNDREDEIGHTY, PI, TWOHUNDRED };
const uint64_t tab4[] = { FROMDEG, ONE, FROMGRA };
const uint64_t tab5[] = { TODEG, ONE, TOGRA };


/* 332A: function ACS,
 ACS (x) = PI/2 - ASN (x) */
uint64_t fp_acs (uint64_t x, unsigned int md /* 0=DEG, 1=RAD, 2=GRA */)
{
  if (md > 2)
  {
    return NOT_A_NUMBER;
  }
  x = fp_asn (x, 1);
  if (x == NOT_A_NUMBER)
  {
    return NOT_A_NUMBER;
  }
  x = fp_sub (HALFPI, x);
/* 34B8: optional conversion of the angle in RAD to DEG or GRA */
  if (md != 1)
  {
    x = fp_mul (x, tab5[md]);
  }
  return x;
}


/* 33A0: function ASN
 ASN (x) = ATN (x / SQR(1-x^2))
 or
 ASN (x) = PI/2 - ATN (SQR(1-x^2) / x) because ATN(x) = PI/2 - ATN(1/x) */
uint64_t fp_asn (uint64_t x, unsigned int md /* 0=DEG, 1=RAD, 2=GRA */)
{
  uint64_t s, y;

  if (md > 2)
  {
    return NOT_A_NUMBER;
  }
  if (x == 0)
  {
    return 0;
  }
  s = x & 0x8000000000000000;	/* sign */
  x = fp_abs (x);
  if (x > ONE)
  {
    return NOT_A_NUMBER;
  }
  y = fp_sqr (fp_sub (ONE, fp_mul (x, x)));
  if (y == NOT_A_NUMBER)	/* shouldn't happen, but just in case... */
  {
    return NOT_A_NUMBER;
  }
  x = (x > SQRTHALF) ?
	fp_sub (HALFPI, sub_atn (fp_div (y, x))) : fp_atn (fp_div (x, y), 1);
  x |= s;
/* 34B8: optional conversion of the angle in RAD to DEG or GRA */
  if (md != 1)
  {
    x = fp_mul (x, tab5[md]);
  }
  return x;
}


/* 33E4: function ATN */
uint64_t fp_atn (uint64_t x, unsigned int md /* 0=DEG, 1=RAD, 2=GRA */)
{
  uint64_t s;

  if (md > 2)
  {
    return NOT_A_NUMBER;
  }
  if (x == 0)
  {
    return 0;
  }
  s = x & 0x8000000000000000;	/* sign */
  x = fp_abs (x);
/* if argument > 1 then this formula is used: ATN(x) = PI/2 - ATN(1/x) */
  x = (x > ONE) ? fp_sub (HALFPI, sub_atn (fp_div (ONE, x))) : sub_atn (x);
  x |= s;
/* 34B8: optional conversion of the angle in RAD to DEG or GRA */
  if (md != 1)
  {
    x = fp_mul (x, tab5[md]);
  }
  return x;
}


/* arcus tangent, argument in range 0 to +1, output in radians */
uint64_t sub_atn (uint64_t a)
{
  uint64_t b, q, m;
  uint16_t e;
  int j;

  if (a == 0)
  {
    return 0;
  }
  e = (uint16_t) (a >> 48) - 0x1001;
  a &= 0xFFFFFFFFFFFF;

/* 344E: */
  b = 0x0001000000000000;
  q = 0;
  a <<= 4;

/* 34E0: First iteration - pseudo division */
  j = 0;
  while (1)
  {
    while (a >= b)
    {
/* shifting a number of "uint64_t" type by more than 64 bits may be illegal */
      m = (j - (int) (int16_t) e < 8) ? a >> (8 * (j - (int) (int16_t) e)) : 0;
      a = bcd_sub (a, b);
      b = bcd_add (b, m);
      ++q;
    }
    if (j >= 12 && ((q == 0) || (q & ~0xFFFFFFFFFFFF) != 0))
    {
      break;
    }
    a <<= 4;
    q <<= 4;
    ++j;
  }

/* 323A: Second iteration:
 calculate ATN(x) as a sum of terms q[j]*ATN(10^-j) using pseudo multiplication
*/
  a = 0;
  while (1)
  {
    b = (j - (int) (int16_t) e < 7) ? atn_tab[j - (int) (int16_t) e] :
	0x0010000000000010;
    while ((q & 0xF) != 0)
    {
      a = bcd_add (a, b);
      --q;
    }
    if (q == 0)
    {
      break;
    }
    a >>= 4;
    q >>= 4;
    --j;
  }

/* 34E6: rounding */
  a = bcd_add (a, 0x5) >> 4;

/* 345E: */
  e += (uint16_t) (int16_t) (0x1000 - j);
/* 3474: normalisation */
  if ((a & ~0xFFFFFFFFFFFF) != 0)
  {
    a >>= 4;
    ++e;
  }

  return a | ((uint64_t) e) << 48;
}


/* 353A: function SIN,
 done differently than in the original code, but gives the same results */
uint64_t fp_sin (uint64_t x, unsigned int md /* 0=DEG, 1=RAD, 2=GRA */)
{
  uint64_t y, z;

  if (md > 2)
  {
    return NOT_A_NUMBER;
  }
  y = fp_mod (x, tab3[md] /* PI */);
  z = y & 0x9FFFFFFFFFFFFFFF;
  if (z == 0)
  {
    return 0;
  }
/* conversion of the trimmed angle to radians */
  if (md != 1 /* RAD */)
  {
    z = fp_mul (z, tab4[md]);
  }
/* if the value of z is near PI/2 then return 1 */
  if (((z ^ HALFPI) & 0x1FFFFFFFFFFF0000) == 0)
  {
    x = ONE;
  }
/* otherwise this formula is used: SIN(x) = TAN(x) / SQR(1+TAN(x)^2) */
  else
  {
    x = fp_tan (x, md);
    x = fp_abs (fp_div (x, fp_sqr (fp_add (ONE, fp_mul (x, x)))));
  }
/* sign */
  if ((y & 0x2000000000000000) != 0)
  {
    y ^= 0x8000000000000000;
  }
  return (y & 0x8000000000000000) | x;
}


/* 35A0: function COS,
 done differently than in the original code, but gives the same results */
uint64_t fp_cos (uint64_t x, unsigned int md /* 0=DEG, 1=RAD, 2=GRA */)
{
  uint64_t y;

  if (md > 2)
  {
    return NOT_A_NUMBER;
  }
  y = fp_mod (fp_abs (x), tab3[md] /* PI */) & 0x9FFFFFFFFFFFFFFF;
/* conversion of the trimmed angle to radians */
  if (md != 1 /* RAD */)
  {
    y = fp_mul (y, tab4[md]);
  }
/* if the value of y is near PI/2 then this formula is used:
 COS(x) = SIN(PI/2-x), which can be approximated by: PI/2-x */
  if (((y ^ HALFPI) & 0x1FFFFFFFFFFF0000) == 0)
  {
    return fp_sub (HALFPI, y);
  }
/* otherwise this formula is used: COS(x) = 1 / SQR(1+TAN(x)^2) */
  y = fp_tan (x, md);
  x ^= y;
  x &= 0x8000000000000000;	/* sign */
  return x | fp_div (ONE, fp_sqr (fp_add (ONE, fp_mul (y, y))));
}


/* 362C: function TAN */
uint64_t fp_tan (uint64_t a, unsigned int md /* 0=DEG, 1=RAD, 2=GRA */)
{
  uint64_t b, m, q;
  uint16_t e, s;
  int i, j;
  unsigned int octant;

  if (md > 2)
  {
    return NOT_A_NUMBER;
  }
  s = (uint16_t) (a >> 48);
  s &= 0x8000;			/* sign */

/* 362E: trim the angle to the range 0..PI/2 */
  a = fp_mod (fp_abs (a), tab2[md]);	/* also quadrant on the bits 62..61 */
  octant = (unsigned int) (a >> 60) & 2;
  a = fp_abs (a);		/* fp_abs clears the bits 62..61 */

/* 3666: if angle > PI/4 then angle = PI/2-angle
   TAN(PI/2 - x) = 1/TAN(x) */
  if (a > tab1[md])
  {
/* 36CA: */
    ++octant;
    a = fp_sub(tab2[md], a);
  }

/* 3680: conversion of the trimmed angle to radians */
  if (md != 1 /* RAD */)
  {
    a = fp_mul (a, tab4[md]);
  }

/* 36A2: */
  if (a == 0)
  {
    return (octant == 0) ? 0 : NOT_A_NUMBER;
  }

  e = (uint16_t) (a >> 48);
  e -= 0x1000;			/* remove the bias */
  a &= 0xFFFFFFFFFFFF;		/* mantissa */

  q = 0;
  a <<= 4;
  
/* 3164: First iteration:
 the trimmed angle will be expressed as a sum of terms q[j]*atn(10^-j) using
 pseudo division */
  j = 0;
  while (1)
  {
    b = (j - (int) (int16_t) e < 7) ? atn_tab[j - (int) (int16_t) e] :
	0x0010000000000010;
    while (a >= b)
    {
      a = bcd_sub (a, b);
      ++q;
    }
    if (j >= 12 && ((q == 0) || (q & ~0xFFFFFFFFFFFF) != 0))
    {
      break;
    }
    a <<= 4;
    q <<= 4;
    ++j;
  }

/* 3172: Second iteration:
 calculate the coordinates A and B using pseudo multiplication */
  a = 0;
  b = 0x0001000000000000;
  while (1)
  {
    while ((q & 0xF) != 0)
    {
/* shifting a number of "uint64_t" type by more than 64 bits may be illegal */
      m = (j - (int) (int16_t) e < 8) ? a >> (8 * (j - (int) (int16_t) e)) : 0;
      a = bcd_add (a, b);
      b = bcd_sub (b, m);
      --q;
    }
    if (q == 0)
    {
      break;
    }
    a >>= 4;
    q >>= 4;
    --j;
  }

/* 375C: */
  if (octant == 1 || octant == 2)
  {
/* 3768: swap the coordinates A and B to calculate a reciprocal of the tangent */
    q = a;
    a = b;
    b = q;
/* 377C: */
    e = -e;
    j = -j;
  }

/* 3164: Third iteration: calculate the tangent with the division A/B */
  q = 0;
  i = 0;
  while (1)
  {
    while (a >= b)
    {
      a = bcd_sub (a, b);
      ++q;
    }
    if (i >= 12 && ((q == 0) || (q & ~0xFFFFFFFFFFFF) != 0))
    {
      break;
    }
    a <<= 4;
    q <<= 4;
    ++i;
  }

/* 37A4: */
  q >>= 4;

/* 36DA: */
  e += (uint16_t) (0x100D - i - j);

  if (octant == 2 || octant == 3)
  {
    s ^= 0x8000;
  }
  
  return (((uint64_t) (s | e)) << 48) | q;
}


/* 3840: integer BCD addition */
uint64_t bcd_add (uint64_t x1, uint64_t x2)
{
  uint64_t r1, r2, sum;
/* perform a binary addition */
  sum = x1 + x2;
/* find digits where carry to the next digit occurred */
  r2 = (x1 | x2) & ~sum;
/* find digits greater than 9 */
  r1 = ~sum;
  sum += 0x6666666666666666;	/* also pre-adjust all digits (add 6 to them) */
  r1 |= sum;
/* mark all digits not meeting any of the above conditions */
  r1 &= ~r2;
  r1 &= 0x8888888888888888;
/* remove the decimal adjustment (subtract the extra 6) from the digits which
 don't need it */
  r1 /= 4;
  r1 *= 3;
  sum -= r1;
  return sum;
}


/* 3874: integer BCD subtraction, minuend >= subtrahend */
uint64_t bcd_sub (uint64_t minuend, uint64_t subtrahend)
{
  uint64_t remainder, r1;
/* perform a binary subtraction */
  remainder = minuend - subtrahend;
/* find digits from which borrow occurred from the next digit */
  r1 = minuend ^ subtrahend ^ remainder;
/* subtract 6 from all digits meeting the above condition */
  r1 /= 8;
  r1 &= 0x2222222222222222;
  r1 *= 3;
  remainder -= r1;
  return remainder;
}
