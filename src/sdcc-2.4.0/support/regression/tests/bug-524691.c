/* Division by powers of two.
 */
#include <testfwk.h>

#if defined __mcs51 || defined __ds390 || defined __xa51
#define XDATA xdata
#else
#define XDATA
#endif

typedef unsigned int UINT;

typedef struct _HeapEntryState
{
  void *pBase;
  UINT uFlags;
} HeapEntryState;

static HeapEntryState *_getHeapEntryState(void *p, HeapEntryState *pStates, UINT nStateEntries)
{
  int uLeft = -1, uRight = nStateEntries, uMiddle;

  while (uRight - uLeft > 1)
    {
      int iDiff;

      uMiddle = (uLeft + uRight)/2;
      /* A divide by zero is added just before iDiff is assigned */
      iDiff = pStates[uMiddle].pBase - p;

      if (iDiff > 0)
	{
	  uRight = uMiddle;
	}
      else if (iDiff < 0)
	{
	  uLeft = uMiddle;
	}
      else
	{
	  return pStates + uMiddle;
	}
    }

  return NULL;
}

void
testDivByZero(void)
{
  HeapEntryState aStates[] = {
    { (void XDATA *)1, 0 }
  };
  void *p = (void XDATA *)0x1234;

  ASSERT(_getHeapEntryState(p, aStates, 1) == NULL);

  aStates[0].pBase = p;
  ASSERT(_getHeapEntryState(p, aStates, 1) == aStates + 0);
}
