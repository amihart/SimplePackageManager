#define __16F873
#include "p16f873.h"
// Signed comparisons of the form:  (variable>=LIT)
//
// This regression test exercises all of the boundary
// conditions in literal less than comparisons. There
// are numerous opportunities to optimize these comparison
// and each one has an astonishing capability of failing
// a boundary condition.

unsigned char success = 0;
unsigned char failures = 0;
unsigned char dummy = 0;
unsigned char result = 0;

//bit bit0 = 0;
int int0 = 0;
int int1 = 0;
unsigned char uchar0 = 0;
unsigned char uchar1 = 0;
signed char char0 = 0;
signed char char1 = 0;
char long0 = 0;
char long1 = 0;

/* copied from 16f877.inc file supplied with gpasm */

#define _CP_ALL          0x0FCF
#define _CP_HALF         0x1FDF
#define _CP_UPPER_256    0x2FEF
#define _CP_OFF          0x3FFF
#define _DEBUG_ON        0x37FF
#define _DEBUG_OFF       0x3FFF
#define _WRT_ENABLE_ON   0x3FFF
#define _WRT_ENABLE_OFF  0x3DFF
#define _CPD_ON          0x3EFF
#define _CPD_OFF         0x3FFF
#define _LVP_ON          0x3FFF
#define _LVP_OFF         0x3F7F
#define _BODEN_ON        0x3FFF
#define _BODEN_OFF       0x3FBF
#define _PWRTE_OFF       0x3FFF
#define _PWRTE_ON        0x3FF7
#define _WDT_ON          0x3FFF
#define _WDT_OFF         0x3FFB
#define _LP_OSC          0x3FFC
#define _XT_OSC          0x3FFD
#define _HS_OSC          0x3FFE
#define _RC_OSC          0x3FFF

/* *** NOTE ***  This particular test takes quite a while to run 
 * ~ 10,000,000 instruction cycles. (2.5 seconds on a 20Mhz PIC).
 * The WDT will reset the CPU if it's enabled. So disable it...
*/

typedef unsigned int word;

word at 0x2007  CONFIG = _WDT_OFF & _PWRTE_ON;

void
done ()
{
  dummy++;
}


void c_char_gte_lit1(unsigned char expected_result)
{
  result = 0;

  if(char0 >= -0x7f)
    result |= 1;


  if(char0 >= -1)
    result |= 2;

  if(char0 >= 0)
    result |= 4;

  if(char0 >= 1)
    result |= 8;

  if(char0 >= 0x7e)
    result |= 0x10;
  
  if(char0 >= 0x7f)
    result |= 0x20;
  
  if(result != expected_result)
    failures++;
}


void char_compare(void)
{
  char0 = 0x7f;
  c_char_gte_lit1(0x3f);

  char0 = 0x7e;
  c_char_gte_lit1(0x1f);

  char0 = 0x40;
  c_char_gte_lit1(0x0f);

  char0 = 0x2;
  c_char_gte_lit1(0x0f);

  char0 = 0x1;
  c_char_gte_lit1(0x0f);

  char0 = 0;
  c_char_gte_lit1(0x07);

  char0 = -1;
  c_char_gte_lit1(0x03);

  char0 = -2;
  c_char_gte_lit1(0x01);

  char0 = -0x40;
  c_char_gte_lit1(0x01);

  char0 = -0x7e;
  c_char_gte_lit1(0x01);

  char0 = -0x7f;
  c_char_gte_lit1(0x01);

  char0 = 0x80;
  c_char_gte_lit1(0x00);


  /* Now test entire range */

  for(char0=1; char0 != 0x7e; char0++)
    c_char_gte_lit1(0x0f);

  for(char0=-0x7f; char0 != -1; char0++)
    c_char_gte_lit1(0x01);

}


