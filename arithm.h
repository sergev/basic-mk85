#ifndef _ARITHM_H_
#define _ARITHM_H_

#define NOT_A_NUMBER -1

/* floating point constants */
#define LN10		0x1001230258509300u	/* 2.30258509300 = ln 10 */
#define ONE		0x1001100000000000u	/* 1.00000000000 */
#define PI		0x1001314159265360u	/* 3.14159265360 = PI */
#define HALFPI		0x1001157079632680u	/* 1.57079632680 = PI/2 */
#define QUARTERPI	0x1000785398163400u	/* 0.785398163400 = PI/4 */
#define SQRTHALF	0x1000707106781186u	/* 0.707106781186 = SQR(1/2) */
#define FORTYFIVE	0x1002450000000000u	/* 45.0000000000 */
#define FIFTY		0x1002500000000000u	/* 50.0000000000 */
#define NINETY		0x1002900000000000u	/* 90.0000000000 */
#define HUNDRED		0x1003100000000000u	/* 100.000000000 */
#define HUNDREDEIGHTY	0x1003180000000000u	/* 180.000000000 */
#define TWOHUNDRED	0x1003200000000000u	/* 200.000000000 */
#define	TODEG		0x1002572957795131u	/* 57.2957795131 = 180/PI */
#define TOGRA		0x1002636619772368u	/* 63.6619772368 = 200/PI */
#define FROMDEG		0x0FFF174532925200u	/* 0.0174532925200 = PI/180 */
#define FROMGRA		0x0FFF157079632680u	/* 0.0157079632680 = PI/200 */

/* public function prototypes */
void fp2str (uint64_t fpnumber, int spec_precision, char *destination);
char* int2str (uint16_t number, char *destination);
uint64_t str2fp (void);
int get_digit (int x);
uint64_t fp_sub (uint64_t x1, uint64_t x2);
uint64_t fp_add (uint64_t x1, uint64_t x2);
uint64_t fp_mul (uint64_t x1, uint64_t x2);
uint64_t fp_div (uint64_t dividend, uint64_t divisor);
uint64_t fp_mod (uint64_t dividend, uint64_t divisor);
uint64_t fp_abs (uint64_t x);
uint64_t fp_neg (uint64_t x);
uint64_t fp_sgn (uint64_t x);
uint64_t fp_rnd (uint64_t x, uint64_t y);
uint64_t fp_frac (uint64_t x);
uint64_t fp_int (uint64_t x);
uint64_t fp_sqr (uint64_t a);
uint64_t fp_exp (uint64_t x);
uint64_t fp_log (uint64_t x);
uint64_t fp_ln (uint64_t a);
uint64_t fp_pwr (uint64_t x, uint64_t y);
uint64_t fp_acs (uint64_t x, unsigned int md);
uint64_t fp_asn (uint64_t x, unsigned int md);
uint64_t fp_atn (uint64_t x, unsigned int md);
uint64_t sub_atn (uint64_t a);
uint64_t fp_sin (uint64_t x, unsigned int md);
uint64_t fp_cos (uint64_t x, unsigned int md);
uint64_t fp_tan (uint64_t a, unsigned int md);
uint64_t int2fp (int16_t x);
int16_t fp2int (uint64_t x);

#endif
