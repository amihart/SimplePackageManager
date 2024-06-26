/** Null character in string tests.

     storage: data, xdata, code,
*/
#include <testfwk.h>

#if defined(PORT_HOST) || defined(SDCC_z80) || defined(SDCC_gbz80)
# define data
# define xdata
# define code
#endif

{storage} char string1[] = "";
{storage} char string2[] = "a\0b\0c";
{storage} char string3[5] = "a\0b\0c";

void
testStringArray(void)
{
  /* Make sure the strings are the correct size */
  /* and have the terminating null character */
  ASSERT(sizeof(string1)==1);
  ASSERT(sizeof(string2)==6);
  ASSERT(sizeof(string3)==5);
  ASSERT(string1[0]==0);
  ASSERT(string2[5]==0);
  
  ASSERT(string2[0]=='a');
  ASSERT(string2[2]=='b');
  ASSERT(string2[4]=='c');  
  
}

void
testStringConst(void)
{
  char * constStr1 = "";
  char * constStr2 = "a\0b\0c";

  ASSERT (constStr1[0]==0);
  ASSERT (constStr2[5]==0);
}
