#define __16F873
#include "p16f873.h"


/* Compound comparisons */


unsigned char success=0;
unsigned char failures=0;
unsigned char dummy=0;

bit bit0 = 0;
bit bit1 = 0;
unsigned int ui0 = 0;
unsigned int ui1 = 0;
unsigned char uc0 = 0;
unsigned char uc1 = 0;
unsigned long uL0 = 0;
unsigned long uL1 = 0;

void done()
{

  dummy++;

}



void compound_compare_uc(void)
{

  failures += (uc0 != uc1);

}


void compound_compare_ui(void)
{

  failures += (ui0 != ui1);

}

void compound_compare_ul(void)
{

  failures += (uL0 != uL1);

}

void compound_compare_uc_lit(void)
{

  failures += (uc0 != 0xff);
  failures += (uc0 != 0xff);
  failures += (uc0 == 0);

}

void main(void)
{

  compound_compare_uc();
  compound_compare_ui();
  compound_compare_ul();

  uc0 = 0xff;
  compound_compare_uc_lit();

  success = failures;
  done();
}
