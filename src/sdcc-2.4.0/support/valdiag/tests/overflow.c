volatile char c;
volatile unsigned char uc;
volatile int i;
volatile unsigned u;
volatile long l;
volatile unsigned long ul;

#ifdef TEST0
void foo(void)
{
  i = 10000 * 10000;	/* WARNING(SDCC) */
  i = 0x4000 * 0x4000;	/* WARNING(SDCC) */
}
#endif

#ifdef TEST1
void foo(void)
{
  c = 1  <<  7;

  i = c  << 10;
  i = 1  << 10;
  i = c  << 16;		/* WARNING(SDCC) */
  i = 1  << 16;		/* WARNING(SDCC) */
  i = uc << 16;		/* WARNING(SDCC) */

  l = l  << 31;
  l = l  << 32;		/* WARNING */
  l = ul << 32;		/* WARNING */
}
#endif


#ifdef TEST2
void foo(void)
{
  i = 1  >> 40;		/* WARNING(GCC)  */

  i = uc >>  7;
  i = 1  >>  7;
  i = uc >>  8;		/* WARNING(SDCC) */
#if 0
  i = 1  >>  8;		/* WARN___(SDCC) */
#endif

  i = i  >> 40;		/* WARNING(GCC)  */

  i = u  >> 15;
  i = u  >> 16;		/* WARNING(SDCC) */

  i = l  >> 40;		/* WARNING(GCC)  */

  i = ul >> 31;
  i = ul >> 32;		/* WARNING       */
}
#endif