void c_int_gte_lit1(unsigned char expected_result)
{
  result = 0;

  if(int0 >= 0)
    result |= 1;

  if(int0 >= 1)
    result |= 2;


  if(int0 >= 0xff)
    result |= 4;

  if(int0 >= 0x100)
    result |= 8;

  if(int0 >= 0x0101)
    result |= 0x10;
  
  if(int0 >= 0x01ff)
    result |= 0x20;
  
  if(int0 >= 0x0200)
    result |= 0x40;

  if(int0 >= 0x0201)
    result |= 0x80;

  if(result != expected_result)
    failures=1;

}


void int_compare1(void)
{
  int0 = -1;
  c_int_gte_lit1(0x00);

  int0 = 0;
  c_int_gte_lit1(0x01);

  int0 = 1;
  c_int_gte_lit1(0x03);

  int0 = 2;
  c_int_gte_lit1(0x03);

  int0 = 0xfe;
  c_int_gte_lit1(0x03);

  int0 = 0xff;
  c_int_gte_lit1(0x07);

  int0 = 0x100;
  c_int_gte_lit1(0x0f);

  int0 = 0x101;
  c_int_gte_lit1(0x1f);

  int0 = 0x102;
  c_int_gte_lit1(0x1f);

  int0 = 0x1fe;
  c_int_gte_lit1(0x1f);

  int0 = 0x1ff;
  c_int_gte_lit1(0x3f);

  int0 = 0x200;
  c_int_gte_lit1(0x7f);

  int0 = 0x201;
  c_int_gte_lit1(0xff);

  int0 = 0x7f00;
  c_int_gte_lit1(0xff);

  /* now check contiguous ranges */

  for(int0 = -0x7fff; int0 != -1; int0++)
    c_int_gte_lit1(0x00);

  for(int0 = 1; int0 != 0xff; int0++)
    c_int_gte_lit1(0x03);

  for(int0 = 0x201; int0 != 0x7fff; int0++)
    c_int_gte_lit1(0xff);

}


void c_int_gte_lit2(unsigned char expected_result)
{
  result = 0;

  if(int0 >= -0x7fff)
    result |= 1;

  if(int0 >= -0x7f00)
    result |= 2;

  if(int0 >= -0x7eff)
    result |= 4;

  if(int0 >= -0x7e00)
    result |= 8;

  if(int0 >= -0x0101)
    result |= 0x10;
  
  if(int0 >= -0x0100)
    result |= 0x20;
  
  if(int0 >= -0xff)
    result |= 0x40;

  if(int0 >= -1)
    result |= 0x80;

  if(result != expected_result)
    failures=1;
}

void int_compare2(void)
{
  int0 = -0x7fff;
  c_int_gte_lit2(0x01);

  int0 = -0x7f00;
  c_int_gte_lit2(0x03);

  int0 = -0x7eff;
  c_int_gte_lit2(0x07);

  int0 = -0x7e00;
  c_int_gte_lit2(0x0f);

  int0 = -0x7dff;
  c_int_gte_lit2(0x0f);

  int0 = -0x4567;
  c_int_gte_lit2(0x0f);

  int0 = -0x200;
  c_int_gte_lit2(0x0f);

  int0 = -0x102;
  c_int_gte_lit2(0x0f);

  int0 = -0x101;
  c_int_gte_lit2(0x1f);

  int0 = -0x100;
  c_int_gte_lit2(0x3f);

  int0 = -0xff;
  c_int_gte_lit2(0x7f);

  int0 = -0x02;
  c_int_gte_lit2(0x7f);

  int0 = -0x01;
  c_int_gte_lit2(0xff);

  int0 = 0;
  c_int_gte_lit2(0xff);

  int0 = 1;
  c_int_gte_lit2(0xff);

  int0 = 0x7fff;
  c_int_gte_lit2(0xff);

  /* now check contiguous ranges */

  for(int0 = -0x7fff; int0 != -0x7f00; int0++)
    c_int_gte_lit2(0x01);

  for(int0 = -0x7e00; int0 != -0x101; int0++)
    c_int_gte_lit2(0x0f);

  for(int0 = -1; int0 != 0x7fff; int0++)
    c_int_gte_lit2(0xff);

}




void
main (void)
{
  char_compare();
  int_compare1();
  int_compare2();

  success = failures;
  done ();
}
