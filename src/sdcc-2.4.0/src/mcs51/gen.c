/*-------------------------------------------------------------------------
  SDCCgen51.c - source file for code generation for 8051

  Written By -  Sandeep Dutta . sandeep.dutta@usa.net (1998)
         and -  Jean-Louis VERN.jlvern@writeme.com (1999)
  Bug Fixes  -  Wojciech Stryjewski  wstryj1@tiger.lsu.edu (1999 v2.1.9a)

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2, or (at your option) any
  later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

  In other words, you are welcome to use, share and improve this program.
  You are forbidden to forbid anyone else to use, share and improve
  what you give them.   Help stamp out software-hoarding!

  Notes:
  000123 mlh  Moved aopLiteral to SDCCglue.c to help the split
      Made everything static
-------------------------------------------------------------------------*/

//#define D(x)
#define D(x) x

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "SDCCglobl.h"
#include "newalloc.h"

#include "common.h"
#include "SDCCpeeph.h"
#include "ralloc.h"
#include "gen.h"

char *aopLiteral (value * val, int offset);
extern int allocInfo;

/* this is the down and dirty file with all kinds of
   kludgy & hacky stuff. This is what it is all about
   CODE GENERATION for a specific MCU . some of the
   routines may be reusable, will have to see */

static char *zero = "#0x00";
static char *one = "#0x01";
static char *spname;

char *fReturn8051[] =
{"dpl", "dph", "b", "a"};
unsigned fReturnSizeMCS51 = 4;	/* shared with ralloc.c */
char **fReturn = fReturn8051;
static char *accUse[] =
{"a", "b"};

static unsigned short rbank = -1;

static struct
  {
    short r0Pushed;
    short r1Pushed;
    short accInUse;
    short inLine;
    short debugLine;
    short nRegsSaved;
    set *sendSet;
    iCode *current_iCode;
    symbol *currentFunc;
  }
_G;

static char *rb1regs[] = {
    "b1_0","b1_1","b1_2","b1_3","b1_4","b1_5","b1_6","b1_7"
};

extern int mcs51_ptrRegReq;
extern int mcs51_nRegs;
extern FILE *codeOutFile;
static void saveRBank (int, iCode *, bool);
#define RESULTONSTACK(x) \
                         (IC_RESULT(x) && IC_RESULT(x)->aop && \
                         IC_RESULT(x)->aop->type == AOP_STK )

#define MOVA(x) mova(x)  /* use function to avoid multiple eval */
#define CLRC    emitcode("clr","c")
#define SETC    emitcode("setb","c")

static lineNode *lineHead = NULL;
static lineNode *lineCurr = NULL;

static unsigned char SLMask[] =
{0xFF, 0xFE, 0xFC, 0xF8, 0xF0,
 0xE0, 0xC0, 0x80, 0x00};
static unsigned char SRMask[] =
{0xFF, 0x7F, 0x3F, 0x1F, 0x0F,
 0x07, 0x03, 0x01, 0x00};

#define LSB     0
#define MSB16   1
#define MSB24   2
#define MSB32   3

/*-----------------------------------------------------------------*/
/* emitcode - writes the code into a file : for now it is simple    */
/*-----------------------------------------------------------------*/
static void
emitcode (char *inst, const char *fmt,...)
{
  va_list ap;
  char lb[INITIAL_INLINEASM];
  char *lbp = lb;

  va_start (ap, fmt);

  if (inst && *inst)
    {
      if (fmt && *fmt)
	sprintf (lb, "%s\t", inst);
      else
	sprintf (lb, "%s", inst);
      vsprintf (lb + (strlen (lb)), fmt, ap);
    }
  else
    vsprintf (lb, fmt, ap);

  while (isspace (*lbp))
    lbp++;

  // printf ("%s\n", lb);
  
  if (lbp && *lbp)
    lineCurr = (lineCurr ?
		connectLine (lineCurr, newLineNode (lb)) :
		(lineHead = newLineNode (lb)));
  lineCurr->isInline = _G.inLine;
  lineCurr->isDebug = _G.debugLine;
  lineCurr->ic = _G.current_iCode;
  lineCurr->isComment = (*lbp==';');
  va_end (ap);
}

/*-----------------------------------------------------------------*/
/* mova - moves specified value into accumulator                   */
/*-----------------------------------------------------------------*/
static void
mova (const char *x)
{
  /* do some early peephole optimization */
  if (!strcmp(x, "a") || !strcmp(x, "acc"))
    return;

  emitcode("mov","a,%s", x);
}

/*-----------------------------------------------------------------*/
/* getFreePtr - returns r0 or r1 whichever is free or can be pushed */
/*-----------------------------------------------------------------*/
static regs *
getFreePtr (iCode * ic, asmop ** aopp, bool result)
{
  bool r0iu = FALSE, r1iu = FALSE;
  bool r0ou = FALSE, r1ou = FALSE;

  /* the logic: if r0 & r1 used in the instruction
     then we are in trouble otherwise */

  /* first check if r0 & r1 are used by this
     instruction, in which case we are in trouble */
  r0iu = bitVectBitValue (ic->rUsed, R0_IDX);
  r1iu = bitVectBitValue (ic->rUsed, R1_IDX);
  if (r0iu && r1iu) {
      goto endOfWorld;
    }

  r0ou = bitVectBitValue (ic->rMask, R0_IDX);
  r1ou = bitVectBitValue (ic->rMask, R1_IDX);

  /* if no usage of r0 then return it */
  if (!r0iu && !r0ou)
    {
      ic->rUsed = bitVectSetBit (ic->rUsed, R0_IDX);
      (*aopp)->type = AOP_R0;

      return (*aopp)->aopu.aop_ptr = mcs51_regWithIdx (R0_IDX);
    }

  /* if no usage of r1 then return it */
  if (!r1iu && !r1ou)
    {
      ic->rUsed = bitVectSetBit (ic->rUsed, R1_IDX);
      (*aopp)->type = AOP_R1;

      return (*aopp)->aopu.aop_ptr = mcs51_regWithIdx (R1_IDX);
    }

  /* now we know they both have usage */
  /* if r0 not used in this instruction */
  if (!r0iu)
    {
      /* push it if not already pushed */
      if (!_G.r0Pushed)
	{
	  emitcode ("push", "%s",
		    mcs51_regWithIdx (R0_IDX)->dname);
	  _G.r0Pushed++;
	}

      ic->rUsed = bitVectSetBit (ic->rUsed, R0_IDX);
      (*aopp)->type = AOP_R0;

      return (*aopp)->aopu.aop_ptr = mcs51_regWithIdx (R0_IDX);
    }

  /* if r1 not used then */

  if (!r1iu)
    {
      /* push it if not already pushed */
      if (!_G.r1Pushed)
	{
	  emitcode ("push", "%s",
		    mcs51_regWithIdx (R1_IDX)->dname);
	  _G.r1Pushed++;
	}

      ic->rUsed = bitVectSetBit (ic->rUsed, R1_IDX);
      (*aopp)->type = AOP_R1;
      return mcs51_regWithIdx (R1_IDX);
    }
endOfWorld:
  /* I said end of world, but not quite end of world yet */
  if (result) {
    /* we can push it on the stack */
    (*aopp)->type = AOP_STK;
    return NULL;
  } else {
    /* in the case that result AND left AND right needs a pointer reg
       we can safely use the result's */
    if (bitVectBitValue (mcs51_rUmaskForOp(IC_RESULT(ic)), R0_IDX)) {
      (*aopp)->type = AOP_R0;
      return mcs51_regWithIdx (R0_IDX);
    }
    if (bitVectBitValue (mcs51_rUmaskForOp(IC_RESULT(ic)), R1_IDX)) {
      (*aopp)->type = AOP_R1;
      return mcs51_regWithIdx (R1_IDX);
    }
  }

  /* now this is REALLY the end of the world */
  werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
	  "getFreePtr should never reach here");
  exit (1);
}


/*-----------------------------------------------------------------*/
/* getTempRegs - initialize an array of pointers to GPR registers */
/*               that are not in use. Returns 1 if the requested   */
/*               number of registers were available, 0 otherwise.  */
/*-----------------------------------------------------------------*/
int
getTempRegs(regs **tempRegs, int size, iCode *ic)
{
  bitVect * freeRegs;
  int i;
  int offset;

  if (!ic)
    ic = _G.current_iCode;
  if (!ic)
    return 0;
  if (!_G.currentFunc)
    return 0;

  freeRegs = newBitVect(8);
  bitVectSetBit (freeRegs, R2_IDX);
  bitVectSetBit (freeRegs, R3_IDX);
  bitVectSetBit (freeRegs, R4_IDX);
  bitVectSetBit (freeRegs, R5_IDX);
  bitVectSetBit (freeRegs, R6_IDX);
  bitVectSetBit (freeRegs, R7_IDX);

  if (IFFUNC_CALLEESAVES(_G.currentFunc->type))
    {
      bitVect * newfreeRegs;
      newfreeRegs = bitVectIntersect (freeRegs, _G.currentFunc->regsUsed);
      freeBitVect(freeRegs);
      freeRegs = newfreeRegs;
    }
  freeRegs = bitVectCplAnd (freeRegs, ic->rMask);

  offset = 0;
  for (i=0; i<freeRegs->size; i++)
    {
      if (bitVectBitValue(freeRegs,i))
        tempRegs[offset++] = mcs51_regWithIdx(i);
      if (offset>=size)
        {
	  freeBitVect(freeRegs);
	  return 1;
	}
    }

  freeBitVect(freeRegs);
  return 1;
}


/*-----------------------------------------------------------------*/
/* newAsmop - creates a new asmOp                                  */
/*-----------------------------------------------------------------*/
static asmop *
newAsmop (short type)
{
  asmop *aop;

  aop = Safe_calloc (1, sizeof (asmop));
  aop->type = type;
  return aop;
}

/*-----------------------------------------------------------------*/
/* pointerCode - returns the code for a pointer type               */
/*-----------------------------------------------------------------*/
static int
pointerCode (sym_link * etype)
{

  return PTR_TYPE (SPEC_OCLS (etype));

}


/*-----------------------------------------------------------------*/
/* leftRightUseAcc - returns size of accumulator use by operands   */
/*-----------------------------------------------------------------*/
static int
leftRightUseAcc(iCode *ic)
{
  operand *op;
  int size;
  int accuseSize = 0;
  int accuse = 0;

  if (!ic)
    {
      werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
	      "null iCode pointer");
      return 0;
    }

  if (ic->op == IFX)
    {
      op = IC_COND (ic);
      if (IS_SYMOP (op) && OP_SYMBOL (op) && OP_SYMBOL (op)->accuse)
        {
          accuse = 1;
          size = getSize (OP_SYMBOL (op)->type);
          if (size>accuseSize)
            accuseSize = size;
        }
    }
  else if (ic->op == JUMPTABLE)
    {
      op = IC_JTCOND (ic);
      if (IS_SYMOP (op) && OP_SYMBOL (op) && OP_SYMBOL (op)->accuse)
        {
          accuse = 1;
          size = getSize (OP_SYMBOL (op)->type);
          if (size>accuseSize)
            accuseSize = size;
        }
    }
  else
    {
      op = IC_LEFT (ic);
      if (IS_SYMOP (op) && OP_SYMBOL (op) && OP_SYMBOL (op)->accuse)
        {
          accuse = 1;
          size = getSize (OP_SYMBOL (op)->type);
          if (size>accuseSize)
            accuseSize = size;
        }
      op = IC_RIGHT (ic);
      if (IS_SYMOP (op) && OP_SYMBOL (op) && OP_SYMBOL (op)->accuse)
        {
          accuse = 1;
          size = getSize (OP_SYMBOL (op)->type);
          if (size>accuseSize)
            accuseSize = size;
        }
    }

  if (accuseSize)
    return accuseSize;
  else
    return accuse;
}


/*-----------------------------------------------------------------*/
/* aopForSym - for a true symbol                                   */
/*-----------------------------------------------------------------*/
static asmop *
aopForSym (iCode * ic, symbol * sym, bool result)
{
  asmop *aop;
  memmap *space;

  wassertl (ic != NULL, "Got a null iCode");
  wassertl (sym != NULL, "Got a null symbol");

  space = SPEC_OCLS (sym->etype);

  /* if already has one */
  if (sym->aop)
    return sym->aop;

  /* assign depending on the storage class */
  /* if it is on the stack or indirectly addressable */
  /* space we need to assign either r0 or r1 to it   */
  if (sym->onStack || sym->iaccess)
    {
      sym->aop = aop = newAsmop (0);
      aop->aopu.aop_ptr = getFreePtr (ic, &aop, result);
      aop->size = getSize (sym->type);

      /* now assign the address of the variable to
         the pointer register */
      if (aop->type != AOP_STK)
	{

	  if (sym->onStack)
	    {
	      if (_G.accInUse || leftRightUseAcc (ic))
		emitcode ("push", "acc");

	      emitcode ("mov", "a,_bp");
	      emitcode ("add", "a,#0x%02x",
			((sym->stack < 0) ?
			 ((char) (sym->stack - _G.nRegsSaved)) :
			 ((char) sym->stack)) & 0xff);
	      emitcode ("mov", "%s,a",
			aop->aopu.aop_ptr->name);

	      if (_G.accInUse || leftRightUseAcc (ic))
		emitcode ("pop", "acc");
	    }
	  else
	    emitcode ("mov", "%s,#%s",
		      aop->aopu.aop_ptr->name,
		      sym->rname);
	  aop->paged = space->paged;
	}
      else
	aop->aopu.aop_stk = sym->stack;
      return aop;
    }

  /* if in bit space */
  if (IN_BITSPACE (space))
    {
      sym->aop = aop = newAsmop (AOP_CRY);
      aop->aopu.aop_dir = sym->rname;
      aop->size = getSize (sym->type);
      return aop;
    }
  /* if it is in direct space */
  if (IN_DIRSPACE (space))
    {
      //printf("aopForSym, using AOP_DIR for %s (%x)\n", sym->name, sym);
      //printTypeChainRaw(sym->type, NULL);
      //printf("space = %s\n", space ? space->sname : "NULL");
      sym->aop = aop = newAsmop (AOP_DIR);
      aop->aopu.aop_dir = sym->rname;
      aop->size = getSize (sym->type);
      return aop;
    }

  /* special case for a function */
  if (IS_FUNC (sym->type))
    {
      sym->aop = aop = newAsmop (AOP_IMMD);
      aop->aopu.aop_immd.aop_immd1 = Safe_calloc (1, strlen (sym->rname) + 1);
      strcpy (aop->aopu.aop_immd.aop_immd1, sym->rname);
      aop->size = FPTRSIZE;
      return aop;
    }

  /* only remaining is far space */
  /* in which case DPTR gets the address */
  sym->aop = aop = newAsmop (AOP_DPTR);
  emitcode ("mov", "dptr,#%s", sym->rname);
  aop->size = getSize (sym->type);

  /* if it is in code space */
  if (IN_CODESPACE (space))
    aop->code = 1;

  return aop;
}

/*-----------------------------------------------------------------*/
/* aopForRemat - rematerialzes an object                           */
/*-----------------------------------------------------------------*/
static asmop *
aopForRemat (symbol * sym)
{
  iCode *ic = sym->rematiCode;
  asmop *aop = newAsmop (AOP_IMMD);
  int ptr_type=0;
  int val = 0;

  for (;;)
    {
      if (ic->op == '+')
	val += (int) operandLitValue (IC_RIGHT (ic));
      else if (ic->op == '-')
	val -= (int) operandLitValue (IC_RIGHT (ic));
      else if (IS_CAST_ICODE(ic)) {
	      sym_link *from_type = operandType(IC_RIGHT(ic));
	      aop->aopu.aop_immd.from_cast_remat = 1;
	      ic = OP_SYMBOL (IC_RIGHT (ic))->rematiCode;
	      ptr_type = DCL_TYPE(from_type);
	      if (ptr_type == IPOINTER) {
		// bug #481053
		ptr_type = POINTER;
	      }
	      continue ;
      } else break;

      ic = OP_SYMBOL (IC_LEFT (ic))->rematiCode;
    }

  if (val)
    sprintf (buffer, "(%s %c 0x%04x)",
	     OP_SYMBOL (IC_LEFT (ic))->rname,
	     val >= 0 ? '+' : '-',
	     abs (val) & 0xffff);
  else
    strcpy (buffer, OP_SYMBOL (IC_LEFT (ic))->rname);

  aop->aopu.aop_immd.aop_immd1 = Safe_calloc (1, strlen (buffer) + 1);
  strcpy (aop->aopu.aop_immd.aop_immd1, buffer);
  /* set immd2 field if required */
  if (aop->aopu.aop_immd.from_cast_remat) {
	  sprintf(buffer,"#0x%02x",ptr_type);
	  aop->aopu.aop_immd.aop_immd2 = Safe_calloc (1, strlen (buffer) + 1);
	  strcpy (aop->aopu.aop_immd.aop_immd2, buffer);
  }

  return aop;
}

/*-----------------------------------------------------------------*/
/* regsInCommon - two operands have some registers in common       */
/*-----------------------------------------------------------------*/
static bool
regsInCommon (operand * op1, operand * op2)
{
  symbol *sym1, *sym2;
  int i;

  /* if they have registers in common */
  if (!IS_SYMOP (op1) || !IS_SYMOP (op2))
    return FALSE;

  sym1 = OP_SYMBOL (op1);
  sym2 = OP_SYMBOL (op2);

  if (sym1->nRegs == 0 || sym2->nRegs == 0)
    return FALSE;

  for (i = 0; i < sym1->nRegs; i++)
    {
      int j;
      if (!sym1->regs[i])
	continue;

      for (j = 0; j < sym2->nRegs; j++)
	{
	  if (!sym2->regs[j])
	    continue;

	  if (sym2->regs[j] == sym1->regs[i])
	    return TRUE;
	}
    }

  return FALSE;
}

/*-----------------------------------------------------------------*/
/* operandsEqu - equivalent                                        */
/*-----------------------------------------------------------------*/
static bool
operandsEqu (operand * op1, operand * op2)
{
  symbol *sym1, *sym2;

  /* if they not symbols */
  if (!IS_SYMOP (op1) || !IS_SYMOP (op2))
    return FALSE;

  sym1 = OP_SYMBOL (op1);
  sym2 = OP_SYMBOL (op2);

  /* if both are itemps & one is spilt
     and the other is not then false */
  if (IS_ITEMP (op1) && IS_ITEMP (op2) &&
      sym1->isspilt != sym2->isspilt)
    return FALSE;

  /* if they are the same */
  if (sym1 == sym2)
    return TRUE;

  if (strcmp (sym1->rname, sym2->rname) == 0)
    return TRUE;


  /* if left is a tmp & right is not */
  if (IS_ITEMP (op1) &&
      !IS_ITEMP (op2) &&
      sym1->isspilt &&
      (sym1->usl.spillLoc == sym2))
    return TRUE;

  if (IS_ITEMP (op2) &&
      !IS_ITEMP (op1) &&
      sym2->isspilt &&
      sym1->level > 0 &&
      (sym2->usl.spillLoc == sym1))
    return TRUE;

  return FALSE;
}

/*-----------------------------------------------------------------*/
/* sameRegs - two asmops have the same registers                   */
/*-----------------------------------------------------------------*/
static bool
sameRegs (asmop * aop1, asmop * aop2)
{
  int i;

  if (aop1 == aop2)
    return TRUE;

  if (aop1->type != AOP_REG ||
      aop2->type != AOP_REG)
    return FALSE;

  if (aop1->size != aop2->size)
    return FALSE;

  for (i = 0; i < aop1->size; i++)
    if (aop1->aopu.aop_reg[i] !=
	aop2->aopu.aop_reg[i])
      return FALSE;

  return TRUE;
}

/*-----------------------------------------------------------------*/
/* aopOp - allocates an asmop for an operand  :                    */
/*-----------------------------------------------------------------*/
static void
aopOp (operand * op, iCode * ic, bool result)
{
  asmop *aop;
  symbol *sym;
  int i;

  if (!op)
    return;

  /* if this a literal */
  if (IS_OP_LITERAL (op))
    {
      op->aop = aop = newAsmop (AOP_LIT);
      aop->aopu.aop_lit = op->operand.valOperand;
      aop->size = getSize (operandType (op));
      return;
    }

  /* if already has a asmop then continue */
  if (op->aop )
    return;

  /* if the underlying symbol has a aop */
  if (IS_SYMOP (op) && OP_SYMBOL (op)->aop)
    {
      op->aop = OP_SYMBOL (op)->aop;
      return;
    }

  /* if this is a true symbol */
  if (IS_TRUE_SYMOP (op))
    {
      op->aop = aopForSym (ic, OP_SYMBOL (op), result);
      return;
    }

  /* this is a temporary : this has
     only four choices :
     a) register
     b) spillocation
     c) rematerialize
     d) conditional
     e) can be a return use only */

  sym = OP_SYMBOL (op);

  /* if the type is a conditional */
  if (sym->regType == REG_CND)
    {
      aop = op->aop = sym->aop = newAsmop (AOP_CRY);
      aop->size = 0;
      return;
    }

  /* if it is spilt then two situations
     a) is rematerialize
     b) has a spill location */
  if (sym->isspilt || sym->nRegs == 0)
    {

      /* rematerialize it NOW */
      if (sym->remat)
	{
	  sym->aop = op->aop = aop =
	    aopForRemat (sym);
	  aop->size = getSize (sym->type);
	  return;
	}

      if (sym->accuse)
	{
	  int i;
	  aop = op->aop = sym->aop = newAsmop (AOP_ACC);
	  aop->size = getSize (sym->type);
	  for (i = 0; i < 2; i++)
	    aop->aopu.aop_str[i] = accUse[i];
	  return;
	}

      if (sym->ruonly)
	{
	  unsigned i;

	  aop = op->aop = sym->aop = newAsmop (AOP_STR);
	  aop->size = getSize (sym->type);
	  for (i = 0; i < fReturnSizeMCS51; i++)
	    aop->aopu.aop_str[i] = fReturn[i];
	  return;
	}

      if (sym->usl.spillLoc)
        {
          if (getSize(sym->type) != getSize(sym->usl.spillLoc->type))
            {
	      /* force a new aop if sizes differ */
	      sym->usl.spillLoc->aop = NULL;
	    }
	  sym->aop = op->aop = aop =
	             aopForSym (ic, sym->usl.spillLoc, result);
	  aop->size = getSize (sym->type);
	  return;
        }

      /* else must be a dummy iTemp */
      sym->aop = op->aop = aop = newAsmop (AOP_DUMMY);
      aop->size = getSize (sym->type);
      return;
    }

  /* must be in a register */
  sym->aop = op->aop = aop = newAsmop (AOP_REG);
  aop->size = sym->nRegs;
  for (i = 0; i < sym->nRegs; i++)
    aop->aopu.aop_reg[i] = sym->regs[i];
}

/*-----------------------------------------------------------------*/
/* freeAsmop - free up the asmop given to an operand               */
/*----------------------------------------------------------------*/
static void
freeAsmop (operand * op, asmop * aaop, iCode * ic, bool pop)
{
  asmop *aop;

  if (!op)
    aop = aaop;
  else
    aop = op->aop;

  if (!aop)
    return;

  if (aop->freed)
    goto dealloc;

  aop->freed = 1;

  /* depending on the asmop type only three cases need work AOP_RO
     , AOP_R1 && AOP_STK */
  switch (aop->type)
    {
    case AOP_R0:
      if (_G.r0Pushed)
	{
	  if (pop)
	    {
	      emitcode ("pop", "ar0");
	      _G.r0Pushed--;
	    }
	}
      bitVectUnSetBit (ic->rUsed, R0_IDX);
      break;

    case AOP_R1:
      if (_G.r1Pushed)
	{
	  if (pop)
	    {
	      emitcode ("pop", "ar1");
	      _G.r1Pushed--;
	    }
	}
      bitVectUnSetBit (ic->rUsed, R1_IDX);
      break;

    case AOP_STK:
      {
	int sz = aop->size;
	int stk = aop->aopu.aop_stk + aop->size - 1;
	bitVectUnSetBit (ic->rUsed, R0_IDX);
	bitVectUnSetBit (ic->rUsed, R1_IDX);

	getFreePtr (ic, &aop, FALSE);

	if (stk)
	  {
	    emitcode ("mov", "a,_bp");
	    emitcode ("add", "a,#0x%02x", ((char) stk) & 0xff);
	    emitcode ("mov", "%s,a", aop->aopu.aop_ptr->name);
	  }
	else
	  {
	    emitcode ("mov", "%s,_bp", aop->aopu.aop_ptr->name);
	  }

	while (sz--)
	  {
	    emitcode ("pop", "acc");
	    emitcode ("mov", "@%s,a", aop->aopu.aop_ptr->name);
	    if (!sz)
	      break;
	    emitcode ("dec", "%s", aop->aopu.aop_ptr->name);
	  }
	op->aop = aop;
	freeAsmop (op, NULL, ic, TRUE);
	if (_G.r1Pushed)
	  {
	    emitcode ("pop", "ar1");
	    _G.r1Pushed--;
	  }

	if (_G.r0Pushed)
	  {
	    emitcode ("pop", "ar0");
	    _G.r0Pushed--;
	  }
      }
    }

dealloc:
  /* all other cases just dealloc */
  if (op)
    {
      op->aop = NULL;
      if (IS_SYMOP (op))
	{
	  OP_SYMBOL (op)->aop = NULL;
	  /* if the symbol has a spill */
	  if (SPIL_LOC (op))
	    SPIL_LOC (op)->aop = NULL;
	}
    }
}

/*-----------------------------------------------------------------*/
/* aopGetUsesAcc - indicates ahead of time whether aopGet() will   */
/*                 clobber the accumulator                         */
/*-----------------------------------------------------------------*/
static bool
aopGetUsesAcc (asmop *aop, int offset)
{
  if (offset > (aop->size - 1))
    return FALSE;

  switch (aop->type)
    {

    case AOP_R0:
    case AOP_R1:
      if (aop->paged)
	return TRUE;
      return FALSE;
    case AOP_DPTR:
      return TRUE;
    case AOP_IMMD:
      return FALSE;
    case AOP_DIR:
      return FALSE;
    case AOP_REG:
      wassert(strcmp(aop->aopu.aop_reg[offset]->name, "a"));
      return FALSE;
    case AOP_CRY:
      return TRUE;
    case AOP_ACC:
      return TRUE;
    case AOP_LIT:
      return FALSE;
    case AOP_STR:
      if (strcmp (aop->aopu.aop_str[offset], "a") == 0)
	return TRUE;
      return FALSE;
    case AOP_DUMMY:
      return FALSE;
    default:
      /* Error case --- will have been caught already */
      wassert(0);
      return FALSE;
    }
}

/*-----------------------------------------------------------------*/
/* aopGet - for fetching value of the aop                          */
/*-----------------------------------------------------------------*/
static char *
aopGet (asmop * aop, int offset, bool bit16, bool dname)
{
  char *s = buffer;
  char *rs;

  /* offset is greater than
     size then zero */
  if (offset > (aop->size - 1) &&
      aop->type != AOP_LIT)
    return zero;

  /* depending on type */
  switch (aop->type)
    {
    case AOP_DUMMY:
      return zero;

    case AOP_R0:
    case AOP_R1:
      /* if we need to increment it */
      while (offset > aop->coff)
	{
	  emitcode ("inc", "%s", aop->aopu.aop_ptr->name);
	  aop->coff++;
	}

      while (offset < aop->coff)
	{
	  emitcode ("dec", "%s", aop->aopu.aop_ptr->name);
	  aop->coff--;
	}

      aop->coff = offset;
      if (aop->paged)
	{
	  emitcode ("movx", "a,@%s", aop->aopu.aop_ptr->name);
	  return (dname ? "acc" : "a");
	}
      sprintf (s, "@%s", aop->aopu.aop_ptr->name);
      rs = Safe_calloc (1, strlen (s) + 1);
      strcpy (rs, s);
      return rs;

    case AOP_DPTR:
      if (aop->code && aop->coff==0 && offset>=1) {
	emitcode ("mov", "a,#0x%02x", offset);
	emitcode ("movc", "a,@a+dptr");
	return (dname ? "acc" : "a");
      }

      while (offset > aop->coff)
	{
	  emitcode ("inc", "dptr");
	  aop->coff++;
	}

      while (offset < aop->coff)
	{
	  emitcode ("lcall", "__decdptr");
	  aop->coff--;
	}

      aop->coff = offset;
      if (aop->code)
	{
	  emitcode ("clr", "a");
	  emitcode ("movc", "a,@a+dptr");
	}
      else
	{
	  emitcode ("movx", "a,@dptr");
	}
      return (dname ? "acc" : "a");


    case AOP_IMMD:
      if (aop->aopu.aop_immd.from_cast_remat && (offset == (aop->size-1))) {
	      sprintf(s,"%s",aop->aopu.aop_immd.aop_immd2);
      } else if (bit16)
	sprintf (s, "#%s", aop->aopu.aop_immd.aop_immd1);
      else if (offset)
	sprintf (s, "#(%s >> %d)",
		 aop->aopu.aop_immd.aop_immd1,
		 offset * 8);
      else
	sprintf (s, "#%s",
		 aop->aopu.aop_immd.aop_immd1);
      rs = Safe_calloc (1, strlen (s) + 1);
      strcpy (rs, s);
      return rs;

    case AOP_DIR:
      if (offset)
	sprintf (s, "(%s + %d)",
		 aop->aopu.aop_dir,
		 offset);
      else
	sprintf (s, "%s", aop->aopu.aop_dir);
      rs = Safe_calloc (1, strlen (s) + 1);
      strcpy (rs, s);
      return rs;

    case AOP_REG:
      if (dname)
	return aop->aopu.aop_reg[offset]->dname;
      else
	return aop->aopu.aop_reg[offset]->name;

    case AOP_CRY:
      emitcode ("clr", "a");
      emitcode ("mov", "c,%s", aop->aopu.aop_dir);
      emitcode ("rlc", "a");
      return (dname ? "acc" : "a");

    case AOP_ACC:
      if (!offset && dname)
	return "acc";
      return aop->aopu.aop_str[offset];

    case AOP_LIT:
      return aopLiteral (aop->aopu.aop_lit, offset);

    case AOP_STR:
      aop->coff = offset;
      if (strcmp (aop->aopu.aop_str[offset], "a") == 0 &&
	  dname)
	return "acc";

      return aop->aopu.aop_str[offset];

    }

  werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
	  "aopget got unsupported aop->type");
  exit (1);
}
/*-----------------------------------------------------------------*/
/* aopPut - puts a string for a aop                                */
/*-----------------------------------------------------------------*/
static void
aopPut (asmop * aop, const char *s, int offset, bool bvolatile)
{
  char *d = buffer;

  if (aop->size && offset > (aop->size - 1))
    {
      werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
	      "aopPut got offset > aop->size");
      exit (1);
    }

  /* will assign value to value */
  /* depending on where it is ofcourse */
  switch (aop->type)
    {
    case AOP_DUMMY:
      MOVA (s);		/* read s in case it was volatile */
      break;

    case AOP_DIR:
      if (offset)
	sprintf (d, "(%s + %d)",
		 aop->aopu.aop_dir, offset);
      else
	sprintf (d, "%s", aop->aopu.aop_dir);

      if (strcmp (d, s) ||
          bvolatile)
	emitcode ("mov", "%s,%s", d, s);

      break;

    case AOP_REG:
      if (strcmp (aop->aopu.aop_reg[offset]->name, s) != 0 &&
	  strcmp (aop->aopu.aop_reg[offset]->dname, s) != 0)
	{
	  if (*s == '@' ||
	      strcmp (s, "r0") == 0 ||
	      strcmp (s, "r1") == 0 ||
	      strcmp (s, "r2") == 0 ||
	      strcmp (s, "r3") == 0 ||
	      strcmp (s, "r4") == 0 ||
	      strcmp (s, "r5") == 0 ||
	      strcmp (s, "r6") == 0 ||
	      strcmp (s, "r7") == 0)
	    emitcode ("mov", "%s,%s",
		      aop->aopu.aop_reg[offset]->dname, s);
	  else
	    emitcode ("mov", "%s,%s",
		      aop->aopu.aop_reg[offset]->name, s);
	}
      break;

    case AOP_DPTR:
      if (aop->code)
	{
	  werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
		  "aopPut writing to code space");
	  exit (1);
	}

      while (offset > aop->coff)
	{
	  aop->coff++;
	  emitcode ("inc", "dptr");
	}

      while (offset < aop->coff)
	{
	  aop->coff--;
	  emitcode ("lcall", "__decdptr");
	}

      aop->coff = offset;

      /* if not in accumulater */
      MOVA (s);

      emitcode ("movx", "@dptr,a");
      break;

    case AOP_R0:
    case AOP_R1:
      while (offset > aop->coff)
	{
	  aop->coff++;
	  emitcode ("inc", "%s", aop->aopu.aop_ptr->name);
	}
      while (offset < aop->coff)
	{
	  aop->coff--;
	  emitcode ("dec", "%s", aop->aopu.aop_ptr->name);
	}
      aop->coff = offset;

      if (aop->paged)
	{
	  MOVA (s);
	  emitcode ("movx", "@%s,a", aop->aopu.aop_ptr->name);

	}
      else if (*s == '@')
	{
	  MOVA (s);
	  emitcode ("mov", "@%s,a", aop->aopu.aop_ptr->name);
	}
      else if (strcmp (s, "r0") == 0 ||
	       strcmp (s, "r1") == 0 ||
	       strcmp (s, "r2") == 0 ||
	       strcmp (s, "r3") == 0 ||
	       strcmp (s, "r4") == 0 ||
	       strcmp (s, "r5") == 0 ||
	       strcmp (s, "r6") == 0 ||
	       strcmp (s, "r7") == 0)
	{
	  char buffer[10];
	  sprintf (buffer, "a%s", s);
	  emitcode ("mov", "@%s,%s",
		    aop->aopu.aop_ptr->name, buffer);
	}
      else
	emitcode ("mov", "@%s,%s", aop->aopu.aop_ptr->name, s);

      break;

    case AOP_STK:
      if (strcmp (s, "a") == 0)
	emitcode ("push", "acc");
      else
	if (*s=='@') {
	  MOVA(s);
	  emitcode ("push", "acc");
	} else {
	  emitcode ("push", s);
	}

      break;

    case AOP_CRY:
      /* if bit variable */
      if (!aop->aopu.aop_dir)
	{
	  emitcode ("clr", "a");
	  emitcode ("rlc", "a");
	}
      else
	{
	  if (s == zero)
	    emitcode ("clr", "%s", aop->aopu.aop_dir);
	  else if (s == one)
	    emitcode ("setb", "%s", aop->aopu.aop_dir);
	  else if (!strcmp (s, "c"))
	    emitcode ("mov", "%s,c", aop->aopu.aop_dir);
	  else
	    {
	      if (strcmp (s, "a"))
		{
		  MOVA (s);
		}
	      {
		/* set C, if a >= 1 */
                emitcode ("add", "a,#0xff");
		emitcode ("mov", "%s,c", aop->aopu.aop_dir);
	      }
	    }
	}
      break;

    case AOP_STR:
      aop->coff = offset;
      if (strcmp (aop->aopu.aop_str[offset], s) ||
          bvolatile)
	emitcode ("mov", "%s,%s", aop->aopu.aop_str[offset], s);
      break;

    case AOP_ACC:
      aop->coff = offset;
      if (!offset && (strcmp (s, "acc") == 0) &&
          !bvolatile)
	break;

      if (strcmp (aop->aopu.aop_str[offset], s) &&
          !bvolatile)
	emitcode ("mov", "%s,%s", aop->aopu.aop_str[offset], s);
      break;

    default:
      werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
	      "aopPut got unsupported aop->type");
      exit (1);
    }

}


#if 0
/*-----------------------------------------------------------------*/
/* pointToEnd :- points to the last byte of the operand            */
/*-----------------------------------------------------------------*/
static void
pointToEnd (asmop * aop)
{
  int count;
  if (!aop)
    return;

  aop->coff = count = (aop->size - 1);
  switch (aop->type)
    {
    case AOP_R0:
    case AOP_R1:
      while (count--)
	emitcode ("inc", "%s", aop->aopu.aop_ptr->name);
      break;
    case AOP_DPTR:
      while (count--)
	emitcode ("inc", "dptr");
      break;
    }

}
#endif

/*-----------------------------------------------------------------*/
/* reAdjustPreg - points a register back to where it should        */
/*-----------------------------------------------------------------*/
static void
reAdjustPreg (asmop * aop)
{
  if ((aop->coff==0) || aop->size <= 1)
    return;

  switch (aop->type)
    {
    case AOP_R0:
    case AOP_R1:
      while (aop->coff--)
	emitcode ("dec", "%s", aop->aopu.aop_ptr->name);
      break;
    case AOP_DPTR:
      while (aop->coff--)
	{
	  emitcode ("lcall", "__decdptr");
	}
      break;
    }
  aop->coff = 0;
}

#define AOP(op) op->aop
#define AOP_TYPE(op) AOP(op)->type
#define AOP_SIZE(op) AOP(op)->size
#define IS_AOP_PREG(x) (AOP(x) && (AOP_TYPE(x) == AOP_R1 || \
                       AOP_TYPE(x) == AOP_R0))

#define AOP_NEEDSACC(x) (AOP(x) && (AOP_TYPE(x) == AOP_CRY ||  \
                        AOP_TYPE(x) == AOP_DPTR || AOP(x)->paged))

#define AOP_INPREG(x) (x && (x->type == AOP_REG &&                        \
                      (x->aopu.aop_reg[0] == mcs51_regWithIdx(R0_IDX) || \
                      x->aopu.aop_reg[0] == mcs51_regWithIdx(R1_IDX) )))


/*-----------------------------------------------------------------*/
/* opIsGptr: returns non-zero if the passed operand is       */
/* a generic pointer type.             */
/*-----------------------------------------------------------------*/
static int
opIsGptr (operand * op)
{
  sym_link *type = operandType (op);

  if ((AOP_SIZE (op) == GPTRSIZE) && IS_GENPTR (type))
    {
      return 1;
    }
  return 0;
}

/*-----------------------------------------------------------------*/
/* getDataSize - get the operand data size                         */
/*-----------------------------------------------------------------*/
static int
getDataSize (operand * op)
{
  int size;
  size = AOP_SIZE (op);
  if (size == GPTRSIZE)
    {
      sym_link *type = operandType (op);
      if (IS_GENPTR (type))
	{
	  /* generic pointer; arithmetic operations
	   * should ignore the high byte (pointer type).
	   */
	  size--;
	}
    }
  return size;
}

/*-----------------------------------------------------------------*/
/* outAcc - output Acc                                             */
/*-----------------------------------------------------------------*/
static void
outAcc (operand * result)
{
  int size, offset;
  size = getDataSize (result);
  if (size)
    {
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      size--;
      offset = 1;
      /* unsigned or positive */
      while (size--)
	{
	  aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
	}
    }
}

/*-----------------------------------------------------------------*/
/* outBitC - output a bit C                                        */
/*-----------------------------------------------------------------*/
static void
outBitC (operand * result)
{
  /* if the result is bit */
  if (AOP_TYPE (result) == AOP_CRY)
    aopPut (AOP (result), "c", 0, isOperandVolatile (result, FALSE));
  else
    {
      emitcode ("clr", "a");
      emitcode ("rlc", "a");
      outAcc (result);
    }
}

/*-----------------------------------------------------------------*/
/* toBoolean - emit code for orl a,operator(sizeop)                */
/*-----------------------------------------------------------------*/
static void
toBoolean (operand * oper)
{
  int size = AOP_SIZE (oper) - 1;
  int offset = 1;
  MOVA (aopGet (AOP (oper), 0, FALSE, FALSE));
  while (size--)
    emitcode ("orl", "a,%s", aopGet (AOP (oper), offset++, FALSE, FALSE));
}


/*-----------------------------------------------------------------*/
/* genNot - generate code for ! operation                          */
/*-----------------------------------------------------------------*/
static void
genNot (iCode * ic)
{
  symbol *tlbl;

  D(emitcode (";     genNot",""));

  /* assign asmOps to operand & result */
  aopOp (IC_LEFT (ic), ic, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE);

  /* if in bit space then a special case */
  if (AOP_TYPE (IC_LEFT (ic)) == AOP_CRY)
    {
      emitcode ("mov", "c,%s", IC_LEFT (ic)->aop->aopu.aop_dir);
      emitcode ("cpl", "c");
      outBitC (IC_RESULT (ic));
      goto release;
    }

  toBoolean (IC_LEFT (ic));

  tlbl = newiTempLabel (NULL);
  emitcode ("cjne", "a,#0x01,%05d$", tlbl->key + 100);
  emitcode ("", "%05d$:", tlbl->key + 100);
  outBitC (IC_RESULT (ic));

release:
  /* release the aops */
  freeAsmop (IC_LEFT (ic), NULL, ic, (RESULTONSTACK (ic) ? 0 : 1));
  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
}


/*-----------------------------------------------------------------*/
/* genCpl - generate code for complement                           */
/*-----------------------------------------------------------------*/
static void
genCpl (iCode * ic)
{
  int offset = 0;
  int size;
  symbol *tlbl;

  D(emitcode (";     genCpl",""));

  /* assign asmOps to operand & result */
  aopOp (IC_LEFT (ic), ic, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE);

  /* special case if in bit space */
  if (AOP_TYPE (IC_RESULT (ic)) == AOP_CRY)
    {
      if (AOP_TYPE (IC_LEFT (ic)) == AOP_CRY)
	{
	  emitcode ("mov", "c,%s", IC_LEFT (ic)->aop->aopu.aop_dir);
	  emitcode ("cpl", "c");
	  emitcode ("mov", "%s,c", IC_RESULT (ic)->aop->aopu.aop_dir);
	  goto release;
	}

      tlbl=newiTempLabel(NULL);
      if (AOP_TYPE (IC_LEFT (ic)) == AOP_ACC ||
	  AOP_TYPE (IC_LEFT (ic)) == AOP_REG ||
	  IS_AOP_PREG (IC_LEFT (ic)))
	{
	  emitcode ("cjne", "%s,#0x01,%05d$",
		    aopGet (AOP (IC_LEFT (ic)), 0, FALSE, FALSE),
		    tlbl->key + 100);
	}
      else
	{
	  char *l = aopGet (AOP (IC_LEFT (ic)), 0, FALSE, FALSE);
	  MOVA (l);
	  emitcode ("cjne", "a,#0x01,%05d$", tlbl->key + 100);
	}
      emitcode ("", "%05d$:", tlbl->key + 100);
      outBitC (IC_RESULT(ic));
      goto release;
    }

  size = AOP_SIZE (IC_RESULT (ic));
  while (size--)
    {
      char *l = aopGet (AOP (IC_LEFT (ic)), offset, FALSE, FALSE);
      MOVA (l);
      emitcode ("cpl", "a");
      aopPut (AOP (IC_RESULT (ic)), "a", offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
    }


release:
  /* release the aops */
  freeAsmop (IC_LEFT (ic), NULL, ic, (RESULTONSTACK (ic) ? 0 : 1));
  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genUminusFloat - unary minus for floating points                */
/*-----------------------------------------------------------------*/
static void
genUminusFloat (operand * op, operand * result)
{
  int size, offset = 0;
  char *l;

  D(emitcode (";     genUminusFloat",""));

  /* for this we just copy and then flip the bit */

  size = AOP_SIZE (op) - 1;

  while (size--)
    {
      aopPut (AOP (result),
	      aopGet (AOP (op), offset, FALSE, FALSE),
	      offset,
	      isOperandVolatile (result, FALSE));
      offset++;
    }

  l = aopGet (AOP (op), offset, FALSE, FALSE);

  MOVA (l);

  emitcode ("cpl", "acc.7");
  aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* genUminus - unary minus code generation                         */
/*-----------------------------------------------------------------*/
static void
genUminus (iCode * ic)
{
  int offset, size;
  sym_link *optype, *rtype;


  D(emitcode (";     genUminus",""));

  /* assign asmops */
  aopOp (IC_LEFT (ic), ic, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE);

  /* if both in bit space then special
     case */
  if (AOP_TYPE (IC_RESULT (ic)) == AOP_CRY &&
      AOP_TYPE (IC_LEFT (ic)) == AOP_CRY)
    {

      emitcode ("mov", "c,%s", IC_LEFT (ic)->aop->aopu.aop_dir);
      emitcode ("cpl", "c");
      emitcode ("mov", "%s,c", IC_RESULT (ic)->aop->aopu.aop_dir);
      goto release;
    }

  optype = operandType (IC_LEFT (ic));
  rtype = operandType (IC_RESULT (ic));

  /* if float then do float stuff */
  if (IS_FLOAT (optype))
    {
      genUminusFloat (IC_LEFT (ic), IC_RESULT (ic));
      goto release;
    }

  /* otherwise subtract from zero */
  size = AOP_SIZE (IC_LEFT (ic));
  offset = 0;
  //CLRC ;
  while (size--)
    {
      char *l = aopGet (AOP (IC_LEFT (ic)), offset, FALSE, FALSE);
      if (!strcmp (l, "a"))
	{
	  if (offset == 0)
	    SETC;
	  emitcode ("cpl", "a");
	  emitcode ("addc", "a,#0");
	}
      else
	{
	  if (offset == 0)
	    CLRC;
	  emitcode ("clr", "a");
	  emitcode ("subb", "a,%s", l);
	}
      aopPut (AOP (IC_RESULT (ic)), "a", offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
    }

  /* if any remaining bytes in the result */
  /* we just need to propagate the sign   */
  if ((size = (AOP_SIZE (IC_RESULT (ic)) - AOP_SIZE (IC_LEFT (ic)))))
    {
      emitcode ("rlc", "a");
      emitcode ("subb", "a,acc");
      while (size--)
	aopPut (AOP (IC_RESULT (ic)), "a", offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
    }

release:
  /* release the aops */
  freeAsmop (IC_LEFT (ic), NULL, ic, (RESULTONSTACK (ic) ? 0 : 1));
  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* saveRegisters - will look for a call and save the registers     */
/*-----------------------------------------------------------------*/
static void
saveRegisters (iCode * lic)
{
  int i;
  iCode *ic;
  bitVect *rsave;

  /* look for call */
  for (ic = lic; ic; ic = ic->next)
    if (ic->op == CALL || ic->op == PCALL)
      break;

  if (!ic)
    {
      fprintf (stderr, "found parameter push with no function call\n");
      return;
    }

  /* if the registers have been saved already or don't need to be then
     do nothing */
  if (ic->regsSaved)
    return;
  if (IS_SYMOP(IC_LEFT(ic)) &&
      (IFFUNC_CALLEESAVES(OP_SYMBOL(IC_LEFT(ic))->type) ||
       IFFUNC_ISNAKED(OP_SYM_TYPE(IC_LEFT (ic)))))
    return;

  /* safe the registers in use at this time but skip the
     ones for the result */
  rsave = bitVectCplAnd (bitVectCopy (ic->rMask),
			 mcs51_rUmaskForOp (IC_RESULT(ic)));

  ic->regsSaved = 1;
  if (options.useXstack)
    {
      if (bitVectBitValue (rsave, R0_IDX))
	emitcode ("mov", "b,r0");
      emitcode ("mov", "r0,%s", spname);
      for (i = 0; i < mcs51_nRegs; i++)
	{
	  if (bitVectBitValue (rsave, i))
	    {
	      if (i == R0_IDX)
		emitcode ("mov", "a,b");
	      else
		emitcode ("mov", "a,%s", mcs51_regWithIdx (i)->name);
	      emitcode ("movx", "@r0,a");
	      emitcode ("inc", "r0");
	    }
	}
      emitcode ("mov", "%s,r0", spname);
      if (bitVectBitValue (rsave, R0_IDX))
	emitcode ("mov", "r0,b");
    }
  else
    for (i = 0; i < mcs51_nRegs; i++)
      {
	if (bitVectBitValue (rsave, i))
	  emitcode ("push", "%s", mcs51_regWithIdx (i)->dname);
      }
}

/*-----------------------------------------------------------------*/
/* unsaveRegisters - pop the pushed registers                      */
/*-----------------------------------------------------------------*/
static void
unsaveRegisters (iCode * ic)
{
  int i;
  bitVect *rsave;

  /* restore the registers in use at this time but skip the
     ones for the result */
  rsave = bitVectCplAnd (bitVectCopy (ic->rMask),
			 mcs51_rUmaskForOp (IC_RESULT(ic)));

  if (options.useXstack)
    {
      emitcode ("mov", "r0,%s", spname);
      for (i = mcs51_nRegs; i >= 0; i--)
	{
	  if (bitVectBitValue (rsave, i))
	    {
	      emitcode ("dec", "r0");
	      emitcode ("movx", "a,@r0");
	      if (i == R0_IDX)
		emitcode ("mov", "b,a");
	      else
		emitcode ("mov", "%s,a", mcs51_regWithIdx (i)->name);
	    }

	}
      emitcode ("mov", "%s,r0", spname);
      if (bitVectBitValue (rsave, R0_IDX))
	emitcode ("mov", "r0,b");
    }
  else
    for (i = mcs51_nRegs; i >= 0; i--)
      {
	if (bitVectBitValue (rsave, i))
	  emitcode ("pop", "%s", mcs51_regWithIdx (i)->dname);
      }

}


/*-----------------------------------------------------------------*/
/* pushSide -                */
/*-----------------------------------------------------------------*/
static void
pushSide (operand * oper, int size)
{
  int offset = 0;
  while (size--)
    {
      char *l = aopGet (AOP (oper), offset++, FALSE, TRUE);
      if (AOP_TYPE (oper) != AOP_REG &&
	  AOP_TYPE (oper) != AOP_DIR &&
	  strcmp (l, "a"))
	{
	  emitcode ("mov", "a,%s", l);
	  emitcode ("push", "acc");
	}
      else
	emitcode ("push", "%s", l);
    }
}

/*-----------------------------------------------------------------*/
/* assignResultValue -               */
/*-----------------------------------------------------------------*/
static void
assignResultValue (operand * oper)
{
  int offset = 0;
  int size = AOP_SIZE (oper);
  while (size--)
    {
      aopPut (AOP (oper), fReturn[offset], offset, isOperandVolatile (oper, FALSE));
      offset++;
    }
}


/*-----------------------------------------------------------------*/
/* genXpush - pushes onto the external stack                       */
/*-----------------------------------------------------------------*/
static void
genXpush (iCode * ic)
{
  asmop *aop = newAsmop (0);
  regs *r;
  int size, offset = 0;

  D(emitcode (";     genXpush",""));

  aopOp (IC_LEFT (ic), ic, FALSE);
  r = getFreePtr (ic, &aop, FALSE);


  emitcode ("mov", "%s,_spx", r->name);

  size = AOP_SIZE (IC_LEFT (ic));
  while (size--)
    {

      char *l = aopGet (AOP (IC_LEFT (ic)),
			offset++, FALSE, FALSE);
      MOVA (l);
      emitcode ("movx", "@%s,a", r->name);
      emitcode ("inc", "%s", r->name);

    }


  emitcode ("mov", "_spx,%s", r->name);

  freeAsmop (NULL, aop, ic, TRUE);
  freeAsmop (IC_LEFT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genIpush - genrate code for pushing this gets a little complex  */
/*-----------------------------------------------------------------*/
static void
genIpush (iCode * ic)
{
  int size, offset = 0;
  char *l;

  D(emitcode (";     genIpush",""));

  /* if this is not a parm push : ie. it is spill push
     and spill push is always done on the local stack */
  if (!ic->parmPush)
    {

      /* and the item is spilt then do nothing */
      if (OP_SYMBOL (IC_LEFT (ic))->isspilt)
	return;

      aopOp (IC_LEFT (ic), ic, FALSE);
      size = AOP_SIZE (IC_LEFT (ic));
      /* push it on the stack */
      while (size--)
	{
	  l = aopGet (AOP (IC_LEFT (ic)), offset++, FALSE, TRUE);
	  if (*l == '#')
	    {
	      MOVA (l);
	      l = "acc";
	    }
	  emitcode ("push", "%s", l);
	}
      return;
    }

  /* this is a paramter push: in this case we call
     the routine to find the call and save those
     registers that need to be saved */
  saveRegisters (ic);

  /* if use external stack then call the external
     stack pushing routine */
  if (options.useXstack)
    {
      genXpush (ic);
      return;
    }

  /* then do the push */
  aopOp (IC_LEFT (ic), ic, FALSE);


  // pushSide(IC_LEFT(ic), AOP_SIZE(IC_LEFT(ic)));
  size = AOP_SIZE (IC_LEFT (ic));

  while (size--)
    {
      l = aopGet (AOP (IC_LEFT (ic)), offset++, FALSE, TRUE);
      if (AOP_TYPE (IC_LEFT (ic)) != AOP_REG &&
	  AOP_TYPE (IC_LEFT (ic)) != AOP_DIR &&
	  strcmp (l, "a"))
	{
	  emitcode ("mov", "a,%s", l);
	  emitcode ("push", "acc");
	}
      else
	emitcode ("push", "%s", l);
    }

  freeAsmop (IC_LEFT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genIpop - recover the registers: can happen only for spilling   */
/*-----------------------------------------------------------------*/
static void
genIpop (iCode * ic)
{
  int size, offset;

  D(emitcode (";     genIpop",""));

  /* if the temp was not pushed then */
  if (OP_SYMBOL (IC_LEFT (ic))->isspilt)
    return;

  aopOp (IC_LEFT (ic), ic, FALSE);
  size = AOP_SIZE (IC_LEFT (ic));
  offset = (size - 1);
  while (size--)
    emitcode ("pop", "%s", aopGet (AOP (IC_LEFT (ic)), offset--,
				   FALSE, TRUE));

  freeAsmop (IC_LEFT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* unsaveRBank - restores the resgister bank from stack            */
/*-----------------------------------------------------------------*/
static void
unsaveRBank (int bank, iCode * ic, bool popPsw)
{
  int i;
  asmop *aop = NULL;
  regs *r = NULL;

  if (options.useXstack)
  {
      if (!ic)
      {
      	  /* Assume r0 is available for use. */
      	  r = mcs51_regWithIdx (R0_IDX);;
      }
      else
      {
	  aop = newAsmop (0);
	  r = getFreePtr (ic, &aop, FALSE);
      }
      emitcode ("mov", "%s,_spx", r->name);
  }

  if (popPsw)
    {
      if (options.useXstack)
      {
	  emitcode ("movx", "a,@%s", r->name);
	  emitcode ("mov", "psw,a");
	  emitcode ("dec", "%s", r->name);
	}
      else
      {
	emitcode ("pop", "psw");
      }
    }

  for (i = (mcs51_nRegs - 1); i >= 0; i--)
    {
      if (options.useXstack)
	{
	  emitcode ("movx", "a,@%s", r->name);
	  emitcode ("mov", "(%s+%d),a",
		    regs8051[i].base, 8 * bank + regs8051[i].offset);
	  emitcode ("dec", "%s", r->name);

	}
      else
	emitcode ("pop", "(%s+%d)",
		  regs8051[i].base, 8 * bank + regs8051[i].offset);
    }

  if (options.useXstack)
    {
      emitcode ("mov", "_spx,%s", r->name);
    }

  if (aop)
  {
      freeAsmop (NULL, aop, ic, TRUE);
  }
}

/*-----------------------------------------------------------------*/
/* saveRBank - saves an entire register bank on the stack          */
/*-----------------------------------------------------------------*/
static void
saveRBank (int bank, iCode * ic, bool pushPsw)
{
  int i;
  asmop *aop = NULL;
  regs *r = NULL;

  if (options.useXstack)
    {
      if (!ic)
      {
      	  /* Assume r0 is available for use. */
      	  r = mcs51_regWithIdx (R0_IDX);;
      }
      else
      {
      	  aop = newAsmop (0);
      	  r = getFreePtr (ic, &aop, FALSE);
      }
      emitcode ("mov", "%s,_spx", r->name);
    }

  for (i = 0; i < mcs51_nRegs; i++)
    {
      if (options.useXstack)
	{
	  emitcode ("inc", "%s", r->name);
	  emitcode ("mov", "a,(%s+%d)",
		    regs8051[i].base, 8 * bank + regs8051[i].offset);
	  emitcode ("movx", "@%s,a", r->name);
	}
      else
	emitcode ("push", "(%s+%d)",
		  regs8051[i].base, 8 * bank + regs8051[i].offset);
    }

  if (pushPsw)
    {
      if (options.useXstack)
	{
	  emitcode ("mov", "a,psw");
	  emitcode ("movx", "@%s,a", r->name);
	  emitcode ("inc", "%s", r->name);
	  emitcode ("mov", "_spx,%s", r->name);

	}
      else
      {
	emitcode ("push", "psw");
      }

      emitcode ("mov", "psw,#0x%02x", (bank << 3) & 0x00ff);
    }

    if (aop)
    {
        freeAsmop (NULL, aop, ic, TRUE);
    }

  if (ic)
  {
      ic->bankSaved = 1;
  }
}

/*-----------------------------------------------------------------*/
/* genSend - gen code for SEND	                                   */
/*-----------------------------------------------------------------*/
static void genSend(set *sendSet)
{
    iCode *sic;
    int rb1_count = 0 ;

    for (sic = setFirstItem (_G.sendSet); sic;
	 sic = setNextItem (_G.sendSet)) {
	  int size, offset = 0;
	  aopOp (IC_LEFT (sic), sic, FALSE);
	  size = AOP_SIZE (IC_LEFT (sic));

	  if (sic->argreg == 1) {
	      while (size--) {
		  char *l = aopGet (AOP (IC_LEFT (sic)), offset,
				    FALSE, FALSE);
		  if (strcmp (l, fReturn[offset]))
		      emitcode ("mov", "%s,%s", fReturn[offset], l);
		  offset++;
	      }
	      rb1_count = 0;
	  } else {
	      while (size--) {
		  emitcode ("mov","b1_%d,%s",rb1_count++,
			    aopGet (AOP (IC_LEFT (sic)), offset++,FALSE, FALSE));
	      }
	  }
	  freeAsmop (IC_LEFT (sic), NULL, sic, TRUE);
    }
}

/*-----------------------------------------------------------------*/
/* genCall - generates a call statement                            */
/*-----------------------------------------------------------------*/
static void
genCall (iCode * ic)
{
  sym_link *dtype;
//  bool restoreBank = FALSE;
  bool swapBanks = FALSE;

  D(emitcode(";     genCall",""));

  dtype = operandType (IC_LEFT (ic));
  /* if send set is not empty the assign */
  if (_G.sendSet)
    {
	if (IFFUNC_ISREENT(dtype)) { /* need to reverse the send set */
	    genSend(reverseSet(_G.sendSet));
	} else {
	    genSend(_G.sendSet);
	}

      _G.sendSet = NULL;
    }

  /* if we are calling a not _naked function that is not using
     the same register bank then we need to save the
     destination registers on the stack */
  dtype = operandType (IC_LEFT (ic));
  if (currFunc && dtype && !IFFUNC_ISNAKED(dtype) &&
      (FUNC_REGBANK (currFunc->type) != FUNC_REGBANK (dtype)) &&
       !IFFUNC_ISISR (dtype))
  {
      swapBanks = TRUE;
  }

  /* if caller saves & we have not saved then */
  if (!ic->regsSaved)
      saveRegisters (ic);

  if (swapBanks)
  {
        emitcode ("mov", "psw,#0x%02x",
           ((FUNC_REGBANK(dtype)) << 3) & 0xff);
  }

  /* make the call */
  emitcode ("lcall", "%s", (OP_SYMBOL (IC_LEFT (ic))->rname[0] ?
			    OP_SYMBOL (IC_LEFT (ic))->rname :
			    OP_SYMBOL (IC_LEFT (ic))->name));

  if (swapBanks)
  {
       emitcode ("mov", "psw,#0x%02x",
          ((FUNC_REGBANK(currFunc->type)) << 3) & 0xff);
  }

  /* if we need assign a result value */
  if ((IS_ITEMP (IC_RESULT (ic)) &&
       (OP_SYMBOL (IC_RESULT (ic))->nRegs ||
	OP_SYMBOL (IC_RESULT (ic))->accuse ||
	OP_SYMBOL (IC_RESULT (ic))->spildir)) ||
      IS_TRUE_SYMOP (IC_RESULT (ic)))
    {

      _G.accInUse++;
      aopOp (IC_RESULT (ic), ic, FALSE);
      _G.accInUse--;

      assignResultValue (IC_RESULT (ic));

      freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
    }

  /* adjust the stack for parameters if
     required */
  if (ic->parmBytes)
    {
      int i;
      if (ic->parmBytes > 3)
	{
	  emitcode ("mov", "a,%s", spname);
	  emitcode ("add", "a,#0x%02x", (-ic->parmBytes) & 0xff);
	  emitcode ("mov", "%s,a", spname);
	}
      else
	for (i = 0; i < ic->parmBytes; i++)
	  emitcode ("dec", "%s", spname);
    }

  /* if we hade saved some registers then unsave them */
  if (ic->regsSaved && !IFFUNC_CALLEESAVES(dtype))
    unsaveRegisters (ic);

//  /* if register bank was saved then pop them */
//  if (restoreBank)
//    unsaveRBank (FUNC_REGBANK (dtype), ic, FALSE);
}

/*-----------------------------------------------------------------*/
/* -10l - generates a call by pointer statement                */
/*-----------------------------------------------------------------*/
static void
genPcall (iCode * ic)
{
  sym_link *dtype;
  symbol *rlbl = newiTempLabel (NULL);
//  bool restoreBank=FALSE;
  bool swapBanks = FALSE;

  D(emitcode(";     genPCall",""));

  /* if caller saves & we have not saved then */
  if (!ic->regsSaved)
    saveRegisters (ic);

  /* if we are calling a not _naked function that is not using
     the same register bank then we need to save the
     destination registers on the stack */
  dtype = operandType (IC_LEFT (ic))->next;
  if (currFunc && dtype && !IFFUNC_ISNAKED(dtype) &&
      (FUNC_REGBANK (currFunc->type) != FUNC_REGBANK (dtype)) &&
      !IFFUNC_ISISR (dtype))
  {
//    saveRBank (FUNC_REGBANK (dtype), ic, TRUE);
//    restoreBank=TRUE;
      swapBanks = TRUE;
      // need caution message to user here
  }

  /* push the return address on to the stack */
  emitcode ("mov", "a,#%05d$", (rlbl->key + 100));
  emitcode ("push", "acc");
  emitcode ("mov", "a,#(%05d$ >> 8)", (rlbl->key + 100));
  emitcode ("push", "acc");

  /* now push the calling address */
  aopOp (IC_LEFT (ic), ic, FALSE);

  pushSide (IC_LEFT (ic), FPTRSIZE);

  freeAsmop (IC_LEFT (ic), NULL, ic, TRUE);

  /* if send set is not empty the assign */
  if (_G.sendSet)
    {
	genSend(reverseSet(_G.sendSet));
	_G.sendSet = NULL;
    }

  if (swapBanks)
  {
        emitcode ("mov", "psw,#0x%02x",
           ((FUNC_REGBANK(dtype)) << 3) & 0xff);
  }

  /* make the call */
  emitcode ("ret", "");
  emitcode ("", "%05d$:", (rlbl->key + 100));


  if (swapBanks)
  {
       emitcode ("mov", "psw,#0x%02x",
          ((FUNC_REGBANK(currFunc->type)) << 3) & 0xff);
  }

  /* if we need assign a result value */
  if ((IS_ITEMP (IC_RESULT (ic)) &&
       (OP_SYMBOL (IC_RESULT (ic))->nRegs ||
	OP_SYMBOL (IC_RESULT (ic))->spildir)) ||
      IS_TRUE_SYMOP (IC_RESULT (ic)))
    {

      _G.accInUse++;
      aopOp (IC_RESULT (ic), ic, FALSE);
      _G.accInUse--;

      assignResultValue (IC_RESULT (ic));

      freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
    }

  /* adjust the stack for parameters if
     required */
  if (ic->parmBytes)
    {
      int i;
      if (ic->parmBytes > 3)
	{
	  emitcode ("mov", "a,%s", spname);
	  emitcode ("add", "a,#0x%02x", (-ic->parmBytes) & 0xff);
	  emitcode ("mov", "%s,a", spname);
	}
      else
	for (i = 0; i < ic->parmBytes; i++)
	  emitcode ("dec", "%s", spname);

    }

//  /* if register bank was saved then unsave them */
//  if (restoreBank)
//    unsaveRBank (FUNC_REGBANK (dtype), ic, TRUE);

  /* if we hade saved some registers then
     unsave them */
  if (ic->regsSaved && !IFFUNC_CALLEESAVES(dtype))
    unsaveRegisters (ic);
}

/*-----------------------------------------------------------------*/
/* resultRemat - result  is rematerializable                       */
/*-----------------------------------------------------------------*/
static int
resultRemat (iCode * ic)
{
  if (SKIP_IC (ic) || ic->op == IFX)
    return 0;

  if (IC_RESULT (ic) && IS_ITEMP (IC_RESULT (ic)))
    {
      symbol *sym = OP_SYMBOL (IC_RESULT (ic));
      if (sym->remat && !POINTER_SET (ic))
	return 1;
    }

  return 0;
}

#if defined(__BORLANDC__) || defined(_MSC_VER)
#define STRCASECMP stricmp
#else
#define STRCASECMP strcasecmp
#endif

/*-----------------------------------------------------------------*/
/* inExcludeList - return 1 if the string is in exclude Reg list   */
/*-----------------------------------------------------------------*/
static int
regsCmp(void *p1, void *p2)
{
  return (STRCASECMP((char *)p1, (char *)(p2)) == 0);
}

static bool
inExcludeList (char *s)
{
  const char *p = setFirstItem(options.excludeRegsSet);

  if (p == NULL || STRCASECMP(p, "none") == 0)
    return FALSE;


  return isinSetWith(options.excludeRegsSet, s, regsCmp);
}

/*-----------------------------------------------------------------*/
/* genFunction - generated code for function entry                 */
/*-----------------------------------------------------------------*/
static void
genFunction (iCode * ic)
{
  symbol *sym;
  sym_link *ftype;
  bool   switchedPSW = FALSE;
  int calleesaves_saved_register = -1;

  _G.nRegsSaved = 0;
  /* create the function header */
  emitcode (";", "-----------------------------------------");
  emitcode (";", " function %s", (sym = OP_SYMBOL (IC_LEFT (ic)))->name);
  emitcode (";", "-----------------------------------------");

  emitcode ("", "%s:", sym->rname);
  ftype = operandType (IC_LEFT (ic));
  _G.currentFunc = sym;

  if (IFFUNC_ISNAKED(ftype))
  {
      emitcode(";", "naked function: no prologue.");
      return;
  }

  /* here we need to generate the equates for the
     register bank if required */
  if (FUNC_REGBANK (ftype) != rbank)
    {
      int i;

      rbank = FUNC_REGBANK (ftype);
      for (i = 0; i < mcs51_nRegs; i++)
	{
	  if (strcmp (regs8051[i].base, "0") == 0)
	    emitcode ("", "%s = 0x%02x",
		      regs8051[i].dname,
		      8 * rbank + regs8051[i].offset);
	  else
	    emitcode ("", "%s = %s + 0x%02x",
		      regs8051[i].dname,
		      regs8051[i].base,
		      8 * rbank + regs8051[i].offset);
	}
    }

  /* if this is an interrupt service routine then
     save acc, b, dpl, dph  */
  if (IFFUNC_ISISR (sym->type))
    {

      if (!inExcludeList ("acc"))
	emitcode ("push", "acc");
      if (!inExcludeList ("b"))
	emitcode ("push", "b");
      if (!inExcludeList ("dpl"))
	emitcode ("push", "dpl");
      if (!inExcludeList ("dph"))
	emitcode ("push", "dph");
      /* if this isr has no bank i.e. is going to
         run with bank 0 , then we need to save more
         registers :-) */
      if (!FUNC_REGBANK (sym->type))
	{

	  /* if this function does not call any other
	     function then we can be economical and
	     save only those registers that are used */
	  if (!IFFUNC_HASFCALL(sym->type))
	    {
	      int i;

	      /* if any registers used */
	      if (sym->regsUsed)
		{
		  /* save the registers used */
		  for (i = 0; i < sym->regsUsed->size; i++)
		    {
		      if (bitVectBitValue (sym->regsUsed, i) ||
			  (mcs51_ptrRegReq && (i == R0_IDX || i == R1_IDX)))
			emitcode ("push", "%s", mcs51_regWithIdx (i)->dname);
		    }
		}

	    }
	  else
	    {

	      /* this function has  a function call cannot
	         determines register usage so we will have to push the
	         entire bank */
		saveRBank (0, ic, FALSE);
		if (options.parms_in_bank1) {
		    int i;
		    for (i=0; i < 8 ; i++ ) {
			emitcode ("push","%s",rb1regs[i]);
		    }
		}
	    }
	}
	else
	{
	    /* This ISR uses a non-zero bank.
	     *
	     * We assume that the bank is available for our
	     * exclusive use.
	     *
	     * However, if this ISR calls a function which uses some
	     * other bank, we must save that bank entirely.
	     */
	    unsigned long banksToSave = 0;

	    if (IFFUNC_HASFCALL(sym->type))
	    {

#define MAX_REGISTER_BANKS 4

	        iCode *i;
	        int ix;

	        for (i = ic; i; i = i->next)
	        {
	            if (i->op == ENDFUNCTION)
	            {
	                /* we got to the end OK. */
	                break;
	            }

	            if (i->op == CALL)
	            {
	                sym_link *dtype;

	                dtype = operandType (IC_LEFT(i));
	                if (dtype
	                 && FUNC_REGBANK(dtype) != FUNC_REGBANK(sym->type))
	                {
	                     /* Mark this bank for saving. */
	                     if (FUNC_REGBANK(dtype) >= MAX_REGISTER_BANKS)
	                     {
	                         werror(E_NO_SUCH_BANK, FUNC_REGBANK(dtype));
	                     }
	                     else
	                     {
	                         banksToSave |= (1 << FUNC_REGBANK(dtype));
	                     }

	                     /* And note that we don't need to do it in
	                      * genCall.
	                      */
	                     i->bankSaved = 1;
	                }
	            }
	            if (i->op == PCALL)
	            {
	             	/* This is a mess; we have no idea what
	             	 * register bank the called function might
	             	 * use.
	             	 *
	             	 * The only thing I can think of to do is
	             	 * throw a warning and hope.
	             	 */
	             	werror(W_FUNCPTR_IN_USING_ISR);
	            }
	        }

		if (banksToSave && options.useXstack)
		{
		    /* Since we aren't passing it an ic,
		     * saveRBank will assume r0 is available to abuse.
		     *
		     * So switch to our (trashable) bank now, so
		     * the caller's R0 isn't trashed.
		     */
		    emitcode ("push", "psw");
		    emitcode ("mov", "psw,#0x%02x",
		    	      (FUNC_REGBANK (sym->type) << 3) & 0x00ff);
		    switchedPSW = TRUE;
		}

	        for (ix = 0; ix < MAX_REGISTER_BANKS; ix++)
	        {
	             if (banksToSave & (1 << ix))
	             {
	                 saveRBank(ix, NULL, FALSE);
	             }
	        }
	    }
	    // TODO: this needs a closer look
	    SPEC_ISR_SAVED_BANKS(currFunc->etype) = banksToSave;
	}
    }
  else
    {
      /* if callee-save to be used for this function
         then save the registers being used in this function */
      if (IFFUNC_CALLEESAVES(sym->type))
	{
	  int i;

	  /* if any registers used */
	  if (sym->regsUsed)
	    {
	      /* save the registers used */
	      for (i = 0; i < sym->regsUsed->size; i++)
		{
		  if (bitVectBitValue (sym->regsUsed, i) ||
		      (mcs51_ptrRegReq && (i == R0_IDX || i == R1_IDX)))
		    {
		      /* remember one saved register for later usage */
		      if (calleesaves_saved_register < 0)
		        calleesaves_saved_register = i;
		      emitcode ("push", "%s", mcs51_regWithIdx (i)->dname);
		      _G.nRegsSaved++;
		    }
		}
	    }
	}
    }

  /* set the register bank to the desired value */
  if (( /* FUNC_REGBANK (sym->type) || */ IFFUNC_ISISR (sym->type))
   && !switchedPSW)
    {
      emitcode ("push", "psw");
      emitcode ("mov", "psw,#0x%02x", (FUNC_REGBANK (sym->type) << 3) & 0x00ff);
    }

  if (IFFUNC_ISREENT (sym->type) || options.stackAuto)
    {

      if (options.useXstack)
	{
	  emitcode ("mov", "r0,%s", spname);
	  emitcode ("mov", "a,_bp");
	  emitcode ("movx", "@r0,a");
	  emitcode ("inc", "%s", spname);
	}
      else
	{
	  /* set up the stack */
	  emitcode ("push", "_bp");	/* save the callers stack  */
	}
      emitcode ("mov", "_bp,%s", spname);
    }

  /* adjust the stack for the function */
  if (sym->stack)
    {

      int i = sym->stack;
      if (i > 256)
	werror (W_STACK_OVERFLOW, sym->name);

      if (i > 3 && sym->recvSize < 4)
	{

	  emitcode ("mov", "a,sp");
	  emitcode ("add", "a,#0x%02x", ((char) sym->stack & 0xff));
	  emitcode ("mov", "sp,a");

	}
      else if (i > 5)
        {
	  if (IFFUNC_CALLEESAVES(sym->type))
	    {
	      /* if it's a callee-saves function we need a saved register */
	      if (calleesaves_saved_register >= 0)
		{
		  emitcode ("mov", "%s,a", mcs51_regWithIdx (calleesaves_saved_register)->dname);
		  emitcode ("mov", "a,sp");
		  emitcode ("add", "a,#0x%02x", ((char) sym->stack & 0xff));
		  emitcode ("mov", "sp,a");
		  emitcode ("mov", "a,%s", mcs51_regWithIdx (calleesaves_saved_register)->dname);
		}
	      else
		/* do it the hard way */
		while (i--)
		  emitcode ("inc", "sp");
	    }
	  else
	    {
	      /* not callee-saves, we can clobber r0 */
	      emitcode ("mov", "r0,a");
	      emitcode ("mov", "a,sp");
	      emitcode ("add", "a,#0x%02x", ((char) sym->stack & 0xff));
	      emitcode ("mov", "sp,a");
	      emitcode ("mov", "a,r0");
	    }
	}
      else
	while (i--)
	  emitcode ("inc", "sp");
    }

  if (sym->xstack)
    {

      emitcode ("mov", "a,_spx");
      emitcode ("add", "a,#0x%02x", ((char) sym->xstack & 0xff));
      emitcode ("mov", "_spx,a");
    }

  /* if critical function then turn interrupts off */
  if (IFFUNC_ISCRITICAL (ftype))
    {
      symbol *tlbl = newiTempLabel (NULL);
      emitcode ("setb", "c");
      emitcode ("jbc", "ea,%05d$", (tlbl->key + 100)); /* atomic test & clear */
      emitcode ("clr", "c");
      emitcode ("", "%05d$:", (tlbl->key + 100));
      emitcode ("push", "psw"); /* save old ea via c in psw */
    }
}

/*-----------------------------------------------------------------*/
/* genEndFunction - generates epilogue for functions               */
/*-----------------------------------------------------------------*/
static void
genEndFunction (iCode * ic)
{
  symbol *sym = OP_SYMBOL (IC_LEFT (ic));

  _G.currentFunc = NULL;
  if (IFFUNC_ISNAKED(sym->type))
  {
      emitcode(";", "naked function: no epilogue.");
      return;
  }

  if (IFFUNC_ISCRITICAL (sym->type))
    {
      emitcode ("pop", "psw"); /* restore ea via c in psw */
      emitcode ("mov", "ea,c");
    }

  if (IFFUNC_ISREENT (sym->type) || options.stackAuto)
    {
      emitcode ("mov", "%s,_bp", spname);
    }

  /* if use external stack but some variables were
     added to the local stack then decrement the
     local stack */
  if (options.useXstack && sym->stack)
    {
      emitcode ("mov", "a,sp");
      emitcode ("add", "a,#0x%02x", ((char) -sym->stack) & 0xff);
      emitcode ("mov", "sp,a");
    }


  if ((IFFUNC_ISREENT (sym->type) || options.stackAuto))
    {
      if (options.useXstack)
	{
	  emitcode ("mov", "r0,%s", spname);
	  emitcode ("movx", "a,@r0");
	  emitcode ("mov", "_bp,a");
	  emitcode ("dec", "%s", spname);
	}
      else
	{
	  emitcode ("pop", "_bp");
	}
    }

  /* restore the register bank  */
  if ( /* FUNC_REGBANK (sym->type) || */ IFFUNC_ISISR (sym->type))
  {
    if (/* !FUNC_REGBANK (sym->type) || */ !IFFUNC_ISISR (sym->type)
     || !options.useXstack)
    {
        /* Special case of ISR using non-zero bank with useXstack
         * is handled below.
         */
        emitcode ("pop", "psw");
    }
  }

  if (IFFUNC_ISISR (sym->type))
    {

      /* now we need to restore the registers */
      /* if this isr has no bank i.e. is going to
         run with bank 0 , then we need to save more
         registers :-) */
      if (!FUNC_REGBANK (sym->type))
	{
	  /* if this function does not call any other
	     function then we can be economical and
	     save only those registers that are used */
	  if (!IFFUNC_HASFCALL(sym->type))
	    {
	      int i;

	      /* if any registers used */
	      if (sym->regsUsed)
		{
		  /* save the registers used */
		  for (i = sym->regsUsed->size; i >= 0; i--)
		    {
		      if (bitVectBitValue (sym->regsUsed, i) ||
			  (mcs51_ptrRegReq && (i == R0_IDX || i == R1_IDX)))
			emitcode ("pop", "%s", mcs51_regWithIdx (i)->dname);
		    }
		}

	    }
	  else
	    {
	      if (options.parms_in_bank1) {
		  int i;
		  for (i = 7 ; i >= 0 ; i-- ) {
		      emitcode ("pop","%s",rb1regs[i]);
		  }
	      }
	      /* this function has  a function call cannot
	         determines register usage so we will have to pop the
	         entire bank */
	      unsaveRBank (0, ic, FALSE);
	    }
	}
	else
	{
	    /* This ISR uses a non-zero bank.
	     *
	     * Restore any register banks saved by genFunction
	     * in reverse order.
	     */
	    unsigned savedBanks = SPEC_ISR_SAVED_BANKS(currFunc->etype);
	    int ix;

	    for (ix = MAX_REGISTER_BANKS - 1; ix >= 0; ix--)
	    {
	        if (savedBanks & (1 << ix))
	        {
	            unsaveRBank(ix, NULL, FALSE);
	        }
	    }

	    if (options.useXstack)
	    {
	    	/* Restore bank AFTER calling unsaveRBank,
	     	 * since it can trash r0.
	     	 */
	    	emitcode ("pop", "psw");
	    }
	}

      if (!inExcludeList ("dph"))
	emitcode ("pop", "dph");
      if (!inExcludeList ("dpl"))
	emitcode ("pop", "dpl");
      if (!inExcludeList ("b"))
	emitcode ("pop", "b");
      if (!inExcludeList ("acc"))
	emitcode ("pop", "acc");

      /* if debug then send end of function */
      if (options.debug && currFunc)
	{
	  _G.debugLine = 1;
	  emitcode ("", "C$%s$%d$%d$%d ==.",
		    FileBaseName (ic->filename), currFunc->lastLine,
		    ic->level, ic->block);
	  if (IS_STATIC (currFunc->etype))
	    emitcode ("", "XF%s$%s$0$0 ==.", moduleName, currFunc->name);
	  else
	    emitcode ("", "XG$%s$0$0 ==.", currFunc->name);
	  _G.debugLine = 0;
	}

      emitcode ("reti", "");
    }
  else
    {
      if (IFFUNC_CALLEESAVES(sym->type))
	{
	  int i;

	  /* if any registers used */
	  if (sym->regsUsed)
	    {
	      /* save the registers used */
	      for (i = sym->regsUsed->size; i >= 0; i--)
		{
		  if (bitVectBitValue (sym->regsUsed, i) ||
		      (mcs51_ptrRegReq && (i == R0_IDX || i == R1_IDX)))
		    emitcode ("pop", "%s", mcs51_regWithIdx (i)->dname);
		}
	    }

	}

      /* if debug then send end of function */
      if (options.debug && currFunc)
	{
	  _G.debugLine = 1;
	  emitcode ("", "C$%s$%d$%d$%d ==.",
		    FileBaseName (ic->filename), currFunc->lastLine,
		    ic->level, ic->block);
	  if (IS_STATIC (currFunc->etype))
	    emitcode ("", "XF%s$%s$0$0 ==.", moduleName, currFunc->name);
	  else
	    emitcode ("", "XG$%s$0$0 ==.", currFunc->name);
	  _G.debugLine = 0;
	}

      emitcode ("ret", "");
    }

}

/*-----------------------------------------------------------------*/
/* genRet - generate code for return statement                     */
/*-----------------------------------------------------------------*/
static void
genRet (iCode * ic)
{
  int size, offset = 0, pushed = 0;

  D(emitcode (";     genRet",""));

  /* if we have no return value then
     just generate the "ret" */
  if (!IC_LEFT (ic))
    goto jumpret;

  /* we have something to return then
     move the return value into place */
  aopOp (IC_LEFT (ic), ic, FALSE);
  size = AOP_SIZE (IC_LEFT (ic));

  while (size--)
    {
      char *l;
      if (AOP_TYPE (IC_LEFT (ic)) == AOP_DPTR)
	{
	  /* #NOCHANGE */
	  l = aopGet (AOP (IC_LEFT (ic)), offset++,
		      FALSE, TRUE);
	  emitcode ("push", "%s", l);
	  pushed++;
	}
      else
	{
	  l = aopGet (AOP (IC_LEFT (ic)), offset,
		      FALSE, FALSE);
	  if (strcmp (fReturn[offset], l))
	    emitcode ("mov", "%s,%s", fReturn[offset++], l);
	}
    }

  if (pushed)
    {
      while (pushed)
	{
	  pushed--;
	  if (strcmp (fReturn[pushed], "a"))
	    emitcode ("pop", fReturn[pushed]);
	  else
	    emitcode ("pop", "acc");
	}
    }
  freeAsmop (IC_LEFT (ic), NULL, ic, TRUE);

jumpret:
  /* generate a jump to the return label
     if the next is not the return statement */
  if (!(ic->next && ic->next->op == LABEL &&
	IC_LABEL (ic->next) == returnLabel))

    emitcode ("ljmp", "%05d$", (returnLabel->key + 100));

}

/*-----------------------------------------------------------------*/
/* genLabel - generates a label                                    */
/*-----------------------------------------------------------------*/
static void
genLabel (iCode * ic)
{
  /* special case never generate */
  if (IC_LABEL (ic) == entryLabel)
    return;

  emitcode ("", "%05d$:", (IC_LABEL (ic)->key + 100));
}

/*-----------------------------------------------------------------*/
/* genGoto - generates a ljmp                                      */
/*-----------------------------------------------------------------*/
static void
genGoto (iCode * ic)
{
  emitcode ("ljmp", "%05d$", (IC_LABEL (ic)->key + 100));
}

/*-----------------------------------------------------------------*/
/* findLabelBackwards: walks back through the iCode chain looking  */
/* for the given label. Returns number of iCode instructions     */
/* between that label and given ic.          */
/* Returns zero if label not found.          */
/*-----------------------------------------------------------------*/
static int
findLabelBackwards (iCode * ic, int key)
{
  int count = 0;

  while (ic->prev)
    {
      ic = ic->prev;
      count++;

      /* If we have any pushes or pops, we cannot predict the distance.
	 I don't like this at all, this should be dealt with in the
	 back-end */
      if (ic->op == IPUSH || ic->op == IPOP) {
	return 0;
      }

      if (ic->op == LABEL && IC_LABEL (ic)->key == key)
	{
	  return count;
	}
    }

  return 0;
}

/*-----------------------------------------------------------------*/
/* genPlusIncr :- does addition with increment if possible         */
/*-----------------------------------------------------------------*/
static bool
genPlusIncr (iCode * ic)
{
  unsigned int icount;
  unsigned int size = getDataSize (IC_RESULT (ic));

  /* will try to generate an increment */
  /* if the right side is not a literal
     we cannot */
  if (AOP_TYPE (IC_RIGHT (ic)) != AOP_LIT)
    return FALSE;

  /* if the literal value of the right hand side
     is greater than 4 then it is not worth it */
  if ((icount = (unsigned int) floatFromVal (AOP (IC_RIGHT (ic))->aopu.aop_lit)) > 4)
    return FALSE;

  D(emitcode (";     genPlusIncr",""));

  /* if increment >=16 bits in register or direct space */
  if ((AOP_TYPE(IC_LEFT(ic)) == AOP_REG || AOP_TYPE(IC_LEFT(ic)) == AOP_DIR ) &&
      sameRegs (AOP (IC_LEFT (ic)), AOP (IC_RESULT (ic))) &&
      (size > 1) &&
      (icount == 1))
    {
      symbol *tlbl;
      int emitTlbl;
      int labelRange;

      /* If the next instruction is a goto and the goto target
       * is < 10 instructions previous to this, we can generate
       * jumps straight to that target.
       */
      if (ic->next && ic->next->op == GOTO
	  && (labelRange = findLabelBackwards (ic, IC_LABEL (ic->next)->key)) != 0
	  && labelRange <= 10)
	{
	  emitcode (";", "tail increment optimized");
	  tlbl = IC_LABEL (ic->next);
	  emitTlbl = 0;
	}
      else
	{
	  tlbl = newiTempLabel (NULL);
	  emitTlbl = 1;
	}
      emitcode ("inc", "%s", aopGet (AOP (IC_RESULT (ic)), LSB, FALSE, FALSE));
      if (AOP_TYPE (IC_RESULT (ic)) == AOP_REG ||
	  IS_AOP_PREG (IC_RESULT (ic)))
	emitcode ("cjne", "%s,#0x00,%05d$",
		  aopGet (AOP (IC_RESULT (ic)), LSB, FALSE, FALSE),
		  tlbl->key + 100);
      else
	{
	  emitcode ("clr", "a");
	  emitcode ("cjne", "a,%s,%05d$",
		    aopGet (AOP (IC_RESULT (ic)), LSB, FALSE, FALSE),
		    tlbl->key + 100);
	}

      emitcode ("inc", "%s", aopGet (AOP (IC_RESULT (ic)), MSB16, FALSE, FALSE));
      if (size > 2)
	{
	  if (AOP_TYPE (IC_RESULT (ic)) == AOP_REG ||
	      IS_AOP_PREG (IC_RESULT (ic)))
	    emitcode ("cjne", "%s,#0x00,%05d$",
		      aopGet (AOP (IC_RESULT (ic)), MSB16, FALSE, FALSE),
		      tlbl->key + 100);
	  else
	    emitcode ("cjne", "a,%s,%05d$",
		      aopGet (AOP (IC_RESULT (ic)), MSB16, FALSE, FALSE),
		      tlbl->key + 100);

	  emitcode ("inc", "%s", aopGet (AOP (IC_RESULT (ic)), MSB24, FALSE, FALSE));
	}
      if (size > 3)
	{
	  if (AOP_TYPE (IC_RESULT (ic)) == AOP_REG ||
	      IS_AOP_PREG (IC_RESULT (ic)))
	    emitcode ("cjne", "%s,#0x00,%05d$",
		      aopGet (AOP (IC_RESULT (ic)), MSB24, FALSE, FALSE),
		      tlbl->key + 100);
	  else
	    {
	      emitcode ("cjne", "a,%s,%05d$",
			aopGet (AOP (IC_RESULT (ic)), MSB24, FALSE, FALSE),
			tlbl->key + 100);
	    }
	  emitcode ("inc", "%s", aopGet (AOP (IC_RESULT (ic)), MSB32, FALSE, FALSE));
	}

      if (emitTlbl)
	{
	  emitcode ("", "%05d$:", tlbl->key + 100);
	}
      return TRUE;
    }

  /* if the sizes are greater than 1 then we cannot */
  if (AOP_SIZE (IC_RESULT (ic)) > 1 ||
      AOP_SIZE (IC_LEFT (ic)) > 1)
    return FALSE;

  /* we can if the aops of the left & result match or
     if they are in registers and the registers are the
     same */
  if (sameRegs (AOP (IC_LEFT (ic)), AOP (IC_RESULT (ic))))
    {

      if (icount > 3)
	{
	  MOVA (aopGet (AOP (IC_LEFT (ic)), 0, FALSE, FALSE));
	  emitcode ("add", "a,#0x%02x", ((char) icount) & 0xff);
	  aopPut (AOP (IC_RESULT (ic)), "a", 0, isOperandVolatile (IC_RESULT (ic), FALSE));
	}
      else
	{

	  while (icount--)
	    emitcode ("inc", "%s", aopGet (AOP (IC_LEFT (ic)), 0, FALSE, FALSE));
	}

      return TRUE;
    }

  return FALSE;
}

/*-----------------------------------------------------------------*/
/* outBitAcc - output a bit in acc                                 */
/*-----------------------------------------------------------------*/
static void
outBitAcc (operand * result)
{
  symbol *tlbl = newiTempLabel (NULL);
  /* if the result is a bit */
  if (AOP_TYPE (result) == AOP_CRY)
    {
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
    }
  else
    {
      emitcode ("jz", "%05d$", tlbl->key + 100);
      emitcode ("mov", "a,%s", one);
      emitcode ("", "%05d$:", tlbl->key + 100);
      outAcc (result);
    }
}

/*-----------------------------------------------------------------*/
/* genPlusBits - generates code for addition of two bits           */
/*-----------------------------------------------------------------*/
static void
genPlusBits (iCode * ic)
{
  D(emitcode (";     genPlusBits",""));

  if (AOP_TYPE (IC_RESULT (ic)) == AOP_CRY)
    {
      symbol *lbl = newiTempLabel (NULL);
      emitcode ("mov", "c,%s", AOP (IC_LEFT (ic))->aopu.aop_dir);
      emitcode ("jnb", "%s,%05d$", AOP (IC_RIGHT (ic))->aopu.aop_dir, (lbl->key + 100));
      emitcode ("cpl", "c");
      emitcode ("", "%05d$:", (lbl->key + 100));
      outBitC (IC_RESULT (ic));
    }
  else
    {
      emitcode ("clr", "a");
      emitcode ("mov", "c,%s", AOP (IC_LEFT (ic))->aopu.aop_dir);
      emitcode ("rlc", "a");
      emitcode ("mov", "c,%s", AOP (IC_RIGHT (ic))->aopu.aop_dir);
      emitcode ("addc", "a,#0x00");
      outAcc (IC_RESULT (ic));
    }
}

#if 0
/* This is the original version of this code.

 * This is being kept around for reference,
 * because I am not entirely sure I got it right...
 */
static void
adjustArithmeticResult (iCode * ic)
{
  if (AOP_SIZE (IC_RESULT (ic)) == 3 &&
      AOP_SIZE (IC_LEFT (ic)) == 3 &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_LEFT (ic))))
    aopPut (AOP (IC_RESULT (ic)),
	    aopGet (AOP (IC_LEFT (ic)), 2, FALSE, FALSE),
	    2,
	    isOperandVolatile (IC_RESULT (ic), FALSE));

  if (AOP_SIZE (IC_RESULT (ic)) == 3 &&
      AOP_SIZE (IC_RIGHT (ic)) == 3 &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_RIGHT (ic))))
    aopPut (AOP (IC_RESULT (ic)),
	    aopGet (AOP (IC_RIGHT (ic)), 2, FALSE, FALSE),
	    2,
	    isOperandVolatile (IC_RESULT (ic), FALSE));

  if (AOP_SIZE (IC_RESULT (ic)) == 3 &&
      AOP_SIZE (IC_LEFT (ic)) < 3 &&
      AOP_SIZE (IC_RIGHT (ic)) < 3 &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_LEFT (ic))) &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_RIGHT (ic))))
    {
      char buffer[5];
      sprintf (buffer, "#%d", pointerCode (getSpec (operandType (IC_LEFT (ic)))));
      aopPut (AOP (IC_RESULT (ic)), buffer, 2, isOperandVolatile (IC_RESULT (ic), FALSE));
    }
}
#else
/* This is the pure and virtuous version of this code.
 * I'm pretty certain it's right, but not enough to toss the old
 * code just yet...
 */
static void
adjustArithmeticResult (iCode * ic)
{
  if (opIsGptr (IC_RESULT (ic)) &&
      opIsGptr (IC_LEFT (ic)) &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_LEFT (ic))))
    {
      aopPut (AOP (IC_RESULT (ic)),
	      aopGet (AOP (IC_LEFT (ic)), GPTRSIZE - 1, FALSE, FALSE),
	      GPTRSIZE - 1,
	      isOperandVolatile (IC_RESULT (ic), FALSE));
    }

  if (opIsGptr (IC_RESULT (ic)) &&
      opIsGptr (IC_RIGHT (ic)) &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_RIGHT (ic))))
    {
      aopPut (AOP (IC_RESULT (ic)),
	      aopGet (AOP (IC_RIGHT (ic)), GPTRSIZE - 1, FALSE, FALSE),
	      GPTRSIZE - 1,
	      isOperandVolatile (IC_RESULT (ic), FALSE));
    }

  if (opIsGptr (IC_RESULT (ic)) &&
      AOP_SIZE (IC_LEFT (ic)) < GPTRSIZE &&
      AOP_SIZE (IC_RIGHT (ic)) < GPTRSIZE &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_LEFT (ic))) &&
      !sameRegs (AOP (IC_RESULT (ic)), AOP (IC_RIGHT (ic))))
    {
      char buffer[5];
      sprintf (buffer, "#%d", pointerCode (getSpec (operandType (IC_LEFT (ic)))));
      aopPut (AOP (IC_RESULT (ic)), buffer, GPTRSIZE - 1, isOperandVolatile (IC_RESULT (ic), FALSE));
    }
}
#endif

/*-----------------------------------------------------------------*/
/* genPlus - generates code for addition                           */
/*-----------------------------------------------------------------*/
static void
genPlus (iCode * ic)
{
  int size, offset = 0;
  int skip_bytes = 0;
  char *add = "add";
  asmop *leftOp, *rightOp;
  operand * op;

  /* special cases :- */

  D(emitcode (";     genPlus",""));

  aopOp (IC_LEFT (ic), ic, FALSE);
  aopOp (IC_RIGHT (ic), ic, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE);

  /* if literal, literal on the right or
     if left requires ACC or right is already
     in ACC */
  if ((AOP_TYPE (IC_LEFT (ic)) == AOP_LIT) ||
      (AOP_NEEDSACC (IC_LEFT (ic))) ||
      AOP_TYPE (IC_RIGHT (ic)) == AOP_ACC)
    {
      operand *t = IC_RIGHT (ic);
      IC_RIGHT (ic) = IC_LEFT (ic);
      IC_LEFT (ic) = t;
    }

  /* if both left & right are in bit
     space */
  if (AOP_TYPE (IC_LEFT (ic)) == AOP_CRY &&
      AOP_TYPE (IC_RIGHT (ic)) == AOP_CRY)
    {
      genPlusBits (ic);
      goto release;
    }

  /* if left in bit space & right literal */
  if (AOP_TYPE (IC_LEFT (ic)) == AOP_CRY &&
      AOP_TYPE (IC_RIGHT (ic)) == AOP_LIT)
    {
      emitcode ("mov", "c,%s", AOP (IC_LEFT (ic))->aopu.aop_dir);
      /* if result in bit space */
      if (AOP_TYPE (IC_RESULT (ic)) == AOP_CRY)
	{
	  if ((unsigned long) floatFromVal (AOP (IC_RIGHT (ic))->aopu.aop_lit) != 0L)
	    emitcode ("cpl", "c");
	  outBitC (IC_RESULT (ic));
	}
      else
	{
	  size = getDataSize (IC_RESULT (ic));
	  while (size--)
	    {
	      MOVA (aopGet (AOP (IC_RIGHT (ic)), offset, FALSE, FALSE));
	      emitcode ("addc", "a,#00");
	      aopPut (AOP (IC_RESULT (ic)), "a", offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
	    }
	}
      goto release;
    }

  /* if I can do an increment instead
     of add then GOOD for ME */
  if (genPlusIncr (ic) == TRUE)
    goto release;

  size = getDataSize (IC_RESULT (ic));
  leftOp = AOP(IC_LEFT(ic));
  rightOp = AOP(IC_RIGHT(ic));
  op=IC_LEFT(ic);

  /* if this is an add for an array access
     at a 256 byte boundary */
  if ( 2 == size
       && AOP_TYPE (op) == AOP_IMMD
       && IS_SYMOP (op)
       && IS_SPEC (OP_SYM_ETYPE (op))
       && SPEC_ABSA (OP_SYM_ETYPE (op))
       && (SPEC_ADDR (OP_SYM_ETYPE (op)) & 0xff) == 0
     )
    {
      D(emitcode (";     genPlus aligned array",""));
      aopPut (AOP (IC_RESULT (ic)),
 	      aopGet (rightOp, 0, FALSE, FALSE),
 	      0,
 	      isOperandVolatile (IC_RESULT (ic), FALSE));

      if( 1 == getDataSize (IC_RIGHT (ic)) )
	{
	  aopPut (AOP (IC_RESULT (ic)),
		  aopGet (leftOp, 1, FALSE, FALSE),
		  1,
		  isOperandVolatile (IC_RESULT (ic), FALSE));
	}
      else
        {
	  MOVA (aopGet (AOP (IC_LEFT (ic)), 1, FALSE, FALSE));
	  emitcode ("add", "a,%s", aopGet (rightOp, 1, FALSE, FALSE));
	  aopPut (AOP (IC_RESULT (ic)), "a", 1, isOperandVolatile (IC_RESULT (ic), FALSE));
	}
      goto release;
    }

  /* if the lower bytes of a literal are zero skip the addition */
  if (AOP_TYPE (IC_RIGHT (ic)) == AOP_LIT )
    {
       while ((0 == ((unsigned int) floatFromVal (AOP (IC_RIGHT (ic))->aopu.aop_lit) & (0xff << skip_bytes*8))) &&
              (skip_bytes+1 < size))
         {
           skip_bytes++;
	 }
       if (skip_bytes)
         D(emitcode (";     genPlus shortcut",""));
    }

  while (size--)
    {
      if( offset >= skip_bytes )
        {
	  if (aopGetUsesAcc (leftOp, offset) && aopGetUsesAcc (rightOp, offset))
	    {
	      emitcode("mov", "b,a");
	      MOVA (aopGet (leftOp,  offset, FALSE, TRUE));
	      emitcode("xch", "a,b");
	      MOVA (aopGet (rightOp, offset, FALSE, TRUE));
	      emitcode (add, "a,b");
	    }
	  else if (aopGetUsesAcc (leftOp, offset))
	    {
	      MOVA (aopGet (leftOp, offset, FALSE, TRUE));
	      emitcode (add, "a,%s", aopGet (rightOp, offset, FALSE, TRUE));
	    }
	  else
	    {
	      MOVA (aopGet (rightOp, offset, FALSE, TRUE));
	      emitcode (add, "a,%s", aopGet (leftOp, offset, FALSE, TRUE));
	    }
	  aopPut (AOP (IC_RESULT (ic)), "a", offset, isOperandVolatile (IC_RESULT (ic), FALSE));
	  add = "addc";  /* further adds must propagate carry */
        }
      else
        {
          if( !sameRegs (AOP (IC_LEFT (ic)), AOP (IC_RESULT (ic))) ||
	      isOperandVolatile (IC_RESULT (ic), FALSE))
	    {
	      /* just move */
              aopPut (AOP (IC_RESULT (ic)),
		      aopGet (leftOp, offset, FALSE, FALSE),
		      offset,
		      isOperandVolatile (IC_RESULT (ic), FALSE));
	    }
	}
      offset++;
    }

  adjustArithmeticResult (ic);

release:
  freeAsmop (IC_LEFT (ic), NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (IC_RIGHT (ic), NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genMinusDec :- does subtraction with deccrement if possible     */
/*-----------------------------------------------------------------*/
static bool
genMinusDec (iCode * ic)
{
  unsigned int icount;
  unsigned int size = getDataSize (IC_RESULT (ic));

  /* will try to generate an increment */
  /* if the right side is not a literal
     we cannot */
  if (AOP_TYPE (IC_RIGHT (ic)) != AOP_LIT)
    return FALSE;

  /* if the literal value of the right hand side
     is greater than 4 then it is not worth it */
  if ((icount = (unsigned int) floatFromVal (AOP (IC_RIGHT (ic))->aopu.aop_lit)) > 4)
    return FALSE;

  D(emitcode (";     genMinusDec",""));

  /* if decrement >=16 bits in register or direct space */
  if ((AOP_TYPE(IC_LEFT(ic)) == AOP_REG || AOP_TYPE(IC_LEFT(ic)) == AOP_DIR) &&
      sameRegs (AOP (IC_LEFT (ic)), AOP (IC_RESULT (ic))) &&
      (size > 1) &&
      (icount == 1))
    {
      symbol *tlbl;
      int emitTlbl;
      int labelRange;

      /* If the next instruction is a goto and the goto target
       * is <= 10 instructions previous to this, we can generate
       * jumps straight to that target.
       */
      if (ic->next && ic->next->op == GOTO
	  && (labelRange = findLabelBackwards (ic, IC_LABEL (ic->next)->key)) != 0
	  && labelRange <= 10)
	{
	  emitcode (";", "tail decrement optimized");
	  tlbl = IC_LABEL (ic->next);
	  emitTlbl = 0;
	}
      else
	{
	  tlbl = newiTempLabel (NULL);
	  emitTlbl = 1;
	}

      emitcode ("dec", "%s", aopGet (AOP (IC_RESULT (ic)), LSB, FALSE, FALSE));
      if (AOP_TYPE (IC_RESULT (ic)) == AOP_REG ||
	  IS_AOP_PREG (IC_RESULT (ic)))
	emitcode ("cjne", "%s,#0xff,%05d$"
		  ,aopGet (AOP (IC_RESULT (ic)), LSB, FALSE, FALSE)
		  ,tlbl->key + 100);
      else
	{
	  emitcode ("mov", "a,#0xff");
	  emitcode ("cjne", "a,%s,%05d$"
		    ,aopGet (AOP (IC_RESULT (ic)), LSB, FALSE, FALSE)
		    ,tlbl->key + 100);
	}
      emitcode ("dec", "%s", aopGet (AOP (IC_RESULT (ic)), MSB16, FALSE, FALSE));
      if (size > 2)
	{
	  if (AOP_TYPE (IC_RESULT (ic)) == AOP_REG ||
	      IS_AOP_PREG (IC_RESULT (ic)))
	    emitcode ("cjne", "%s,#0xff,%05d$"
		      ,aopGet (AOP (IC_RESULT (ic)), MSB16, FALSE, FALSE)
		      ,tlbl->key + 100);
	  else
	    {
	      emitcode ("cjne", "a,%s,%05d$"
			,aopGet (AOP (IC_RESULT (ic)), MSB16, FALSE, FALSE)
			,tlbl->key + 100);
	    }
	  emitcode ("dec", "%s", aopGet (AOP (IC_RESULT (ic)), MSB24, FALSE, FALSE));
	}
      if (size > 3)
	{
	  if (AOP_TYPE (IC_RESULT (ic)) == AOP_REG ||
	      IS_AOP_PREG (IC_RESULT (ic)))
	    emitcode ("cjne", "%s,#0xff,%05d$"
		      ,aopGet (AOP (IC_RESULT (ic)), MSB24, FALSE, FALSE)
		      ,tlbl->key + 100);
	  else
	    {
	      emitcode ("cjne", "a,%s,%05d$"
			,aopGet (AOP (IC_RESULT (ic)), MSB24, FALSE, FALSE)
			,tlbl->key + 100);
	    }
	  emitcode ("dec", "%s", aopGet (AOP (IC_RESULT (ic)), MSB32, FALSE, FALSE));
	}
      if (emitTlbl)
	{
	  emitcode ("", "%05d$:", tlbl->key + 100);
	}
      return TRUE;
    }

  /* if the sizes are greater than 1 then we cannot */
  if (AOP_SIZE (IC_RESULT (ic)) > 1 ||
      AOP_SIZE (IC_LEFT (ic)) > 1)
    return FALSE;

  /* we can if the aops of the left & result match or
     if they are in registers and the registers are the
     same */
  if (sameRegs (AOP (IC_LEFT (ic)), AOP (IC_RESULT (ic))))
    {

      while (icount--)
	emitcode ("dec", "%s", aopGet (AOP (IC_RESULT (ic)), 0, FALSE, FALSE));

      return TRUE;
    }

  return FALSE;
}

/*-----------------------------------------------------------------*/
/* addSign - complete with sign                                    */
/*-----------------------------------------------------------------*/
static void
addSign (operand * result, int offset, int sign)
{
  int size = (getDataSize (result) - offset);
  if (size > 0)
    {
      if (sign)
	{
	  emitcode ("rlc", "a");
	  emitcode ("subb", "a,acc");
	  while (size--)
	    aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
	}
      else
	while (size--)
	  aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
    }
}

/*-----------------------------------------------------------------*/
/* genMinusBits - generates code for subtraction  of two bits      */
/*-----------------------------------------------------------------*/
static void
genMinusBits (iCode * ic)
{
  symbol *lbl = newiTempLabel (NULL);

  D(emitcode (";     genMinusBits",""));

  if (AOP_TYPE (IC_RESULT (ic)) == AOP_CRY)
    {
      emitcode ("mov", "c,%s", AOP (IC_LEFT (ic))->aopu.aop_dir);
      emitcode ("jnb", "%s,%05d$", AOP (IC_RIGHT (ic))->aopu.aop_dir, (lbl->key + 100));
      emitcode ("cpl", "c");
      emitcode ("", "%05d$:", (lbl->key + 100));
      outBitC (IC_RESULT (ic));
    }
  else
    {
      emitcode ("mov", "c,%s", AOP (IC_RIGHT (ic))->aopu.aop_dir);
      emitcode ("subb", "a,acc");
      emitcode ("jnb", "%s,%05d$", AOP (IC_LEFT (ic))->aopu.aop_dir, (lbl->key + 100));
      emitcode ("inc", "a");
      emitcode ("", "%05d$:", (lbl->key + 100));
      aopPut (AOP (IC_RESULT (ic)), "a", 0, isOperandVolatile (IC_RESULT (ic), FALSE));
      addSign (IC_RESULT (ic), MSB16, SPEC_USIGN (getSpec (operandType (IC_RESULT (ic)))));
    }
}

/*-----------------------------------------------------------------*/
/* genMinus - generates code for subtraction                       */
/*-----------------------------------------------------------------*/
static void
genMinus (iCode * ic)
{
  int size, offset = 0;

  D(emitcode (";     genMinus",""));

  aopOp (IC_LEFT (ic), ic, FALSE);
  aopOp (IC_RIGHT (ic), ic, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE);

  /* special cases :- */
  /* if both left & right are in bit space */
  if (AOP_TYPE (IC_LEFT (ic)) == AOP_CRY &&
      AOP_TYPE (IC_RIGHT (ic)) == AOP_CRY)
    {
      genMinusBits (ic);
      goto release;
    }

  /* if I can do an decrement instead
     of subtract then GOOD for ME */
  if (genMinusDec (ic) == TRUE)
    goto release;

  size = getDataSize (IC_RESULT (ic));

  /* if literal, add a,#-lit, else normal subb */
  if (AOP_TYPE (IC_RIGHT (ic)) == AOP_LIT)
    {
      unsigned long lit = 0L;

      lit = (unsigned long) floatFromVal (AOP (IC_RIGHT (ic))->aopu.aop_lit);
      lit = -(long) lit;

      while (size--)
	{
	  MOVA (aopGet (AOP (IC_LEFT (ic)), offset, FALSE, FALSE));
	  /* first add without previous c */
	  if (!offset) {
	    if (!size && lit== (unsigned long) -1) {
	      emitcode ("dec", "a");
	    } else {
	      emitcode ("add", "a,#0x%02x",
			(unsigned int) (lit & 0x0FFL));
	    }
	  } else {
	    emitcode ("addc", "a,#0x%02x",
		      (unsigned int) ((lit >> (offset * 8)) & 0x0FFL));
	  }
	  aopPut (AOP (IC_RESULT (ic)), "a", offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
	}
    }
  else
    {
      asmop *leftOp, *rightOp;

      leftOp = AOP(IC_LEFT(ic));
      rightOp = AOP(IC_RIGHT(ic));

      while (size--)
	{
	  if (aopGetUsesAcc(rightOp, offset)) {
	    wassertl(!aopGetUsesAcc(leftOp, offset), "accumulator clash");
	    MOVA (aopGet(rightOp, offset, FALSE, TRUE));
	    if (offset == 0) {
	      emitcode( "setb", "c");
	    }
	    emitcode("subb", "a,%s", aopGet(leftOp, offset, FALSE, TRUE));
	    emitcode("cpl", "a");
	  } else {
	    MOVA (aopGet (leftOp, offset, FALSE, FALSE));
	    if (offset == 0)
	      CLRC;
	    emitcode ("subb", "a,%s",
		      aopGet(rightOp, offset, FALSE, TRUE));
	  }

	  aopPut (AOP (IC_RESULT (ic)), "a", offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
	}
    }


  adjustArithmeticResult (ic);

release:
  freeAsmop (IC_LEFT (ic), NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (IC_RIGHT (ic), NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
}


/*-----------------------------------------------------------------*/
/* genMultbits :- multiplication of bits                           */
/*-----------------------------------------------------------------*/
static void
genMultbits (operand * left,
	     operand * right,
	     operand * result)
{
  D(emitcode (";     genMultbits",""));

  emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
  emitcode ("anl", "c,%s", AOP (right)->aopu.aop_dir);
  outBitC (result);
}

/*-----------------------------------------------------------------*/
/* genMultOneByte : 8*8=8/16 bit multiplication                    */
/*-----------------------------------------------------------------*/
static void
genMultOneByte (operand * left,
		operand * right,
		operand * result)
{
  symbol *lbl;
  int size = AOP_SIZE (result);
  bool runtimeSign, compiletimeSign;
  bool lUnsigned, rUnsigned;

  D(emitcode (";     genMultOneByte",""));

  if (size < 1 || size > 2)
    {
      /* this should never happen */
      fprintf (stderr, "size!=1||2 (%d) in %s at line:%d \n",
	       AOP_SIZE(result), __FILE__, lineno);
      exit (1);
    }

  /* (if two literals: the value is computed before) */
  /* if one literal, literal on the right */
  if (AOP_TYPE (left) == AOP_LIT)
    {
      operand *t = right;
      right = left;
      left = t;
      /* emitcode (";", "swapped left and right"); */
    }
  /* if no literal, unsigned on the right: shorter code */
  if (   AOP_TYPE (right) != AOP_LIT
      && SPEC_USIGN (getSpec (operandType (left))))
    {
      operand *t = right;
      right = left;
      left = t;
    }

  lUnsigned = SPEC_USIGN (getSpec (operandType (left)));
  rUnsigned = SPEC_USIGN (getSpec (operandType (right)));

  if (size == 1 /* no, this is not a bug; with a 1 byte result there's
		   no need to take care about the signedness! */
      || (lUnsigned && rUnsigned))
    {
      /* just an unsigned 8 * 8 = 8 multiply
         or 8u * 8u = 16u */
      /* emitcode (";","unsigned"); */
      /* TODO: check for accumulator clash between left & right aops? */

      if (AOP_TYPE (right) == AOP_LIT)
        {
          /* moving to accumulator first helps peepholes */
          MOVA (aopGet (AOP (left), 0, FALSE, FALSE));
          emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
        }
      else
        {
          emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
          MOVA (aopGet (AOP (left), 0, FALSE, FALSE));
        }

      emitcode ("mul", "ab");
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      if (size == 2)
        aopPut (AOP (result), "b", 1, isOperandVolatile (result, FALSE));
      return;
    }

  /* we have to do a signed multiply */
  /* emitcode (";", "signed"); */

  /* now sign adjust for both left & right */

  /* let's see what's needed: */
  /* apply negative sign during runtime */
  runtimeSign = FALSE;
  /* negative sign from literals */
  compiletimeSign = FALSE;

  if (!lUnsigned)
    {
      if (AOP_TYPE(left) == AOP_LIT)
        {
          /* signed literal */
          signed char val = (char) floatFromVal (AOP (left)->aopu.aop_lit);
          if (val < 0)
            compiletimeSign = TRUE;
        }
      else
        /* signed but not literal */
        runtimeSign = TRUE;
    }

  if (!rUnsigned)
    {
      if (AOP_TYPE(right) == AOP_LIT)
        {
          /* signed literal */
          signed char val = (char) floatFromVal (AOP (right)->aopu.aop_lit);
          if (val < 0)
            compiletimeSign ^= TRUE;
        }
      else
        /* signed but not literal */
        runtimeSign = TRUE;
    }

  /* initialize F0, which stores the runtime sign */
  if (runtimeSign)
    {
      if (compiletimeSign)
	emitcode ("setb", "F0"); /* set sign flag */
      else
	emitcode ("clr", "F0"); /* reset sign flag */
    }

  /* save the signs of the operands */
  if (AOP_TYPE(right) == AOP_LIT)
    {
      signed char val = (char) floatFromVal (AOP (right)->aopu.aop_lit);

      if (!rUnsigned && val < 0)
        emitcode ("mov", "b,#0x%02x", -val);
      else
        emitcode ("mov", "b,#0x%02x", (unsigned char) val);
    }
  else /* ! literal */
    {
      if (rUnsigned)  /* emitcode (";", "signed"); */

        emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
      else
        {
	  MOVA (aopGet (AOP (right), 0, FALSE, FALSE));
	  lbl = newiTempLabel (NULL);
	  emitcode ("jnb", "acc.7,%05d$", (lbl->key + 100));
	  emitcode ("cpl", "F0"); /* complement sign flag */
	  emitcode ("cpl", "a");  /* 2's complement */
	  emitcode ("inc", "a");
	  emitcode ("", "%05d$:", (lbl->key + 100));
          emitcode ("mov", "b,a");
	}
    }

  if (AOP_TYPE(left) == AOP_LIT)
    {
      signed char val = (char) floatFromVal (AOP (left)->aopu.aop_lit);

      if (!lUnsigned && val < 0)
        emitcode ("mov", "a,#0x%02x", -val);
      else
        emitcode ("mov", "a,#0x%02x", (unsigned char) val);
    }
  else /* ! literal */
    {
      MOVA (aopGet (AOP (left), 0, FALSE, FALSE));

      if (!lUnsigned)
        {
          lbl = newiTempLabel (NULL);
          emitcode ("jnb", "acc.7,%05d$", (lbl->key + 100));
          emitcode ("cpl", "F0"); /* complement sign flag */
          emitcode ("cpl", "a"); /* 2's complement */
          emitcode ("inc", "a");
          emitcode ("", "%05d$:", (lbl->key + 100));
        }
    }

  /* now the multiplication */
  emitcode ("mul", "ab");
  if (runtimeSign || compiletimeSign)
    {
      lbl = newiTempLabel (NULL);
      if (runtimeSign)
        emitcode ("jnb", "F0,%05d$", (lbl->key + 100));
      emitcode ("cpl", "a"); /* lsb 2's complement */
      if (size != 2)
        emitcode ("inc", "a"); /* inc doesn't set carry flag */
      else
        {
          emitcode ("add", "a,#1"); /* this sets carry flag */
          emitcode ("xch", "a,b");
          emitcode ("cpl", "a"); /* msb 2's complement */
          emitcode ("addc", "a,#0");
          emitcode ("xch", "a,b");
        }
      emitcode ("", "%05d$:", (lbl->key + 100));
    }
  aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
  if (size == 2)
    aopPut (AOP (result), "b", 1, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* genMult - generates code for multiplication                     */
/*-----------------------------------------------------------------*/
static void
genMult (iCode * ic)
{
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  operand *result = IC_RESULT (ic);

  D(emitcode (";     genMult",""));

  /* assign the amsops */
  aopOp (left, ic, FALSE);
  aopOp (right, ic, FALSE);
  aopOp (result, ic, TRUE);

  /* special cases first */
  /* both are bits */
  if (AOP_TYPE (left) == AOP_CRY &&
      AOP_TYPE (right) == AOP_CRY)
    {
      genMultbits (left, right, result);
      goto release;
    }

  /* if both are of size == 1 */
#if 0 // one of them can be a sloc shared with the result
    if (AOP_SIZE (left) == 1 && AOP_SIZE (right) == 1)
#else
  if (getSize(operandType(left)) == 1 &&
      getSize(operandType(right)) == 1)
#endif
    {
      genMultOneByte (left, right, result);
      goto release;
    }

  /* should have been converted to function call */
    fprintf (stderr, "left: %d right: %d\n", getSize(OP_SYMBOL(left)->type),
	     getSize(OP_SYMBOL(right)->type));
  assert (0);

release:
  freeAsmop (result, NULL, ic, TRUE);
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
}

/*-----------------------------------------------------------------*/
/* genDivbits :- division of bits                                  */
/*-----------------------------------------------------------------*/
static void
genDivbits (operand * left,
	    operand * right,
	    operand * result)
{

  char *l;

  D(emitcode (";     genDivbits",""));

  /* the result must be bit */
  emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
  l = aopGet (AOP (left), 0, FALSE, FALSE);

  MOVA (l);

  emitcode ("div", "ab");
  emitcode ("rrc", "a");
  aopPut (AOP (result), "c", 0, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* genDivOneByte : 8 bit division                                  */
/*-----------------------------------------------------------------*/
static void
genDivOneByte (operand * left,
	       operand * right,
	       operand * result)
{
  bool lUnsigned, rUnsigned;
  bool runtimeSign, compiletimeSign;
  symbol *lbl;
  int size, offset;

  D(emitcode (";     genDivOneByte",""));

  /* Why is it necessary that genDivOneByte() can return an int result?
     Have a look at:
     
	volatile unsigned char uc;
	volatile signed char sc1, sc2;
	volatile int i;
     
	uc  = 255;
	sc1 = -1;
	i = uc / sc1;

     Or:
  
	sc1 = -128;
	sc2 = -1;
	i = sc1 / sc2;

     In all cases a one byte result would overflow, the following cast to int
     would return the wrong result.
  
     Two possible solution:
	a) cast operands to int, if ((unsigned) / (signed)) or
	   ((signed) / (signed))
	b) return an 16 bit signed int; this is what we're doing here!
  */
  
  size = AOP_SIZE (result) - 1;
  offset = 1;
  lUnsigned = SPEC_USIGN (getSpec (operandType (left)));
  rUnsigned = SPEC_USIGN (getSpec (operandType (right)));

  /* signed or unsigned */
  if (lUnsigned && rUnsigned)
    {
      /* unsigned is easy */
      emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
      MOVA (aopGet (AOP (left), 0, FALSE, FALSE));
      emitcode ("div", "ab");
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      while (size--)
	aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
      return;
    }
  
  /* signed is a little bit more difficult */

  /* now sign adjust for both left & right */

  /* let's see what's needed: */
  /* apply negative sign during runtime */
  runtimeSign = FALSE;
  /* negative sign from literals */
  compiletimeSign = FALSE;

  if (!lUnsigned)
    {
      if (AOP_TYPE(left) == AOP_LIT)
        {
          /* signed literal */
          signed char val = (char) floatFromVal (AOP (left)->aopu.aop_lit);
          if (val < 0)
            compiletimeSign = TRUE;
        }
      else
        /* signed but not literal */
        runtimeSign = TRUE;
    }

  if (!rUnsigned)
    {
      if (AOP_TYPE(right) == AOP_LIT)
        {
          /* signed literal */
          signed char val = (char) floatFromVal (AOP (right)->aopu.aop_lit);
          if (val < 0)
            compiletimeSign ^= TRUE;
        }
      else
        /* signed but not literal */
        runtimeSign = TRUE;
    }

  /* initialize F0, which stores the runtime sign */
  if (runtimeSign)
    {
      if (compiletimeSign)
	emitcode ("setb", "F0"); /* set sign flag */
      else
	emitcode ("clr", "F0"); /* reset sign flag */
    }

  /* save the signs of the operands */
  if (AOP_TYPE(right) == AOP_LIT)
    {
      signed char val = (char) floatFromVal (AOP (right)->aopu.aop_lit);

      if (!rUnsigned && val < 0)
        emitcode ("mov", "b,#0x%02x", -val);
      else
        emitcode ("mov", "b,#0x%02x", (unsigned char) val);
    }
  else /* ! literal */
    {
      if (rUnsigned)
        emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
      else
        {
	  MOVA (aopGet (AOP (right), 0, FALSE, FALSE));
	  lbl = newiTempLabel (NULL);
	  emitcode ("jnb", "acc.7,%05d$", (lbl->key + 100));
	  emitcode ("cpl", "F0"); /* complement sign flag */
	  emitcode ("cpl", "a");  /* 2's complement */
	  emitcode ("inc", "a");
	  emitcode ("", "%05d$:", (lbl->key + 100));
          emitcode ("mov", "b,a");
	}
    }

  if (AOP_TYPE(left) == AOP_LIT)
    {
      signed char val = (char) floatFromVal (AOP (left)->aopu.aop_lit);

      if (!lUnsigned && val < 0)
        emitcode ("mov", "a,#0x%02x", -val);
      else
        emitcode ("mov", "a,#0x%02x", (unsigned char) val);
    }
  else /* ! literal */
    {
      MOVA (aopGet (AOP (left), 0, FALSE, FALSE));

      if (!lUnsigned)
        {
          lbl = newiTempLabel (NULL);
          emitcode ("jnb", "acc.7,%05d$", (lbl->key + 100));
          emitcode ("cpl", "F0"); /* complement sign flag */
          emitcode ("cpl", "a");  /* 2's complement */
          emitcode ("inc", "a");
          emitcode ("", "%05d$:", (lbl->key + 100));
        }
    }

  /* now the division */
  emitcode ("div", "ab");

  if (runtimeSign || compiletimeSign)
    {
      lbl = newiTempLabel (NULL);
      if (runtimeSign)
        emitcode ("jnb", "F0,%05d$", (lbl->key + 100));
      emitcode ("cpl", "a"); /* lsb 2's complement */
      emitcode ("inc", "a");
      emitcode ("", "%05d$:", (lbl->key + 100));

      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      if (size > 0)
	{
	  /* msb is 0x00 or 0xff depending on the sign */
	  if (runtimeSign)
	    {
	      emitcode ("mov", "c,F0");
	      emitcode ("subb", "a,acc");
	      while (size--)
	        aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
	    }
	  else /* compiletimeSign */
	    while (size--)
	      aopPut (AOP (result), "#0xff", offset++, isOperandVolatile (result, FALSE));
	}
    }
  else
    {
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      while (size--)
	aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
    }
}

/*-----------------------------------------------------------------*/
/* genDiv - generates code for division                            */
/*-----------------------------------------------------------------*/
static void
genDiv (iCode * ic)
{
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  operand *result = IC_RESULT (ic);

  D(emitcode (";     genDiv",""));

  /* assign the amsops */
  aopOp (left, ic, FALSE);
  aopOp (right, ic, FALSE);
  aopOp (result, ic, TRUE);

  /* special cases first */
  /* both are bits */
  if (AOP_TYPE (left) == AOP_CRY &&
      AOP_TYPE (right) == AOP_CRY)
    {
      genDivbits (left, right, result);
      goto release;
    }

  /* if both are of size == 1 */
  if (AOP_SIZE (left) == 1 &&
      AOP_SIZE (right) == 1)
    {
      genDivOneByte (left, right, result);
      goto release;
    }

  /* should have been converted to function call */
  assert (0);
release:
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genModbits :- modulus of bits                                   */
/*-----------------------------------------------------------------*/
static void
genModbits (operand * left,
	    operand * right,
	    operand * result)
{

  char *l;

  D(emitcode (";     genModbits",""));

  /* the result must be bit */
  emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
  l = aopGet (AOP (left), 0, FALSE, FALSE);

  MOVA (l);

  emitcode ("div", "ab");
  emitcode ("mov", "a,b");
  emitcode ("rrc", "a");
  aopPut (AOP (result), "c", 0, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* genModOneByte : 8 bit modulus                                   */
/*-----------------------------------------------------------------*/
static void
genModOneByte (operand * left,
	       operand * right,
	       operand * result)
{
  bool lUnsigned, rUnsigned;
  bool runtimeSign, compiletimeSign;
  symbol *lbl;
  int size, offset;

  D(emitcode (";     genModOneByte",""));

  size = AOP_SIZE (result) - 1;
  offset = 1;
  lUnsigned = SPEC_USIGN (getSpec (operandType (left)));
  rUnsigned = SPEC_USIGN (getSpec (operandType (right)));
  
  /* signed or unsigned */
  if (lUnsigned && rUnsigned)
    {
      /* unsigned is easy */
      emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
      MOVA (aopGet (AOP (left), 0, FALSE, FALSE));
      emitcode ("div", "ab");
      aopPut (AOP (result), "b", 0, isOperandVolatile (result, FALSE));
      while (size--)
	aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
      return;
    }

  /* signed is a little bit more difficult */

  /* now sign adjust for both left & right */

  /* modulus: sign of the right operand has no influence on the result! */
  if (AOP_TYPE(right) == AOP_LIT)
    {
      signed char val = (char) floatFromVal (AOP (right)->aopu.aop_lit);

      if (!rUnsigned && val < 0)
        emitcode ("mov", "b,#0x%02x", -val);
      else
        emitcode ("mov", "b,#0x%02x", (unsigned char) val);
    }
  else /* not literal */
    {
      if (rUnsigned)
        emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
      else
        {
	  MOVA (aopGet (AOP (right), 0, FALSE, FALSE));
	  lbl = newiTempLabel (NULL);
	  emitcode ("jnb", "acc.7,%05d$", (lbl->key + 100));
	  emitcode ("cpl", "a"); /* 2's complement */
	  emitcode ("inc", "a");
	  emitcode ("", "%05d$:", (lbl->key + 100));
          emitcode ("mov", "b,a");
	}
    }

  /* let's see what's needed: */
  /* apply negative sign during runtime */
  runtimeSign = FALSE;
  /* negative sign from literals */
  compiletimeSign = FALSE;
  
  /* sign adjust left side */
  if (AOP_TYPE(left) == AOP_LIT)
    {
      signed char val = (char) floatFromVal (AOP (left)->aopu.aop_lit);

      if (!lUnsigned && val < 0)
	{
          compiletimeSign = TRUE; /* set sign flag */
          emitcode ("mov", "a,#0x%02x", -val);
        }
      else
        emitcode ("mov", "a,#0x%02x", (unsigned char) val);
    }
  else /* ! literal */
    {
      MOVA (aopGet (AOP (left), 0, FALSE, FALSE));

      if (!lUnsigned)
        {
          runtimeSign = TRUE;
          emitcode ("clr", "F0"); /* clear sign flag */

          lbl = newiTempLabel (NULL);
          emitcode ("jnb", "acc.7,%05d$", (lbl->key + 100));
          emitcode ("setb", "F0"); /* set sign flag */
          emitcode ("cpl", "a");   /* 2's complement */
          emitcode ("inc", "a");
          emitcode ("", "%05d$:", (lbl->key + 100));
        }
    }

  /* now the modulus */
  emitcode ("div", "ab");
  
  if (runtimeSign || compiletimeSign)
    {
      emitcode ("mov", "a,b");
      lbl = newiTempLabel (NULL);
      if (runtimeSign)
        emitcode ("jnb", "F0,%05d$", (lbl->key + 100));
      emitcode ("cpl", "a"); /* 2's complement */
      emitcode ("inc", "a");
      emitcode ("", "%05d$:", (lbl->key + 100));
     
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      if (size > 0)
	{
	  /* msb is 0x00 or 0xff depending on the sign */
	  if (runtimeSign)
	    {
	      emitcode ("mov", "c,F0");
	      emitcode ("subb", "a,acc");
	      while (size--)
	        aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
	    }
	  else /* compiletimeSign */
	    while (size--)
	      aopPut (AOP (result), "#0xff", offset++, isOperandVolatile (result, FALSE));
	}
    }
  else
    {
      aopPut (AOP (result), "b", 0, isOperandVolatile (result, FALSE));
      while (size--)
	aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
    }
}

/*-----------------------------------------------------------------*/
/* genMod - generates code for division                            */
/*-----------------------------------------------------------------*/
static void
genMod (iCode * ic)
{
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  operand *result = IC_RESULT (ic);

  D(emitcode (";     genMod",""));

  /* assign the amsops */
  aopOp (left, ic, FALSE);
  aopOp (right, ic, FALSE);
  aopOp (result, ic, TRUE);

  /* special cases first */
  /* both are bits */
  if (AOP_TYPE (left) == AOP_CRY &&
      AOP_TYPE (right) == AOP_CRY)
    {
      genModbits (left, right, result);
      goto release;
    }

  /* if both are of size == 1 */
  if (AOP_SIZE (left) == 1 &&
      AOP_SIZE (right) == 1)
    {
      genModOneByte (left, right, result);
      goto release;
    }

  /* should have been converted to function call */
  assert (0);

release:
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genIfxJump :- will create a jump depending on the ifx           */
/*-----------------------------------------------------------------*/
static void
genIfxJump (iCode * ic, char *jval)
{
  symbol *jlbl;
  symbol *tlbl = newiTempLabel (NULL);
  char *inst;

  D(emitcode (";     genIfxJump",""));

  /* if true label then we jump if condition
     supplied is true */
  if (IC_TRUE (ic))
    {
      jlbl = IC_TRUE (ic);
      inst = ((strcmp (jval, "a") == 0 ? "jz" :
	       (strcmp (jval, "c") == 0 ? "jnc" : "jnb")));
    }
  else
    {
      /* false label is present */
      jlbl = IC_FALSE (ic);
      inst = ((strcmp (jval, "a") == 0 ? "jnz" :
	       (strcmp (jval, "c") == 0 ? "jc" : "jb")));
    }
  if (strcmp (inst, "jb") == 0 || strcmp (inst, "jnb") == 0)
    emitcode (inst, "%s,%05d$", jval, (tlbl->key + 100));
  else
    emitcode (inst, "%05d$", tlbl->key + 100);
  emitcode ("ljmp", "%05d$", jlbl->key + 100);
  emitcode ("", "%05d$:", tlbl->key + 100);

  /* mark the icode as generated */
  ic->generated = 1;
}

/*-----------------------------------------------------------------*/
/* genCmp :- greater or less than comparison                       */
/*-----------------------------------------------------------------*/
static void
genCmp (operand * left, operand * right,
	operand * result, iCode * ifx, int sign, iCode *ic)
{
  int size, offset = 0;
  unsigned long lit = 0L;
  bool rightInB;

  D(emitcode (";     genCmp",""));

  /* if left & right are bit variables */
  if (AOP_TYPE (left) == AOP_CRY &&
      AOP_TYPE (right) == AOP_CRY)
    {
      emitcode ("mov", "c,%s", AOP (right)->aopu.aop_dir);
      emitcode ("anl", "c,%s", AOP (left)->aopu.aop_dir);
    }
  else
    {
      /* subtract right from left if at the
         end the carry flag is set then we know that
         left is greater than right */
      size = max (AOP_SIZE (left), AOP_SIZE (right));

      /* if unsigned char cmp with lit, do cjne left,#right,zz */
      if ((size == 1) && !sign &&
	  (AOP_TYPE (right) == AOP_LIT && AOP_TYPE (left) != AOP_DIR))
	{
	  symbol *lbl = newiTempLabel (NULL);
	  emitcode ("cjne", "%s,%s,%05d$",
		    aopGet (AOP (left), offset, FALSE, FALSE),
		    aopGet (AOP (right), offset, FALSE, FALSE),
		    lbl->key + 100);
	  emitcode ("", "%05d$:", lbl->key + 100);
	}
      else
	{
	  if (AOP_TYPE (right) == AOP_LIT)
	    {
	      lit = (unsigned long) floatFromVal (AOP (right)->aopu.aop_lit);
	      /* optimize if(x < 0) or if(x >= 0) */
	      if (lit == 0L)
		{
		  if (!sign)
		    {
		      CLRC;
		    }
		  else
		    {
		      MOVA (aopGet (AOP (left), AOP_SIZE (left) - 1, FALSE, FALSE));
		      if (!(AOP_TYPE (result) == AOP_CRY && AOP_SIZE (result)) && ifx)
			{
			  genIfxJump (ifx, "acc.7");
			  return;
			}
		      else
			emitcode ("rlc", "a");
		    }
		  goto release;
		}
	    }
	  CLRC;
	  while (size--)
	    {
	      rightInB = aopGetUsesAcc(AOP (right), offset);
	      if (rightInB)
		emitcode ("mov", "b,%s", aopGet (AOP (right), offset, FALSE, FALSE));
	      MOVA (aopGet (AOP (left), offset, FALSE, FALSE));
	      if (sign && size == 0)
		{
		  emitcode ("xrl", "a,#0x80");
		  if (AOP_TYPE (right) == AOP_LIT)
		    {
		      unsigned long lit = (unsigned long)
		      floatFromVal (AOP (right)->aopu.aop_lit);
		      emitcode ("subb", "a,#0x%02x",
				0x80 ^ (unsigned int) ((lit >> (offset * 8)) & 0x0FFL));
		    }
		  else
		    {
		      if (!rightInB)
		        emitcode ("mov", "b,%s", aopGet (AOP (right), offset, FALSE, FALSE));
		      emitcode ("xrl", "b,#0x80");
		      emitcode ("subb", "a,b");
		    }
		}
	      else
		{
		  if (rightInB)
		    emitcode ("subb", "a,b");
		  else
		    emitcode ("subb", "a,%s", aopGet (AOP (right), offset, FALSE, FALSE));
		}
	      offset++;
	    }
	}
    }

release:
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  if (AOP_TYPE (result) == AOP_CRY && AOP_SIZE (result))
    {
      outBitC (result);
    }
  else
    {
      /* if the result is used in the next
         ifx conditional branch then generate
         code a little differently */
      if (ifx)
	genIfxJump (ifx, "c");
      else
	outBitC (result);
      /* leave the result in acc */
    }
}

/*-----------------------------------------------------------------*/
/* genCmpGt :- greater than comparison                             */
/*-----------------------------------------------------------------*/
static void
genCmpGt (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  sym_link *letype, *retype;
  int sign;

  D(emitcode (";     genCmpGt",""));

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);

  letype = getSpec (operandType (left));
  retype = getSpec (operandType (right));
  sign = !((SPEC_USIGN (letype) && !(IS_CHAR (letype) && IS_LITERAL (letype))) ||
           (SPEC_USIGN (retype) && !(IS_CHAR (retype) && IS_LITERAL (retype))));
  /* assign the amsops */
  aopOp (left, ic, FALSE);
  aopOp (right, ic, FALSE);
  aopOp (result, ic, TRUE);

  genCmp (right, left, result, ifx, sign,ic);

  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genCmpLt - less than comparisons                                */
/*-----------------------------------------------------------------*/
static void
genCmpLt (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  sym_link *letype, *retype;
  int sign;

  D(emitcode (";     genCmpLt",""));

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);

  letype = getSpec (operandType (left));
  retype = getSpec (operandType (right));
  sign = !((SPEC_USIGN (letype) && !(IS_CHAR (letype) && IS_LITERAL (letype))) ||
           (SPEC_USIGN (retype) && !(IS_CHAR (retype) && IS_LITERAL (retype))));
  /* assign the amsops */
  aopOp (left, ic, FALSE);
  aopOp (right, ic, FALSE);
  aopOp (result, ic, TRUE);

  genCmp (left, right, result, ifx, sign,ic);

  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* gencjneshort - compare and jump if not equal                    */
/*-----------------------------------------------------------------*/
static void
gencjneshort (operand * left, operand * right, symbol * lbl)
{
  int size = max (AOP_SIZE (left), AOP_SIZE (right));
  int offset = 0;
  unsigned long lit = 0L;

  /* if the left side is a literal or
     if the right is in a pointer register and left
     is not */
  if ((AOP_TYPE (left) == AOP_LIT) ||
      (AOP_TYPE (left) == AOP_IMMD) ||
      (IS_AOP_PREG (right) && !IS_AOP_PREG (left)))
    {
      operand *t = right;
      right = left;
      left = t;
    }

  if (AOP_TYPE (right) == AOP_LIT)
    lit = (unsigned long) floatFromVal (AOP (right)->aopu.aop_lit);

  /* if the right side is a literal then anything goes */
  if (AOP_TYPE (right) == AOP_LIT &&
      AOP_TYPE (left) != AOP_DIR  &&
      AOP_TYPE (left) != AOP_IMMD)
    {
      while (size--)
	{
	  emitcode ("cjne", "%s,%s,%05d$",
		    aopGet (AOP (left), offset, FALSE, FALSE),
		    aopGet (AOP (right), offset, FALSE, FALSE),
		    lbl->key + 100);
	  offset++;
	}
    }

  /* if the right side is in a register or in direct space or
     if the left is a pointer register & right is not */
  else if (AOP_TYPE (right) == AOP_REG ||
	   AOP_TYPE (right) == AOP_DIR ||
	   AOP_TYPE (right) == AOP_LIT ||
           AOP_TYPE (right) == AOP_IMMD ||
	   (AOP_TYPE (left) == AOP_DIR && AOP_TYPE (right) == AOP_LIT) ||
	   (IS_AOP_PREG (left) && !IS_AOP_PREG (right)))
    {
      while (size--)
	{
	  MOVA (aopGet (AOP (left), offset, FALSE, FALSE));
	  if ((AOP_TYPE (left) == AOP_DIR && AOP_TYPE (right) == AOP_LIT) &&
	      ((unsigned int) ((lit >> (offset * 8)) & 0x0FFL) == 0))
	    emitcode ("jnz", "%05d$", lbl->key + 100);
	  else
	    emitcode ("cjne", "a,%s,%05d$",
		      aopGet (AOP (right), offset, FALSE, TRUE),
		      lbl->key + 100);
	  offset++;
	}
    }
  else
    {
      /* right is a pointer reg need both a & b */
      while (size--)
	{
	  char *l = aopGet (AOP (left), offset, FALSE, FALSE);
	  if (strcmp (l, "b"))
	    emitcode ("mov", "b,%s", l);
	  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
	  emitcode ("cjne", "a,b,%05d$", lbl->key + 100);
	  offset++;
	}
    }
}

/*-----------------------------------------------------------------*/
/* gencjne - compare and jump if not equal                         */
/*-----------------------------------------------------------------*/
static void
gencjne (operand * left, operand * right, symbol * lbl)
{
  symbol *tlbl = newiTempLabel (NULL);

  gencjneshort (left, right, lbl);

  emitcode ("mov", "a,%s", one);
  emitcode ("sjmp", "%05d$", tlbl->key + 100);
  emitcode ("", "%05d$:", lbl->key + 100);
  emitcode ("clr", "a");
  emitcode ("", "%05d$:", tlbl->key + 100);
}

/*-----------------------------------------------------------------*/
/* genCmpEq - generates code for equal to                          */
/*-----------------------------------------------------------------*/
static void
genCmpEq (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;

  D(emitcode (";     genCmpEq",""));

  aopOp ((left = IC_LEFT (ic)), ic, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, TRUE);

  /* if literal, literal on the right or
     if the right is in a pointer register and left
     is not */
  if ((AOP_TYPE (IC_LEFT (ic)) == AOP_LIT) ||
      (IS_AOP_PREG (right) && !IS_AOP_PREG (left)))
    {
      operand *t = IC_RIGHT (ic);
      IC_RIGHT (ic) = IC_LEFT (ic);
      IC_LEFT (ic) = t;
    }

  if (ifx && !AOP_SIZE (result))
    {
      symbol *tlbl;
      /* if they are both bit variables */
      if (AOP_TYPE (left) == AOP_CRY &&
	  ((AOP_TYPE (right) == AOP_CRY) || (AOP_TYPE (right) == AOP_LIT)))
	{
	  if (AOP_TYPE (right) == AOP_LIT)
	    {
	      unsigned long lit = (unsigned long) floatFromVal (AOP (IC_RIGHT (ic))->aopu.aop_lit);
	      if (lit == 0L)
		{
		  emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
		  emitcode ("cpl", "c");
		}
	      else if (lit == 1L)
		{
		  emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
		}
	      else
		{
		  emitcode ("clr", "c");
		}
	      /* AOP_TYPE(right) == AOP_CRY */
	    }
	  else
	    {
	      symbol *lbl = newiTempLabel (NULL);
	      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
	      emitcode ("jb", "%s,%05d$", AOP (right)->aopu.aop_dir, (lbl->key + 100));
	      emitcode ("cpl", "c");
	      emitcode ("", "%05d$:", (lbl->key + 100));
	    }
	  /* if true label then we jump if condition
	     supplied is true */
	  tlbl = newiTempLabel (NULL);
	  if (IC_TRUE (ifx))
	    {
	      emitcode ("jnc", "%05d$", tlbl->key + 100);
	      emitcode ("ljmp", "%05d$", IC_TRUE (ifx)->key + 100);
	    }
	  else
	    {
	      emitcode ("jc", "%05d$", tlbl->key + 100);
	      emitcode ("ljmp", "%05d$", IC_FALSE (ifx)->key + 100);
	    }
	  emitcode ("", "%05d$:", tlbl->key + 100);
	}
      else
	{
	  tlbl = newiTempLabel (NULL);
	  gencjneshort (left, right, tlbl);
	  if (IC_TRUE (ifx))
	    {
	      emitcode ("ljmp", "%05d$", IC_TRUE (ifx)->key + 100);
	      emitcode ("", "%05d$:", tlbl->key + 100);
	    }
	  else
	    {
	      symbol *lbl = newiTempLabel (NULL);
	      emitcode ("sjmp", "%05d$", lbl->key + 100);
	      emitcode ("", "%05d$:", tlbl->key + 100);
	      emitcode ("ljmp", "%05d$", IC_FALSE (ifx)->key + 100);
	      emitcode ("", "%05d$:", lbl->key + 100);
	    }
	}
      /* mark the icode as generated */
      ifx->generated = 1;
      goto release;
    }

  /* if they are both bit variables */
  if (AOP_TYPE (left) == AOP_CRY &&
      ((AOP_TYPE (right) == AOP_CRY) || (AOP_TYPE (right) == AOP_LIT)))
    {
      if (AOP_TYPE (right) == AOP_LIT)
	{
	  unsigned long lit = (unsigned long) floatFromVal (AOP (IC_RIGHT (ic))->aopu.aop_lit);
	  if (lit == 0L)
	    {
	      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
	      emitcode ("cpl", "c");
	    }
	  else if (lit == 1L)
	    {
	      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
	    }
	  else
	    {
	      emitcode ("clr", "c");
	    }
	  /* AOP_TYPE(right) == AOP_CRY */
	}
      else
	{
	  symbol *lbl = newiTempLabel (NULL);
	  emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
	  emitcode ("jb", "%s,%05d$", AOP (right)->aopu.aop_dir, (lbl->key + 100));
	  emitcode ("cpl", "c");
	  emitcode ("", "%05d$:", (lbl->key + 100));
	}
      /* c = 1 if egal */
      if (AOP_TYPE (result) == AOP_CRY && AOP_SIZE (result))
	{
	  outBitC (result);
	  goto release;
	}
      if (ifx)
	{
	  genIfxJump (ifx, "c");
	  goto release;
	}
      /* if the result is used in an arithmetic operation
         then put the result in place */
      outBitC (result);
    }
  else
    {
      gencjne (left, right, newiTempLabel (NULL));
      if (AOP_TYPE (result) == AOP_CRY && AOP_SIZE (result))
	{
	  aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
	  goto release;
	}
      if (ifx)
	{
	  genIfxJump (ifx, "a");
	  goto release;
	}
      /* if the result is used in an arithmetic operation
         then put the result in place */
      if (AOP_TYPE (result) != AOP_CRY)
	outAcc (result);
      /* leave the result in acc */
    }

release:
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* ifxForOp - returns the icode containing the ifx for operand     */
/*-----------------------------------------------------------------*/
static iCode *
ifxForOp (operand * op, iCode * ic)
{
  /* if true symbol then needs to be assigned */
  if (IS_TRUE_SYMOP (op))
    return NULL;

  /* if this has register type condition and
     the next instruction is ifx with the same operand
     and live to of the operand is upto the ifx only then */
  if (ic->next &&
      ic->next->op == IFX &&
      IC_COND (ic->next)->key == op->key &&
      OP_SYMBOL (op)->liveTo <= ic->next->seq)
    return ic->next;

  return NULL;
}

/*-----------------------------------------------------------------*/
/* hasInc - operand is incremented before any other use            */
/*-----------------------------------------------------------------*/
static iCode *
hasInc (operand *op, iCode *ic,int osize)
{
  sym_link *type = operandType(op);
  sym_link *retype = getSpec (type);
  iCode *lic = ic->next;
  int isize ;

  /* this could from a cast, e.g.: "(char xdata *) 0x7654;" */
  if (!IS_SYMOP(op)) return NULL;

  if (IS_BITVAR(retype)||!IS_PTR(type)) return NULL;
  if (IS_AGGREGATE(type->next)) return NULL;
  if (osize != (isize = getSize(type->next))) return NULL;

  while (lic) {
    /* if operand of the form op = op + <sizeof *op> */
    if (lic->op == '+' && isOperandEqual(IC_LEFT(lic),op) &&
	isOperandEqual(IC_RESULT(lic),op) &&
	isOperandLiteral(IC_RIGHT(lic)) &&
	operandLitValue(IC_RIGHT(lic)) == isize) {
      return lic;
    }
    /* if the operand used or deffed */
    if (bitVectBitValue(OP_USES(op),lic->key) || (unsigned) lic->defKey == op->key) {
      return NULL;
    }
    /* if GOTO or IFX */
    if (lic->op == IFX || lic->op == GOTO || lic->op == LABEL) break;
    lic = lic->next;
  }
  return NULL;
}

/*-----------------------------------------------------------------*/
/* genAndOp - for && operation                                     */
/*-----------------------------------------------------------------*/
static void
genAndOp (iCode * ic)
{
  operand *left, *right, *result;
  symbol *tlbl;

  D(emitcode (";     genAndOp",""));

  /* note here that && operations that are in an
     if statement are taken away by backPatchLabels
     only those used in arthmetic operations remain */
  aopOp ((left = IC_LEFT (ic)), ic, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, FALSE);

  /* if both are bit variables */
  if (AOP_TYPE (left) == AOP_CRY &&
      AOP_TYPE (right) == AOP_CRY)
    {
      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
      emitcode ("anl", "c,%s", AOP (right)->aopu.aop_dir);
      outBitC (result);
    }
  else
    {
      tlbl = newiTempLabel (NULL);
      toBoolean (left);
      emitcode ("jz", "%05d$", tlbl->key + 100);
      toBoolean (right);
      emitcode ("", "%05d$:", tlbl->key + 100);
      outBitAcc (result);
    }

  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}


/*-----------------------------------------------------------------*/
/* genOrOp - for || operation                                      */
/*-----------------------------------------------------------------*/
static void
genOrOp (iCode * ic)
{
  operand *left, *right, *result;
  symbol *tlbl;

  D(emitcode (";     genOrOp",""));

  /* note here that || operations that are in an
     if statement are taken away by backPatchLabels
     only those used in arthmetic operations remain */
  aopOp ((left = IC_LEFT (ic)), ic, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, FALSE);

  /* if both are bit variables */
  if (AOP_TYPE (left) == AOP_CRY &&
      AOP_TYPE (right) == AOP_CRY)
    {
      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
      emitcode ("orl", "c,%s", AOP (right)->aopu.aop_dir);
      outBitC (result);
    }
  else
    {
      tlbl = newiTempLabel (NULL);
      toBoolean (left);
      emitcode ("jnz", "%05d$", tlbl->key + 100);
      toBoolean (right);
      emitcode ("", "%05d$:", tlbl->key + 100);
      outBitAcc (result);
    }

  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* isLiteralBit - test if lit == 2^n                               */
/*-----------------------------------------------------------------*/
static int
isLiteralBit (unsigned long lit)
{
  unsigned long pw[32] =
  {1L, 2L, 4L, 8L, 16L, 32L, 64L, 128L,
   0x100L, 0x200L, 0x400L, 0x800L,
   0x1000L, 0x2000L, 0x4000L, 0x8000L,
   0x10000L, 0x20000L, 0x40000L, 0x80000L,
   0x100000L, 0x200000L, 0x400000L, 0x800000L,
   0x1000000L, 0x2000000L, 0x4000000L, 0x8000000L,
   0x10000000L, 0x20000000L, 0x40000000L, 0x80000000L};
  int idx;

  for (idx = 0; idx < 32; idx++)
    if (lit == pw[idx])
      return idx + 1;
  return 0;
}

/*-----------------------------------------------------------------*/
/* continueIfTrue -                                                */
/*-----------------------------------------------------------------*/
static void
continueIfTrue (iCode * ic)
{
  if (IC_TRUE (ic))
    emitcode ("ljmp", "%05d$", IC_TRUE (ic)->key + 100);
  ic->generated = 1;
}

/*-----------------------------------------------------------------*/
/* jmpIfTrue -                                                     */
/*-----------------------------------------------------------------*/
static void
jumpIfTrue (iCode * ic)
{
  if (!IC_TRUE (ic))
    emitcode ("ljmp", "%05d$", IC_FALSE (ic)->key + 100);
  ic->generated = 1;
}

/*-----------------------------------------------------------------*/
/* jmpTrueOrFalse -                                                */
/*-----------------------------------------------------------------*/
static void
jmpTrueOrFalse (iCode * ic, symbol * tlbl)
{
  // ugly but optimized by peephole
  if (IC_TRUE (ic))
    {
      symbol *nlbl = newiTempLabel (NULL);
      emitcode ("sjmp", "%05d$", nlbl->key + 100);
      emitcode ("", "%05d$:", tlbl->key + 100);
      emitcode ("ljmp", "%05d$", IC_TRUE (ic)->key + 100);
      emitcode ("", "%05d$:", nlbl->key + 100);
    }
  else
    {
      emitcode ("ljmp", "%05d$", IC_FALSE (ic)->key + 100);
      emitcode ("", "%05d$:", tlbl->key + 100);
    }
  ic->generated = 1;
}

/*-----------------------------------------------------------------*/
/* genAnd  - code for and                                          */
/*-----------------------------------------------------------------*/
static void
genAnd (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  int size, offset = 0;
  unsigned long lit = 0L;
  int bytelit = 0;
  char buffer[10];

  D(emitcode (";     genAnd",""));

  aopOp ((left = IC_LEFT (ic)), ic, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, TRUE);

#ifdef DEBUG_TYPE
  emitcode ("", "; Type res[%d] = l[%d]&r[%d]",
	    AOP_TYPE (result),
	    AOP_TYPE (left), AOP_TYPE (right));
  emitcode ("", "; Size res[%d] = l[%d]&r[%d]",
	    AOP_SIZE (result),
	    AOP_SIZE (left), AOP_SIZE (right));
#endif

  /* if left is a literal & right is not then exchange them */
  if ((AOP_TYPE (left) == AOP_LIT && AOP_TYPE (right) != AOP_LIT) ||
      AOP_NEEDSACC (left))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if result = right then exchange them */
  if (sameRegs (AOP (result), AOP (right)))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if right is bit then exchange them */
  if (AOP_TYPE (right) == AOP_CRY &&
      AOP_TYPE (left) != AOP_CRY)
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }
  if (AOP_TYPE (right) == AOP_LIT)
    lit = (unsigned long) floatFromVal (AOP (right)->aopu.aop_lit);

  size = AOP_SIZE (result);

  // if(bit & yy)
  // result = bit & yy;
  if (AOP_TYPE (left) == AOP_CRY)
    {
      // c = bit & literal;
      if (AOP_TYPE (right) == AOP_LIT)
	{
	  if (lit & 1)
	    {
	      if (size && sameRegs (AOP (result), AOP (left)))
		// no change
		goto release;
	      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
	    }
	  else
	    {
	      // bit(result) = 0;
	      if (size && (AOP_TYPE (result) == AOP_CRY))
		{
		  emitcode ("clr", "%s", AOP (result)->aopu.aop_dir);
		  goto release;
		}
	      if ((AOP_TYPE (result) == AOP_CRY) && ifx)
		{
		  jumpIfTrue (ifx);
		  goto release;
		}
	      emitcode ("clr", "c");
	    }
	}
      else
	{
	  if (AOP_TYPE (right) == AOP_CRY)
	    {
	      // c = bit & bit;
	      emitcode ("mov", "c,%s", AOP (right)->aopu.aop_dir);
	      emitcode ("anl", "c,%s", AOP (left)->aopu.aop_dir);
	    }
	  else
	    {
	      // c = bit & val;
	      MOVA (aopGet (AOP (right), 0, FALSE, FALSE));
	      // c = lsb
	      emitcode ("rrc", "a");
	      emitcode ("anl", "c,%s", AOP (left)->aopu.aop_dir);
	    }
	}
      // bit = c
      // val = c
      if (size)
	outBitC (result);
      // if(bit & ...)
      else if ((AOP_TYPE (result) == AOP_CRY) && ifx)
	genIfxJump (ifx, "c");
      goto release;
    }

  // if(val & 0xZZ)       - size = 0, ifx != FALSE  -
  // bit = val & 0xZZ     - size = 1, ifx = FALSE -
  if ((AOP_TYPE (right) == AOP_LIT) &&
      (AOP_TYPE (result) == AOP_CRY) &&
      (AOP_TYPE (left) != AOP_CRY))
    {
      int posbit = isLiteralBit (lit);
      /* left &  2^n */
      if (posbit)
	{
	  posbit--;
	  MOVA (aopGet (AOP (left), posbit >> 3, FALSE, FALSE));
	  // bit = left & 2^n
	  if (size)
	    emitcode ("mov", "c,acc.%d", posbit & 0x07);
	  // if(left &  2^n)
	  else
	    {
	      if (ifx)
		{
		  sprintf (buffer, "acc.%d", posbit & 0x07);
		  genIfxJump (ifx, buffer);
		}
	      goto release;
	    }
	}
      else
	{
	  symbol *tlbl = newiTempLabel (NULL);
	  int sizel = AOP_SIZE (left);
	  if (size)
	    emitcode ("setb", "c");
	  while (sizel--)
	    {
	      if ((bytelit = ((lit >> (offset * 8)) & 0x0FFL)) != 0x0L)
		{
		  MOVA (aopGet (AOP (left), offset, FALSE, FALSE));
		  // byte ==  2^n ?
		  if ((posbit = isLiteralBit (bytelit)) != 0)
		    emitcode ("jb", "acc.%d,%05d$", (posbit - 1) & 0x07, tlbl->key + 100);
		  else
		    {
		      if (bytelit != 0x0FFL)
			emitcode ("anl", "a,%s",
				  aopGet (AOP (right), offset, FALSE, TRUE));
		      emitcode ("jnz", "%05d$", tlbl->key + 100);
		    }
		}
	      offset++;
	    }
	  // bit = left & literal
	  if (size)
	    {
	      emitcode ("clr", "c");
	      emitcode ("", "%05d$:", tlbl->key + 100);
	    }
	  // if(left & literal)
	  else
	    {
	      if (ifx)
		jmpTrueOrFalse (ifx, tlbl);
              else
	        emitcode ("", "%05d$:", tlbl->key + 100);
	      goto release;
	    }
	}
      outBitC (result);
      goto release;
    }

  /* if left is same as result */
  if (sameRegs (AOP (result), AOP (left)))
    {
      for (; size--; offset++)
	{
	  if (AOP_TYPE (right) == AOP_LIT)
	    {
	      if ((bytelit = (int) ((lit >> (offset * 8)) & 0x0FFL)) == 0x0FF)
		continue;
	      else if (bytelit == 0)
		{
		  aopPut (AOP (result), zero, offset, isOperandVolatile (result, FALSE));
		}
	      else if (IS_AOP_PREG (result))
		{
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  emitcode ("anl", "a,%s", aopGet (AOP (left), offset, FALSE, TRUE));
		  aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
		}
	      else
		emitcode ("anl", "%s,%s",
			  aopGet (AOP (left), offset, FALSE, TRUE),
			  aopGet (AOP (right), offset, FALSE, FALSE));
	    }
	  else
	    {
	      if (AOP_TYPE (left) == AOP_ACC)
		emitcode ("anl", "a,%s", aopGet (AOP (right), offset, FALSE, FALSE));
	      else
		{
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  if (IS_AOP_PREG (result))
		    {
		      emitcode ("anl", "a,%s", aopGet (AOP (left), offset, FALSE, TRUE));
		      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));

		    }
		  else
		    emitcode ("anl", "%s,a",
			      aopGet (AOP (left), offset, FALSE, TRUE));
		}
	    }
	}
    }
  else
    {
      // left & result in different registers
      if (AOP_TYPE (result) == AOP_CRY)
	{
	  // result = bit
	  // if(size), result in bit
	  // if(!size && ifx), conditional oper: if(left & right)
	  symbol *tlbl = newiTempLabel (NULL);
	  int sizer = min (AOP_SIZE (left), AOP_SIZE (right));
	  if (size)
	    emitcode ("setb", "c");
	  while (sizer--)
	    {
	      if (AOP_TYPE(right)==AOP_REG && AOP_TYPE(left)==AOP_ACC) {
		emitcode ("anl", "a,%s",
			  aopGet (AOP (right), offset, FALSE, FALSE));
	      } else {
		if (AOP_TYPE(left)==AOP_ACC) {
		  emitcode("mov", "b,a");
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  emitcode("anl", "a,b");
		}else {
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  emitcode ("anl", "a,%s",
			    aopGet (AOP (left), offset, FALSE, FALSE));
		}
	      }
	      emitcode ("jnz", "%05d$", tlbl->key + 100);
	      offset++;
	    }
	  if (size)
	    {
	      CLRC;
	      emitcode ("", "%05d$:", tlbl->key + 100);
	      outBitC (result);
	    }
	  else if (ifx)
	    jmpTrueOrFalse (ifx, tlbl);
          else
	    emitcode ("", "%05d$:", tlbl->key + 100);
	}
      else
	{
	  for (; (size--); offset++)
	    {
	      // normal case
	      // result = left & right
	      if (AOP_TYPE (right) == AOP_LIT)
		{
		  if ((bytelit = (int) ((lit >> (offset * 8)) & 0x0FFL)) == 0x0FF)
		    {
		      aopPut (AOP (result),
			      aopGet (AOP (left), offset, FALSE, FALSE),
			      offset,
			      isOperandVolatile (result, FALSE));
		      continue;
		    }
		  else if (bytelit == 0)
		    {
		      /* dummy read of volatile operand */
		      if (isOperandVolatile (left, FALSE))
			MOVA (aopGet (AOP (left), offset, FALSE, FALSE));
		      aopPut (AOP (result), zero, offset, isOperandVolatile (result, FALSE));
		      continue;
		    }
		}
	      // faster than result <- left, anl result,right
	      // and better if result is SFR
	      if (AOP_TYPE (left) == AOP_ACC)
		emitcode ("anl", "a,%s", aopGet (AOP (right), offset, FALSE, FALSE));
	      else
		{
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  emitcode ("anl", "a,%s",
			    aopGet (AOP (left), offset, FALSE, FALSE));
		}
	      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
	    }
	}
    }

release:
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genOr  - code for or                                            */
/*-----------------------------------------------------------------*/
static void
genOr (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  int size, offset = 0;
  unsigned long lit = 0L;

  D(emitcode (";     genOr",""));

  aopOp ((left = IC_LEFT (ic)), ic, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, TRUE);

#ifdef DEBUG_TYPE
  emitcode ("", "; Type res[%d] = l[%d]&r[%d]",
	    AOP_TYPE (result),
	    AOP_TYPE (left), AOP_TYPE (right));
  emitcode ("", "; Size res[%d] = l[%d]&r[%d]",
	    AOP_SIZE (result),
	    AOP_SIZE (left), AOP_SIZE (right));
#endif

  /* if left is a literal & right is not then exchange them */
  if ((AOP_TYPE (left) == AOP_LIT && AOP_TYPE (right) != AOP_LIT) ||
      AOP_NEEDSACC (left))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if result = right then exchange them */
  if (sameRegs (AOP (result), AOP (right)))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if right is bit then exchange them */
  if (AOP_TYPE (right) == AOP_CRY &&
      AOP_TYPE (left) != AOP_CRY)
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }
  if (AOP_TYPE (right) == AOP_LIT)
    lit = (unsigned long) floatFromVal (AOP (right)->aopu.aop_lit);

  size = AOP_SIZE (result);

  // if(bit | yy)
  // xx = bit | yy;
  if (AOP_TYPE (left) == AOP_CRY)
    {
      if (AOP_TYPE (right) == AOP_LIT)
	{
	  // c = bit | literal;
	  if (lit)
	    {
	      // lit != 0 => result = 1
	      if (AOP_TYPE (result) == AOP_CRY)
		{
		  if (size)
		    emitcode ("setb", "%s", AOP (result)->aopu.aop_dir);
		  else if (ifx)
		    continueIfTrue (ifx);
		  goto release;
		}
	      emitcode ("setb", "c");
	    }
	  else
	    {
	      // lit == 0 => result = left
	      if (size && sameRegs (AOP (result), AOP (left)))
		goto release;
	      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
	    }
	}
      else
	{
	  if (AOP_TYPE (right) == AOP_CRY)
	    {
	      // c = bit | bit;
	      emitcode ("mov", "c,%s", AOP (right)->aopu.aop_dir);
	      emitcode ("orl", "c,%s", AOP (left)->aopu.aop_dir);
	    }
	  else
	    {
	      // c = bit | val;
	      symbol *tlbl = newiTempLabel (NULL);
	      if (!((AOP_TYPE (result) == AOP_CRY) && ifx))
		emitcode ("setb", "c");
	      emitcode ("jb", "%s,%05d$",
			AOP (left)->aopu.aop_dir, tlbl->key + 100);
	      toBoolean (right);
	      emitcode ("jnz", "%05d$", tlbl->key + 100);
	      if ((AOP_TYPE (result) == AOP_CRY) && ifx)
		{
		  jmpTrueOrFalse (ifx, tlbl);
		  goto release;
		}
	      else
		{
		  CLRC;
		  emitcode ("", "%05d$:", tlbl->key + 100);
		}
	    }
	}
      // bit = c
      // val = c
      if (size)
	outBitC (result);
      // if(bit | ...)
      else if ((AOP_TYPE (result) == AOP_CRY) && ifx)
	genIfxJump (ifx, "c");
      goto release;
    }

  // if(val | 0xZZ)       - size = 0, ifx != FALSE  -
  // bit = val | 0xZZ     - size = 1, ifx = FALSE -
  if ((AOP_TYPE (right) == AOP_LIT) &&
      (AOP_TYPE (result) == AOP_CRY) &&
      (AOP_TYPE (left) != AOP_CRY))
    {
      if (lit)
	{
	  // result = 1
	  if (size)
	    emitcode ("setb", "%s", AOP (result)->aopu.aop_dir);
	  else
	    continueIfTrue (ifx);
	  goto release;
	}
      else
	{
	  // lit = 0, result = boolean(left)
	  if (size)
	    emitcode ("setb", "c");
	  toBoolean (right);
	  if (size)
	    {
	      symbol *tlbl = newiTempLabel (NULL);
	      emitcode ("jnz", "%05d$", tlbl->key + 100);
	      CLRC;
	      emitcode ("", "%05d$:", tlbl->key + 100);
	    }
	  else
	    {
	      genIfxJump (ifx, "a");
	      goto release;
	    }
	}
      outBitC (result);
      goto release;
    }

  /* if left is same as result */
  if (sameRegs (AOP (result), AOP (left)))
    {
      for (; size--; offset++)
	{
	  if (AOP_TYPE (right) == AOP_LIT)
	    {
	      if (((lit >> (offset * 8)) & 0x0FFL) == 0x00L)
		{
		  /* dummy read of volatile operand */
		  if (isOperandVolatile (left, FALSE))
		    MOVA (aopGet (AOP (left), offset, FALSE, FALSE));
		  else
        	    continue;
		}
	      else if (IS_AOP_PREG (left))
		{
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  emitcode ("orl", "a,%s", aopGet (AOP (left), offset, FALSE, TRUE));
		  aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
		}
	      else
		emitcode ("orl", "%s,%s",
			  aopGet (AOP (left), offset, FALSE, TRUE),
			  aopGet (AOP (right), offset, FALSE, FALSE));
	    }
	  else
	    {
	      if (AOP_TYPE (left) == AOP_ACC)
		emitcode ("orl", "a,%s", aopGet (AOP (right), offset, FALSE, FALSE));
	      else
		{
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  if (IS_AOP_PREG (left))
		    {
		      emitcode ("orl", "a,%s", aopGet (AOP (left), offset, FALSE, TRUE));
		      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
		    }
		  else
		    emitcode ("orl", "%s,a",
			      aopGet (AOP (left), offset, FALSE, TRUE));
		}
	    }
	}
    }
  else
    {
      // left & result in different registers
      if (AOP_TYPE (result) == AOP_CRY)
	{
	  // result = bit
	  // if(size), result in bit
	  // if(!size && ifx), conditional oper: if(left | right)
	  symbol *tlbl = newiTempLabel (NULL);
	  int sizer = max (AOP_SIZE (left), AOP_SIZE (right));
	  if (size)
	    emitcode ("setb", "c");
	  while (sizer--)
	    {
	      if (AOP_TYPE(right)==AOP_REG && AOP_TYPE(left)==AOP_ACC) {
		emitcode ("orl", "a,%s",
			  aopGet (AOP (right), offset, FALSE, FALSE));
	      } else {
		MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		emitcode ("orl", "a,%s",
			  aopGet (AOP (left), offset, FALSE, FALSE));
	      }
	      emitcode ("jnz", "%05d$", tlbl->key + 100);
	      offset++;
	    }
	  if (size)
	    {
	      CLRC;
	      emitcode ("", "%05d$:", tlbl->key + 100);
	      outBitC (result);
	    }
	  else if (ifx)
	    jmpTrueOrFalse (ifx, tlbl);
	  else
	    emitcode ("", "%05d$:", tlbl->key + 100);
	}
      else
	for (; (size--); offset++)
	  {
	    // normal case
	    // result = left & right
	    if (AOP_TYPE (right) == AOP_LIT)
	      {
		if (((lit >> (offset * 8)) & 0x0FFL) == 0x00L)
		  {
		    aopPut (AOP (result),
			    aopGet (AOP (left), offset, FALSE, FALSE),
			    offset,
			    isOperandVolatile (result, FALSE));
		    continue;
		  }
	      }
	    // faster than result <- left, anl result,right
	    // and better if result is SFR
	    if (AOP_TYPE (left) == AOP_ACC)
	      emitcode ("orl", "a,%s", aopGet (AOP (right), offset, FALSE, FALSE));
	    else
	      {
		MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		emitcode ("orl", "a,%s",
			  aopGet (AOP (left), offset, FALSE, FALSE));
	      }
	    aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
	  }
    }

release:
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genXor - code for xclusive or                                   */
/*-----------------------------------------------------------------*/
static void
genXor (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  int size, offset = 0;
  unsigned long lit = 0L;

  D(emitcode (";     genXor",""));

  aopOp ((left = IC_LEFT (ic)), ic, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, TRUE);

#ifdef DEBUG_TYPE
  emitcode ("", "; Type res[%d] = l[%d]&r[%d]",
	    AOP_TYPE (result),
	    AOP_TYPE (left), AOP_TYPE (right));
  emitcode ("", "; Size res[%d] = l[%d]&r[%d]",
	    AOP_SIZE (result),
	    AOP_SIZE (left), AOP_SIZE (right));
#endif

  /* if left is a literal & right is not ||
     if left needs acc & right does not */
  if ((AOP_TYPE (left) == AOP_LIT && AOP_TYPE (right) != AOP_LIT) ||
      (AOP_NEEDSACC (left) && !AOP_NEEDSACC (right)))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if result = right then exchange them */
  if (sameRegs (AOP (result), AOP (right)))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if right is bit then exchange them */
  if (AOP_TYPE (right) == AOP_CRY &&
      AOP_TYPE (left) != AOP_CRY)
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }
  if (AOP_TYPE (right) == AOP_LIT)
    lit = (unsigned long) floatFromVal (AOP (right)->aopu.aop_lit);

  size = AOP_SIZE (result);

  // if(bit ^ yy)
  // xx = bit ^ yy;
  if (AOP_TYPE (left) == AOP_CRY)
    {
      if (AOP_TYPE (right) == AOP_LIT)
	{
	  // c = bit & literal;
	  if (lit >> 1)
	    {
	      // lit>>1  != 0 => result = 1
	      if (AOP_TYPE (result) == AOP_CRY)
		{
		  if (size)
		    emitcode ("setb", "%s", AOP (result)->aopu.aop_dir);
		  else if (ifx)
		    continueIfTrue (ifx);
		  goto release;
		}
	      emitcode ("setb", "c");
	    }
	  else
	    {
	      // lit == (0 or 1)
	      if (lit == 0)
		{
		  // lit == 0, result = left
		  if (size && sameRegs (AOP (result), AOP (left)))
		    goto release;
		  emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
		}
	      else
		{
		  // lit == 1, result = not(left)
		  if (size && sameRegs (AOP (result), AOP (left)))
		    {
		      emitcode ("cpl", "%s", AOP (result)->aopu.aop_dir);
		      goto release;
		    }
		  else
		    {
		      emitcode ("mov", "c,%s", AOP (left)->aopu.aop_dir);
		      emitcode ("cpl", "c");
		    }
		}
	    }

	}
      else
	{
	  // right != literal
	  symbol *tlbl = newiTempLabel (NULL);
	  if (AOP_TYPE (right) == AOP_CRY)
	    {
	      // c = bit ^ bit;
	      emitcode ("mov", "c,%s", AOP (right)->aopu.aop_dir);
	    }
	  else
	    {
	      int sizer = AOP_SIZE (right);
	      // c = bit ^ val
	      // if val>>1 != 0, result = 1
	      emitcode ("setb", "c");
	      while (sizer)
		{
		  MOVA (aopGet (AOP (right), sizer - 1, FALSE, FALSE));
		  if (sizer == 1)
		    // test the msb of the lsb
		    emitcode ("anl", "a,#0xfe");
		  emitcode ("jnz", "%05d$", tlbl->key + 100);
		  sizer--;
		}
	      // val = (0,1)
	      emitcode ("rrc", "a");
	    }
	  emitcode ("jnb", "%s,%05d$", AOP (left)->aopu.aop_dir, (tlbl->key + 100));
	  emitcode ("cpl", "c");
	  emitcode ("", "%05d$:", (tlbl->key + 100));
	}
      // bit = c
      // val = c
      if (size)
	outBitC (result);
      // if(bit | ...)
      else if ((AOP_TYPE (result) == AOP_CRY) && ifx)
	genIfxJump (ifx, "c");
      goto release;
    }

  if (sameRegs (AOP (result), AOP (left)))
    {
      /* if left is same as result */
      for (; size--; offset++)
	{
	  if (AOP_TYPE (right) == AOP_LIT)
	    {
	      if (((lit >> (offset * 8)) & 0x0FFL) == 0x00L)
		continue;
	      else if (IS_AOP_PREG (left))
		{
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  emitcode ("xrl", "a,%s", aopGet (AOP (left), offset, FALSE, TRUE));
		  aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
		}
	      else
		emitcode ("xrl", "%s,%s",
			  aopGet (AOP (left), offset, FALSE, TRUE),
			  aopGet (AOP (right), offset, FALSE, FALSE));
	    }
	  else
	    {
	      if (AOP_TYPE (left) == AOP_ACC)
		emitcode ("xrl", "a,%s", aopGet (AOP (right), offset, FALSE, FALSE));
	      else
		{
		  MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		  if (IS_AOP_PREG (left))
		    {
		      emitcode ("xrl", "a,%s", aopGet (AOP (left), offset, FALSE, TRUE));
		      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
		    }
		  else
		    emitcode ("xrl", "%s,a",
			      aopGet (AOP (left), offset, FALSE, TRUE));
		}
	    }
	}
    }
  else
    {
      // left & result in different registers
      if (AOP_TYPE (result) == AOP_CRY)
	{
	  // result = bit
	  // if(size), result in bit
	  // if(!size && ifx), conditional oper: if(left ^ right)
	  symbol *tlbl = newiTempLabel (NULL);
	  int sizer = max (AOP_SIZE (left), AOP_SIZE (right));
	  if (size)
	    emitcode ("setb", "c");
	  while (sizer--)
	    {
	      if ((AOP_TYPE (right) == AOP_LIT) &&
		  (((lit >> (offset * 8)) & 0x0FFL) == 0x00L))
		{
		  MOVA (aopGet (AOP (left), offset, FALSE, FALSE));
		}
	      else
		{
		  if (AOP_TYPE(right)==AOP_REG && AOP_TYPE(left)==AOP_ACC) {
		    emitcode ("xrl", "a,%s",
			      aopGet (AOP (right), offset, FALSE, FALSE));
		  } else {
		    MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		    emitcode ("xrl", "a,%s",
			      aopGet (AOP (left), offset, FALSE, FALSE));
		  }
		}
	      emitcode ("jnz", "%05d$", tlbl->key + 100);
	      offset++;
	    }
	  if (size)
	    {
	      CLRC;
	      emitcode ("", "%05d$:", tlbl->key + 100);
	      outBitC (result);
	    }
	  else if (ifx)
	    jmpTrueOrFalse (ifx, tlbl);
	}
      else
	for (; (size--); offset++)
	  {
	    // normal case
	    // result = left & right
	    if (AOP_TYPE (right) == AOP_LIT)
	      {
		if (((lit >> (offset * 8)) & 0x0FFL) == 0x00L)
		  {
		    aopPut (AOP (result),
			    aopGet (AOP (left), offset, FALSE, FALSE),
			    offset,
			    isOperandVolatile (result, FALSE));
		    continue;
		  }
	      }
	    // faster than result <- left, anl result,right
	    // and better if result is SFR
	    if (AOP_TYPE (left) == AOP_ACC)
	      emitcode ("xrl", "a,%s", aopGet (AOP (right), offset, FALSE, FALSE));
	    else
	      {
		MOVA (aopGet (AOP (right), offset, FALSE, FALSE));
		emitcode ("xrl", "a,%s",
			  aopGet (AOP (left), offset, FALSE, TRUE));
	      }
	    aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
	  }
    }

release:
  freeAsmop (right, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (left, NULL, ic, (RESULTONSTACK (ic) ? FALSE : TRUE));
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genInline - write the inline code out                           */
/*-----------------------------------------------------------------*/
static void
genInline (iCode * ic)
{
  char *buffer, *bp, *bp1;

  D(emitcode (";     genInline",""));

  _G.inLine += (!options.asmpeep);

  buffer = bp = bp1 = Safe_calloc(1, strlen(IC_INLINE(ic))+1);
  strcpy (buffer, IC_INLINE (ic));

  /* emit each line as a code */
  while (*bp)
    {
      if (*bp == '\n')
	{
	  *bp++ = '\0';
	  emitcode (bp1, "");
	  bp1 = bp;
	}
      else
	{
	  if (*bp == ':')
	    {
	      bp++;
	      *bp = '\0';
	      bp++;
	      emitcode (bp1, "");
	      bp1 = bp;
	    }
	  else
	    bp++;
	}
    }
  if (bp1 != bp)
    emitcode (bp1, "");
  /*     emitcode("",buffer); */
  _G.inLine -= (!options.asmpeep);
}

/*-----------------------------------------------------------------*/
/* genRRC - rotate right with carry                                */
/*-----------------------------------------------------------------*/
static void
genRRC (iCode * ic)
{
  operand *left, *result;
  int size, offset = 0;
  char *l;

  D(emitcode (";     genRRC",""));

  /* rotate right with carry */
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  /* move it to the result */
  size = AOP_SIZE (result);
  offset = size - 1;
  if (size == 1) { /* special case for 1 byte */
      l = aopGet (AOP (left), offset, FALSE, FALSE);
      MOVA (l);
      emitcode ("rr", "a");
      goto release;
  }
  CLRC;
  while (size--)
    {
      l = aopGet (AOP (left), offset, FALSE, FALSE);
      MOVA (l);
      emitcode ("rrc", "a");
      if (AOP_SIZE (result) > 1)
	aopPut (AOP (result), "a", offset--, isOperandVolatile (result, FALSE));
    }
  /* now we need to put the carry into the
     highest order byte of the result */
  if (AOP_SIZE (result) > 1)
    {
      l = aopGet (AOP (result), AOP_SIZE (result) - 1, FALSE, FALSE);
      MOVA (l);
    }
  emitcode ("mov", "acc.7,c");
 release:
  aopPut (AOP (result), "a", AOP_SIZE (result) - 1, isOperandVolatile (result, FALSE));
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genRLC - generate code for rotate left with carry               */
/*-----------------------------------------------------------------*/
static void
genRLC (iCode * ic)
{
  operand *left, *result;
  int size, offset = 0;
  char *l;

  D(emitcode (";     genRLC",""));

  /* rotate right with carry */
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  /* move it to the result */
  size = AOP_SIZE (result);
  offset = 0;
  if (size--)
    {
      l = aopGet (AOP (left), offset, FALSE, FALSE);
      MOVA (l);
      if (size == 0) { /* special case for 1 byte */
	      emitcode("rl","a");
	      goto release;
      }
      emitcode ("add", "a,acc");
      if (AOP_SIZE (result) > 1)
	aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
      while (size--)
	{
	  l = aopGet (AOP (left), offset, FALSE, FALSE);
	  MOVA (l);
	  emitcode ("rlc", "a");
	  if (AOP_SIZE (result) > 1)
	    aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
	}
    }
  /* now we need to put the carry into the
     highest order byte of the result */
  if (AOP_SIZE (result) > 1)
    {
      l = aopGet (AOP (result), 0, FALSE, FALSE);
      MOVA (l);
    }
  emitcode ("mov", "acc.0,c");
 release:
  aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genGetHbit - generates code get highest order bit               */
/*-----------------------------------------------------------------*/
static void
genGetHbit (iCode * ic)
{
  operand *left, *result;

  D(emitcode (";     genGetHbit",""));

  left = IC_LEFT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  /* get the highest order byte into a */
  MOVA (aopGet (AOP (left), AOP_SIZE (left) - 1, FALSE, FALSE));
  if (AOP_TYPE (result) == AOP_CRY)
    {
      emitcode ("rlc", "a");
      outBitC (result);
    }
  else
    {
      emitcode ("rl", "a");
      emitcode ("anl", "a,#0x01");
      outAcc (result);
    }


  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genSwap - generates code to swap nibbles or bytes               */
/*-----------------------------------------------------------------*/
static void
genSwap (iCode * ic)
{
  operand *left, *result;

  D(emitcode (";     genSwap",""));

  left = IC_LEFT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  switch (AOP_SIZE (left))
    {
    case 1: /* swap nibbles in byte */
      MOVA (aopGet (AOP (left), 0, FALSE, FALSE));
      emitcode ("swap", "a");
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      break;
    case 2: /* swap bytes in word */
      if (AOP_TYPE(left) == AOP_REG && sameRegs(AOP(left), AOP(result)))
	{
	  MOVA (aopGet (AOP (left), 0, FALSE, FALSE));
	  aopPut (AOP (result), aopGet (AOP (left), 1, FALSE, FALSE),
		  0, isOperandVolatile (result, FALSE));
	  aopPut (AOP (result), "a", 1, isOperandVolatile (result, FALSE));
	}
      else if (operandsEqu (left, result))
	{
          char * reg = "a";
	  MOVA (aopGet (AOP (left), 0, FALSE, FALSE));
	  if (aopGetUsesAcc(AOP (left), 1) || aopGetUsesAcc(AOP (result), 0))
	    {
	      emitcode ("mov", "b,a");
              reg = "b";
            }
	  aopPut (AOP (result), aopGet (AOP (left), 1, FALSE, FALSE),
		  0, isOperandVolatile (result, FALSE));
	  aopPut (AOP (result), reg, 1, isOperandVolatile (result, FALSE));
	}
      else
	{
	  aopPut (AOP (result), aopGet (AOP (left), 1, FALSE, FALSE),
		  0, isOperandVolatile (result, FALSE));
	  aopPut (AOP (result), aopGet (AOP (left), 0, FALSE, FALSE),
		  1, isOperandVolatile (result, FALSE));
	}
      break;
    default:
      wassertl(FALSE, "unsupported SWAP operand size");
    }

  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}


/*-----------------------------------------------------------------*/
/* AccRol - rotate left accumulator by known count                 */
/*-----------------------------------------------------------------*/
static void
AccRol (int shCount)
{
  shCount &= 0x0007;		// shCount : 0..7

  switch (shCount)
    {
    case 0:
      break;
    case 1:
      emitcode ("rl", "a");
      break;
    case 2:
      emitcode ("rl", "a");
      emitcode ("rl", "a");
      break;
    case 3:
      emitcode ("swap", "a");
      emitcode ("rr", "a");
      break;
    case 4:
      emitcode ("swap", "a");
      break;
    case 5:
      emitcode ("swap", "a");
      emitcode ("rl", "a");
      break;
    case 6:
      emitcode ("rr", "a");
      emitcode ("rr", "a");
      break;
    case 7:
      emitcode ("rr", "a");
      break;
    }
}

/*-----------------------------------------------------------------*/
/* AccLsh - left shift accumulator by known count                  */
/*-----------------------------------------------------------------*/
static void
AccLsh (int shCount)
{
  if (shCount != 0)
    {
      if (shCount == 1)
	emitcode ("add", "a,acc");
      else if (shCount == 2)
	{
	  emitcode ("add", "a,acc");
	  emitcode ("add", "a,acc");
	}
      else
	{
	  /* rotate left accumulator */
	  AccRol (shCount);
	  /* and kill the lower order bits */
	  emitcode ("anl", "a,#0x%02x", SLMask[shCount]);
	}
    }
}

/*-----------------------------------------------------------------*/
/* AccRsh - right shift accumulator by known count                 */
/*-----------------------------------------------------------------*/
static void
AccRsh (int shCount)
{
  if (shCount != 0)
    {
      if (shCount == 1)
	{
	  CLRC;
	  emitcode ("rrc", "a");
	}
      else
	{
	  /* rotate right accumulator */
	  AccRol (8 - shCount);
	  /* and kill the higher order bits */
	  emitcode ("anl", "a,#0x%02x", SRMask[shCount]);
	}
    }
}

/*-----------------------------------------------------------------*/
/* AccSRsh - signed right shift accumulator by known count                 */
/*-----------------------------------------------------------------*/
static void
AccSRsh (int shCount)
{
  symbol *tlbl;
  if (shCount != 0)
    {
      if (shCount == 1)
	{
	  emitcode ("mov", "c,acc.7");
	  emitcode ("rrc", "a");
	}
      else if (shCount == 2)
	{
	  emitcode ("mov", "c,acc.7");
	  emitcode ("rrc", "a");
	  emitcode ("mov", "c,acc.7");
	  emitcode ("rrc", "a");
	}
      else
	{
	  tlbl = newiTempLabel (NULL);
	  /* rotate right accumulator */
	  AccRol (8 - shCount);
	  /* and kill the higher order bits */
	  emitcode ("anl", "a,#0x%02x", SRMask[shCount]);
	  emitcode ("jnb", "acc.%d,%05d$", 7 - shCount, tlbl->key + 100);
	  emitcode ("orl", "a,#0x%02x",
		    (unsigned char) ~SRMask[shCount]);
	  emitcode ("", "%05d$:", tlbl->key + 100);
	}
    }
}

/*-----------------------------------------------------------------*/
/* shiftR1Left2Result - shift right one byte from left to result   */
/*-----------------------------------------------------------------*/
static void
shiftR1Left2Result (operand * left, int offl,
		    operand * result, int offr,
		    int shCount, int sign)
{
  MOVA (aopGet (AOP (left), offl, FALSE, FALSE));
  /* shift right accumulator */
  if (sign)
    AccSRsh (shCount);
  else
    AccRsh (shCount);
  aopPut (AOP (result), "a", offr, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* shiftL1Left2Result - shift left one byte from left to result    */
/*-----------------------------------------------------------------*/
static void
shiftL1Left2Result (operand * left, int offl,
		    operand * result, int offr, int shCount)
{
  char *l;
  l = aopGet (AOP (left), offl, FALSE, FALSE);
  MOVA (l);
  /* shift left accumulator */
  AccLsh (shCount);
  aopPut (AOP (result), "a", offr, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* movLeft2Result - move byte from left to result                  */
/*-----------------------------------------------------------------*/
static void
movLeft2Result (operand * left, int offl,
		operand * result, int offr, int sign)
{
  char *l;
  if (!sameRegs (AOP (left), AOP (result)) || (offl != offr))
    {
      l = aopGet (AOP (left), offl, FALSE, FALSE);

      if (*l == '@' && (IS_AOP_PREG (result)))
	{
	  emitcode ("mov", "a,%s", l);
	  aopPut (AOP (result), "a", offr, isOperandVolatile (result, FALSE));
	}
      else
	{
	  if (!sign)
	    aopPut (AOP (result), l, offr, isOperandVolatile (result, FALSE));
	  else
	    {
	      /* MSB sign in acc.7 ! */
	      if (getDataSize (left) == offl + 1)
		{
		  emitcode ("mov", "a,%s", l);
		  aopPut (AOP (result), "a", offr, isOperandVolatile (result, FALSE));
		}
	    }
	}
    }
}

/*-----------------------------------------------------------------*/
/* AccAXRrl1 - right rotate c->a:x->c by 1                         */
/*-----------------------------------------------------------------*/
static void
AccAXRrl1 (char *x)
{
  emitcode ("rrc", "a");
  emitcode ("xch", "a,%s", x);
  emitcode ("rrc", "a");
  emitcode ("xch", "a,%s", x);
}

/*-----------------------------------------------------------------*/
/* AccAXLrl1 - left rotate c<-a:x<-c by 1                          */
/*-----------------------------------------------------------------*/
static void
AccAXLrl1 (char *x)
{
  emitcode ("xch", "a,%s", x);
  emitcode ("rlc", "a");
  emitcode ("xch", "a,%s", x);
  emitcode ("rlc", "a");
}

/*-----------------------------------------------------------------*/
/* AccAXLsh1 - left shift a:x<-0 by 1                              */
/*-----------------------------------------------------------------*/
static void
AccAXLsh1 (char *x)
{
  emitcode ("xch", "a,%s", x);
  emitcode ("add", "a,acc");
  emitcode ("xch", "a,%s", x);
  emitcode ("rlc", "a");
}

/*-----------------------------------------------------------------*/
/* AccAXLsh - left shift a:x by known count (0..7)                 */
/*-----------------------------------------------------------------*/
static void
AccAXLsh (char *x, int shCount)
{
  switch (shCount)
    {
    case 0:
      break;
    case 1:
      AccAXLsh1 (x);
      break;
    case 2:
      AccAXLsh1 (x);
      AccAXLsh1 (x);
      break;
    case 3:
    case 4:
    case 5:			// AAAAABBB:CCCCCDDD

      AccRol (shCount);		// BBBAAAAA:CCCCCDDD

      emitcode ("anl", "a,#0x%02x",
		SLMask[shCount]);	// BBB00000:CCCCCDDD

      emitcode ("xch", "a,%s", x);	// CCCCCDDD:BBB00000

      AccRol (shCount);		// DDDCCCCC:BBB00000

      emitcode ("xch", "a,%s", x);	// BBB00000:DDDCCCCC

      emitcode ("xrl", "a,%s", x);	// (BBB^DDD)CCCCC:DDDCCCCC

      emitcode ("xch", "a,%s", x);	// DDDCCCCC:(BBB^DDD)CCCCC

      emitcode ("anl", "a,#0x%02x",
		SLMask[shCount]);	// DDD00000:(BBB^DDD)CCCCC

      emitcode ("xch", "a,%s", x);	// (BBB^DDD)CCCCC:DDD00000

      emitcode ("xrl", "a,%s", x);	// BBBCCCCC:DDD00000

      break;
    case 6:			// AAAAAABB:CCCCCCDD
      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 000000BB:CCCCCCDD
      emitcode ("mov", "c,acc.0");	// c = B
      emitcode ("xch", "a,%s", x);	// CCCCCCDD:000000BB
#if 0 // REMOVE ME
      AccAXRrl1 (x);		// BCCCCCCD:D000000B
      AccAXRrl1 (x);		// BBCCCCCC:DD000000
#else
      emitcode("rrc","a");
      emitcode("xch","a,%s", x);
      emitcode("rrc","a");
      emitcode("mov","c,acc.0"); //<< get correct bit
      emitcode("xch","a,%s", x);

      emitcode("rrc","a");
      emitcode("xch","a,%s", x);
      emitcode("rrc","a");
      emitcode("xch","a,%s", x);
#endif
      break;
    case 7:			// a:x <<= 7

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 0000000B:CCCCCCCD

      emitcode ("mov", "c,acc.0");	// c = B

      emitcode ("xch", "a,%s", x);	// CCCCCCCD:0000000B

      AccAXRrl1 (x);		// BCCCCCCC:D0000000

      break;
    default:
      break;
    }
}

/*-----------------------------------------------------------------*/
/* AccAXRsh - right shift a:x known count (0..7)                   */
/*-----------------------------------------------------------------*/
static void
AccAXRsh (char *x, int shCount)
{
  switch (shCount)
    {
    case 0:
      break;
    case 1:
      CLRC;
      AccAXRrl1 (x);		// 0->a:x

      break;
    case 2:
      CLRC;
      AccAXRrl1 (x);		// 0->a:x

      CLRC;
      AccAXRrl1 (x);		// 0->a:x

      break;
    case 3:
    case 4:
    case 5:			// AAAAABBB:CCCCCDDD = a:x

      AccRol (8 - shCount);	// BBBAAAAA:DDDCCCCC

      emitcode ("xch", "a,%s", x);	// CCCCCDDD:BBBAAAAA

      AccRol (8 - shCount);	// DDDCCCCC:BBBAAAAA

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 000CCCCC:BBBAAAAA

      emitcode ("xrl", "a,%s", x);	// BBB(CCCCC^AAAAA):BBBAAAAA

      emitcode ("xch", "a,%s", x);	// BBBAAAAA:BBB(CCCCC^AAAAA)

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 000AAAAA:BBB(CCCCC^AAAAA)

      emitcode ("xch", "a,%s", x);	// BBB(CCCCC^AAAAA):000AAAAA

      emitcode ("xrl", "a,%s", x);	// BBBCCCCC:000AAAAA

      emitcode ("xch", "a,%s", x);	// 000AAAAA:BBBCCCCC

      break;
    case 6:			// AABBBBBB:CCDDDDDD

      emitcode ("mov", "c,acc.7");
      AccAXLrl1 (x);		// ABBBBBBC:CDDDDDDA

      emitcode ("mov", "c,acc.7");
      AccAXLrl1 (x);		// BBBBBBCC:DDDDDDAA

      emitcode ("xch", "a,%s", x);	// DDDDDDAA:BBBBBBCC

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 000000AA:BBBBBBCC

      break;
    case 7:			// ABBBBBBB:CDDDDDDD

      emitcode ("mov", "c,acc.7");	// c = A

      AccAXLrl1 (x);		// BBBBBBBC:DDDDDDDA

      emitcode ("xch", "a,%s", x);	// DDDDDDDA:BBBBBBCC

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 0000000A:BBBBBBBC

      break;
    default:
      break;
    }
}

/*-----------------------------------------------------------------*/
/* AccAXRshS - right shift signed a:x known count (0..7)           */
/*-----------------------------------------------------------------*/
static void
AccAXRshS (char *x, int shCount)
{
  symbol *tlbl;
  switch (shCount)
    {
    case 0:
      break;
    case 1:
      emitcode ("mov", "c,acc.7");
      AccAXRrl1 (x);		// s->a:x

      break;
    case 2:
      emitcode ("mov", "c,acc.7");
      AccAXRrl1 (x);		// s->a:x

      emitcode ("mov", "c,acc.7");
      AccAXRrl1 (x);		// s->a:x

      break;
    case 3:
    case 4:
    case 5:			// AAAAABBB:CCCCCDDD = a:x

      tlbl = newiTempLabel (NULL);
      AccRol (8 - shCount);	// BBBAAAAA:CCCCCDDD

      emitcode ("xch", "a,%s", x);	// CCCCCDDD:BBBAAAAA

      AccRol (8 - shCount);	// DDDCCCCC:BBBAAAAA

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 000CCCCC:BBBAAAAA

      emitcode ("xrl", "a,%s", x);	// BBB(CCCCC^AAAAA):BBBAAAAA

      emitcode ("xch", "a,%s", x);	// BBBAAAAA:BBB(CCCCC^AAAAA)

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 000AAAAA:BBB(CCCCC^AAAAA)

      emitcode ("xch", "a,%s", x);	// BBB(CCCCC^AAAAA):000AAAAA

      emitcode ("xrl", "a,%s", x);	// BBBCCCCC:000AAAAA

      emitcode ("xch", "a,%s", x);	// 000SAAAA:BBBCCCCC

      emitcode ("jnb", "acc.%d,%05d$", 7 - shCount, tlbl->key + 100);
      emitcode ("orl", "a,#0x%02x",
		(unsigned char) ~SRMask[shCount]);	// 111AAAAA:BBBCCCCC

      emitcode ("", "%05d$:", tlbl->key + 100);
      break;			// SSSSAAAA:BBBCCCCC

    case 6:			// AABBBBBB:CCDDDDDD

      tlbl = newiTempLabel (NULL);
      emitcode ("mov", "c,acc.7");
      AccAXLrl1 (x);		// ABBBBBBC:CDDDDDDA

      emitcode ("mov", "c,acc.7");
      AccAXLrl1 (x);		// BBBBBBCC:DDDDDDAA

      emitcode ("xch", "a,%s", x);	// DDDDDDAA:BBBBBBCC

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 000000AA:BBBBBBCC

      emitcode ("jnb", "acc.%d,%05d$", 7 - shCount, tlbl->key + 100);
      emitcode ("orl", "a,#0x%02x",
		(unsigned char) ~SRMask[shCount]);	// 111111AA:BBBBBBCC

      emitcode ("", "%05d$:", tlbl->key + 100);
      break;
    case 7:			// ABBBBBBB:CDDDDDDD

      tlbl = newiTempLabel (NULL);
      emitcode ("mov", "c,acc.7");	// c = A

      AccAXLrl1 (x);		// BBBBBBBC:DDDDDDDA

      emitcode ("xch", "a,%s", x);	// DDDDDDDA:BBBBBBCC

      emitcode ("anl", "a,#0x%02x",
		SRMask[shCount]);	// 0000000A:BBBBBBBC

      emitcode ("jnb", "acc.%d,%05d$", 7 - shCount, tlbl->key + 100);
      emitcode ("orl", "a,#0x%02x",
		(unsigned char) ~SRMask[shCount]);	// 1111111A:BBBBBBBC

      emitcode ("", "%05d$:", tlbl->key + 100);
      break;
    default:
      break;
    }
}

/*-----------------------------------------------------------------*/
/* shiftL2Left2Result - shift left two bytes from left to result   */
/*-----------------------------------------------------------------*/
static void
shiftL2Left2Result (operand * left, int offl,
		    operand * result, int offr, int shCount)
{
  if (sameRegs (AOP (result), AOP (left)) &&
      ((offl + MSB16) == offr))
    {
      /* don't crash result[offr] */
      MOVA (aopGet (AOP (left), offl, FALSE, FALSE));
      emitcode ("xch", "a,%s", aopGet (AOP (left), offl + MSB16, FALSE, FALSE));
    }
  else
    {
      movLeft2Result (left, offl, result, offr, 0);
      MOVA (aopGet (AOP (left), offl + MSB16, FALSE, FALSE));
    }
  /* ax << shCount (x = lsb(result)) */
  AccAXLsh (aopGet (AOP (result), offr, FALSE, FALSE), shCount);
  aopPut (AOP (result), "a", offr + MSB16, isOperandVolatile (result, FALSE));
}


/*-----------------------------------------------------------------*/
/* shiftR2Left2Result - shift right two bytes from left to result  */
/*-----------------------------------------------------------------*/
static void
shiftR2Left2Result (operand * left, int offl,
		    operand * result, int offr,
		    int shCount, int sign)
{
  if (sameRegs (AOP (result), AOP (left)) &&
      ((offl + MSB16) == offr))
    {
      /* don't crash result[offr] */
      MOVA (aopGet (AOP (left), offl, FALSE, FALSE));
      emitcode ("xch", "a,%s", aopGet (AOP (left), offl + MSB16, FALSE, FALSE));
    }
  else
    {
      movLeft2Result (left, offl, result, offr, 0);
      MOVA (aopGet (AOP (left), offl + MSB16, FALSE, FALSE));
    }
  /* a:x >> shCount (x = lsb(result)) */
  if (sign)
    AccAXRshS (aopGet (AOP (result), offr, FALSE, FALSE), shCount);
  else
    AccAXRsh (aopGet (AOP (result), offr, FALSE, FALSE), shCount);
  if (getDataSize (result) > 1)
    aopPut (AOP (result), "a", offr + MSB16, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* shiftLLeftOrResult - shift left one byte from left, or to result */
/*-----------------------------------------------------------------*/
static void
shiftLLeftOrResult (operand * left, int offl,
		    operand * result, int offr, int shCount)
{
  MOVA (aopGet (AOP (left), offl, FALSE, FALSE));
  /* shift left accumulator */
  AccLsh (shCount);
  /* or with result */
  emitcode ("orl", "a,%s", aopGet (AOP (result), offr, FALSE, FALSE));
  /* back to result */
  aopPut (AOP (result), "a", offr, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* shiftRLeftOrResult - shift right one byte from left,or to result */
/*-----------------------------------------------------------------*/
static void
shiftRLeftOrResult (operand * left, int offl,
		    operand * result, int offr, int shCount)
{
  MOVA (aopGet (AOP (left), offl, FALSE, FALSE));
  /* shift right accumulator */
  AccRsh (shCount);
  /* or with result */
  emitcode ("orl", "a,%s", aopGet (AOP (result), offr, FALSE, FALSE));
  /* back to result */
  aopPut (AOP (result), "a", offr, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* genlshOne - left shift a one byte quantity by known count       */
/*-----------------------------------------------------------------*/
static void
genlshOne (operand * result, operand * left, int shCount)
{
  D(emitcode (";     genlshOne",""));

  shiftL1Left2Result (left, LSB, result, LSB, shCount);
}

/*-----------------------------------------------------------------*/
/* genlshTwo - left shift two bytes by known amount != 0           */
/*-----------------------------------------------------------------*/
static void
genlshTwo (operand * result, operand * left, int shCount)
{
  int size;

  D(emitcode (";     genlshTwo",""));

  size = getDataSize (result);

  /* if shCount >= 8 */
  if (shCount >= 8)
    {
      shCount -= 8;

      if (size > 1)
	{
	  if (shCount)
	    shiftL1Left2Result (left, LSB, result, MSB16, shCount);
	  else
	    movLeft2Result (left, LSB, result, MSB16, 0);
	}
      aopPut (AOP (result), zero, LSB, isOperandVolatile (result, FALSE));
    }

  /*  1 <= shCount <= 7 */
  else
    {
      if (size == 1)
	shiftL1Left2Result (left, LSB, result, LSB, shCount);
      else
	shiftL2Left2Result (left, LSB, result, LSB, shCount);
    }
}

/*-----------------------------------------------------------------*/
/* shiftLLong - shift left one long from left to result            */
/* offl = LSB or MSB16                                             */
/*-----------------------------------------------------------------*/
static void
shiftLLong (operand * left, operand * result, int offr)
{
  char *l;
  int size = AOP_SIZE (result);

  if (size >= LSB + offr)
    {
      l = aopGet (AOP (left), LSB, FALSE, FALSE);
      MOVA (l);
      emitcode ("add", "a,acc");
      if (sameRegs (AOP (left), AOP (result)) &&
	  size >= MSB16 + offr && offr != LSB)
	emitcode ("xch", "a,%s",
		  aopGet (AOP (left), LSB + offr, FALSE, FALSE));
      else
	aopPut (AOP (result), "a", LSB + offr, isOperandVolatile (result, FALSE));
    }

  if (size >= MSB16 + offr)
    {
      if (!(sameRegs (AOP (result), AOP (left)) && size >= MSB16 + offr && offr != LSB))
	{
	  l = aopGet (AOP (left), MSB16, FALSE, FALSE);
	  MOVA (l);
	}
      emitcode ("rlc", "a");
      if (sameRegs (AOP (left), AOP (result)) &&
	  size >= MSB24 + offr && offr != LSB)
	emitcode ("xch", "a,%s",
		  aopGet (AOP (left), MSB16 + offr, FALSE, FALSE));
      else
	aopPut (AOP (result), "a", MSB16 + offr, isOperandVolatile (result, FALSE));
    }

  if (size >= MSB24 + offr)
    {
      if (!(sameRegs (AOP (left), AOP (left)) && size >= MSB24 + offr && offr != LSB))
	{
	  l = aopGet (AOP (left), MSB24, FALSE, FALSE);
	  MOVA (l);
	}
      emitcode ("rlc", "a");
      if (sameRegs (AOP (left), AOP (result)) &&
	  size >= MSB32 + offr && offr != LSB)
	emitcode ("xch", "a,%s",
		  aopGet (AOP (left), MSB24 + offr, FALSE, FALSE));
      else
	aopPut (AOP (result), "a", MSB24 + offr, isOperandVolatile (result, FALSE));
    }

  if (size > MSB32 + offr)
    {
      if (!(sameRegs (AOP (result), AOP (left)) && size >= MSB32 + offr && offr != LSB))
	{
	  l = aopGet (AOP (left), MSB32, FALSE, FALSE);
	  MOVA (l);
	}
      emitcode ("rlc", "a");
      aopPut (AOP (result), "a", MSB32 + offr, isOperandVolatile (result, FALSE));
    }
  if (offr != LSB)
    aopPut (AOP (result), zero, LSB, isOperandVolatile (result, FALSE));
}

/*-----------------------------------------------------------------*/
/* genlshFour - shift four byte by a known amount != 0             */
/*-----------------------------------------------------------------*/
static void
genlshFour (operand * result, operand * left, int shCount)
{
  int size;

  D(emitcode (";     genlshFour",""));

  size = AOP_SIZE (result);

  /* if shifting more that 3 bytes */
  if (shCount >= 24)
    {
      shCount -= 24;
      if (shCount)
	/* lowest order of left goes to the highest
	   order of the destination */
	shiftL1Left2Result (left, LSB, result, MSB32, shCount);
      else
	movLeft2Result (left, LSB, result, MSB32, 0);
      aopPut (AOP (result), zero, LSB, isOperandVolatile (result, FALSE));
      aopPut (AOP (result), zero, MSB16, isOperandVolatile (result, FALSE));
      aopPut (AOP (result), zero, MSB24, isOperandVolatile (result, FALSE));
      return;
    }

  /* more than two bytes */
  else if (shCount >= 16)
    {
      /* lower order two bytes goes to higher order two bytes */
      shCount -= 16;
      /* if some more remaining */
      if (shCount)
	shiftL2Left2Result (left, LSB, result, MSB24, shCount);
      else
	{
	  movLeft2Result (left, MSB16, result, MSB32, 0);
	  movLeft2Result (left, LSB, result, MSB24, 0);
	}
      aopPut (AOP (result), zero, MSB16, isOperandVolatile (result, FALSE));
      aopPut (AOP (result), zero, LSB, isOperandVolatile (result, FALSE));
      return;
    }

  /* if more than 1 byte */
  else if (shCount >= 8)
    {
      /* lower order three bytes goes to higher order  three bytes */
      shCount -= 8;
      if (size == 2)
	{
	  if (shCount)
	    shiftL1Left2Result (left, LSB, result, MSB16, shCount);
	  else
	    movLeft2Result (left, LSB, result, MSB16, 0);
	}
      else
	{			/* size = 4 */
	  if (shCount == 0)
	    {
	      movLeft2Result (left, MSB24, result, MSB32, 0);
	      movLeft2Result (left, MSB16, result, MSB24, 0);
	      movLeft2Result (left, LSB, result, MSB16, 0);
	      aopPut (AOP (result), zero, LSB, isOperandVolatile (result, FALSE));
	    }
	  else if (shCount == 1)
	    shiftLLong (left, result, MSB16);
	  else
	    {
	      shiftL2Left2Result (left, MSB16, result, MSB24, shCount);
	      shiftL1Left2Result (left, LSB, result, MSB16, shCount);
	      shiftRLeftOrResult (left, LSB, result, MSB24, 8 - shCount);
	      aopPut (AOP (result), zero, LSB, isOperandVolatile (result, FALSE));
	    }
	}
    }

  /* 1 <= shCount <= 7 */
  else if (shCount <= 2)
    {
      shiftLLong (left, result, LSB);
      if (shCount == 2)
	shiftLLong (result, result, LSB);
    }
  /* 3 <= shCount <= 7, optimize */
  else
    {
      shiftL2Left2Result (left, MSB24, result, MSB24, shCount);
      shiftRLeftOrResult (left, MSB16, result, MSB24, 8 - shCount);
      shiftL2Left2Result (left, LSB, result, LSB, shCount);
    }
}

/*-----------------------------------------------------------------*/
/* genLeftShiftLiteral - left shifting by known count              */
/*-----------------------------------------------------------------*/
static void
genLeftShiftLiteral (operand * left,
		     operand * right,
		     operand * result,
		     iCode * ic)
{
  int shCount = (int) floatFromVal (AOP (right)->aopu.aop_lit);
  int size;

  D(emitcode (";     genLeftShiftLiteral",""));

  freeAsmop (right, NULL, ic, TRUE);

  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  size = getSize (operandType (result));

#if VIEW_SIZE
  emitcode ("; shift left ", "result %d, left %d", size,
	    AOP_SIZE (left));
#endif

  /* I suppose that the left size >= result size */
  if (shCount == 0)
    {
      while (size--)
	{
	  movLeft2Result (left, size, result, size, 0);
	}
    }

  else if (shCount >= (size * 8))
    while (size--)
      aopPut (AOP (result), zero, size, isOperandVolatile (result, FALSE));
  else
    {
      switch (size)
	{
	case 1:
	  genlshOne (result, left, shCount);
	  break;

	case 2:
	  genlshTwo (result, left, shCount);
	  break;

	case 4:
	  genlshFour (result, left, shCount);
	  break;
	default:
	  werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
		  "*** ack! mystery literal shift!\n");
	  break;
	}
    }
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genLeftShift - generates code for left shifting                 */
/*-----------------------------------------------------------------*/
static void
genLeftShift (iCode * ic)
{
  operand *left, *right, *result;
  int size, offset;
  char *l;
  symbol *tlbl, *tlbl1;

  D(emitcode (";     genLeftShift",""));

  right = IC_RIGHT (ic);
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);

  aopOp (right, ic, FALSE);

  /* if the shift count is known then do it
     as efficiently as possible */
  if (AOP_TYPE (right) == AOP_LIT)
    {
      genLeftShiftLiteral (left, right, result, ic);
      return;
    }

  /* shift count is unknown then we have to form
     a loop get the loop count in B : Note: we take
     only the lower order byte since shifting
     more that 32 bits make no sense anyway, ( the
     largest size of an object can be only 32 bits ) */

  emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
  emitcode ("inc", "b");
  freeAsmop (right, NULL, ic, TRUE);
  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  /* now move the left to the result if they are not the
     same */
  if (!sameRegs (AOP (left), AOP (result)) &&
      AOP_SIZE (result) > 1)
    {

      size = AOP_SIZE (result);
      offset = 0;
      while (size--)
	{
	  l = aopGet (AOP (left), offset, FALSE, TRUE);
	  if (*l == '@' && (IS_AOP_PREG (result)))
	    {

	      emitcode ("mov", "a,%s", l);
	      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
	    }
	  else
	    aopPut (AOP (result), l, offset, isOperandVolatile (result, FALSE));
	  offset++;
	}
    }

  tlbl = newiTempLabel (NULL);
  size = AOP_SIZE (result);
  offset = 0;
  tlbl1 = newiTempLabel (NULL);

  /* if it is only one byte then */
  if (size == 1)
    {
      symbol *tlbl1 = newiTempLabel (NULL);

      l = aopGet (AOP (left), 0, FALSE, FALSE);
      MOVA (l);
      emitcode ("sjmp", "%05d$", tlbl1->key + 100);
      emitcode ("", "%05d$:", tlbl->key + 100);
      emitcode ("add", "a,acc");
      emitcode ("", "%05d$:", tlbl1->key + 100);
      emitcode ("djnz", "b,%05d$", tlbl->key + 100);
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      goto release;
    }

  reAdjustPreg (AOP (result));

  emitcode ("sjmp", "%05d$", tlbl1->key + 100);
  emitcode ("", "%05d$:", tlbl->key + 100);
  l = aopGet (AOP (result), offset, FALSE, FALSE);
  MOVA (l);
  emitcode ("add", "a,acc");
  aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
  while (--size)
    {
      l = aopGet (AOP (result), offset, FALSE, FALSE);
      MOVA (l);
      emitcode ("rlc", "a");
      aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
    }
  reAdjustPreg (AOP (result));

  emitcode ("", "%05d$:", tlbl1->key + 100);
  emitcode ("djnz", "b,%05d$", tlbl->key + 100);
release:
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genrshOne - right shift a one byte quantity by known count      */
/*-----------------------------------------------------------------*/
static void
genrshOne (operand * result, operand * left,
	   int shCount, int sign)
{
  D(emitcode (";     genrshOne",""));

  shiftR1Left2Result (left, LSB, result, LSB, shCount, sign);
}

/*-----------------------------------------------------------------*/
/* genrshTwo - right shift two bytes by known amount != 0          */
/*-----------------------------------------------------------------*/
static void
genrshTwo (operand * result, operand * left,
	   int shCount, int sign)
{
  D(emitcode (";     genrshTwo",""));

  /* if shCount >= 8 */
  if (shCount >= 8)
    {
      shCount -= 8;
      if (shCount)
	shiftR1Left2Result (left, MSB16, result, LSB,
			    shCount, sign);
      else
	movLeft2Result (left, MSB16, result, LSB, sign);
      addSign (result, MSB16, sign);
    }

  /*  1 <= shCount <= 7 */
  else
    shiftR2Left2Result (left, LSB, result, LSB, shCount, sign);
}

/*-----------------------------------------------------------------*/
/* shiftRLong - shift right one long from left to result           */
/* offl = LSB or MSB16                                             */
/*-----------------------------------------------------------------*/
static void
shiftRLong (operand * left, int offl,
	    operand * result, int sign)
{
  int isSameRegs=sameRegs(AOP(left),AOP(result));

  if (isSameRegs && offl>1) {
    // we are in big trouble, but this shouldn't happen
    werror(E_INTERNAL_ERROR, __FILE__, __LINE__);
  }

  MOVA (aopGet (AOP (left), MSB32, FALSE, FALSE));

  if (offl==MSB16) {
    // shift is > 8
    if (sign) {
      emitcode ("rlc", "a");
      emitcode ("subb", "a,acc");
      if (isSameRegs)
        emitcode ("xch", "a,%s", aopGet(AOP(left), MSB32, FALSE, FALSE));
      else {
        aopPut (AOP (result), "a", MSB32, isOperandVolatile (result, FALSE));
        MOVA (aopGet (AOP (left), MSB32, FALSE, FALSE));
      }
    } else {
      aopPut (AOP(result), zero, MSB32, isOperandVolatile (result, FALSE));
    }
  }

  if (!sign) {
    emitcode ("clr", "c");
  } else {
    emitcode ("mov", "c,acc.7");
  }

  emitcode ("rrc", "a");

  if (isSameRegs && offl==MSB16) {
    emitcode ("xch", "a,%s",aopGet (AOP (left), MSB24, FALSE, FALSE));
  } else {
    aopPut (AOP (result), "a", MSB32-offl, isOperandVolatile (result, FALSE));
    MOVA (aopGet (AOP (left), MSB24, FALSE, FALSE));
  }

  emitcode ("rrc", "a");
  if (isSameRegs && offl==1) {
    emitcode ("xch", "a,%s",aopGet (AOP (left), MSB16, FALSE, FALSE));
  } else {
    aopPut (AOP (result), "a", MSB24-offl, isOperandVolatile (result, FALSE));
    MOVA (aopGet (AOP (left), MSB16, FALSE, FALSE));
  }
  emitcode ("rrc", "a");
  aopPut (AOP (result), "a", MSB16 - offl, isOperandVolatile (result, FALSE));

  if (offl == LSB)
    {
      MOVA (aopGet (AOP (left), LSB, FALSE, FALSE));
      emitcode ("rrc", "a");
      aopPut (AOP (result), "a", LSB, isOperandVolatile (result, FALSE));
    }
}

/*-----------------------------------------------------------------*/
/* genrshFour - shift four byte by a known amount != 0             */
/*-----------------------------------------------------------------*/
static void
genrshFour (operand * result, operand * left,
	    int shCount, int sign)
{
  D(emitcode (";     genrshFour",""));

  /* if shifting more that 3 bytes */
  if (shCount >= 24)
    {
      shCount -= 24;
      if (shCount)
	shiftR1Left2Result (left, MSB32, result, LSB, shCount, sign);
      else
	movLeft2Result (left, MSB32, result, LSB, sign);
      addSign (result, MSB16, sign);
    }
  else if (shCount >= 16)
    {
      shCount -= 16;
      if (shCount)
	shiftR2Left2Result (left, MSB24, result, LSB, shCount, sign);
      else
	{
	  movLeft2Result (left, MSB24, result, LSB, 0);
	  movLeft2Result (left, MSB32, result, MSB16, sign);
	}
      addSign (result, MSB24, sign);
    }
  else if (shCount >= 8)
    {
      shCount -= 8;
      if (shCount == 1)
	shiftRLong (left, MSB16, result, sign);
      else if (shCount == 0)
	{
	  movLeft2Result (left, MSB16, result, LSB, 0);
	  movLeft2Result (left, MSB24, result, MSB16, 0);
	  movLeft2Result (left, MSB32, result, MSB24, sign);
	  addSign (result, MSB32, sign);
	}
      else
	{
	  shiftR2Left2Result (left, MSB16, result, LSB, shCount, 0);
	  shiftLLeftOrResult (left, MSB32, result, MSB16, 8 - shCount);
	  /* the last shift is signed */
	  shiftR1Left2Result (left, MSB32, result, MSB24, shCount, sign);
	  addSign (result, MSB32, sign);
	}
    }
  else
    {				/* 1 <= shCount <= 7 */
      if (shCount <= 2)
	{
	  shiftRLong (left, LSB, result, sign);
	  if (shCount == 2)
	    shiftRLong (result, LSB, result, sign);
	}
      else
	{
	  shiftR2Left2Result (left, LSB, result, LSB, shCount, 0);
	  shiftLLeftOrResult (left, MSB24, result, MSB16, 8 - shCount);
	  shiftR2Left2Result (left, MSB24, result, MSB24, shCount, sign);
	}
    }
}

/*-----------------------------------------------------------------*/
/* genRightShiftLiteral - right shifting by known count            */
/*-----------------------------------------------------------------*/
static void
genRightShiftLiteral (operand * left,
		      operand * right,
		      operand * result,
		      iCode * ic,
		      int sign)
{
  int shCount = (int) floatFromVal (AOP (right)->aopu.aop_lit);
  int size;

  D(emitcode (";     genRightShiftLiteral",""));

  freeAsmop (right, NULL, ic, TRUE);

  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

#if VIEW_SIZE
  emitcode ("; shift right ", "result %d, left %d", AOP_SIZE (result),
	    AOP_SIZE (left));
#endif

  size = getDataSize (left);
  /* test the LEFT size !!! */

  /* I suppose that the left size >= result size */
  if (shCount == 0)
    {
      size = getDataSize (result);
      while (size--)
	movLeft2Result (left, size, result, size, 0);
    }

  else if (shCount >= (size * 8))
    {
      if (sign) {
	/* get sign in acc.7 */
	MOVA (aopGet (AOP (left), size - 1, FALSE, FALSE));
      }
      addSign (result, LSB, sign);
    }
  else
    {
      switch (size)
	{
	case 1:
	  genrshOne (result, left, shCount, sign);
	  break;

	case 2:
	  genrshTwo (result, left, shCount, sign);
	  break;

	case 4:
	  genrshFour (result, left, shCount, sign);
	  break;
	default:
	  break;
	}
    }
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genSignedRightShift - right shift of signed number              */
/*-----------------------------------------------------------------*/
static void
genSignedRightShift (iCode * ic)
{
  operand *right, *left, *result;
  int size, offset;
  char *l;
  symbol *tlbl, *tlbl1;

  D(emitcode (";     genSignedRightShift",""));

  /* we do it the hard way put the shift count in b
     and loop thru preserving the sign */

  right = IC_RIGHT (ic);
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);

  aopOp (right, ic, FALSE);


  if (AOP_TYPE (right) == AOP_LIT)
    {
      genRightShiftLiteral (left, right, result, ic, 1);
      return;
    }
  /* shift count is unknown then we have to form
     a loop get the loop count in B : Note: we take
     only the lower order byte since shifting
     more that 32 bits make no sense anyway, ( the
     largest size of an object can be only 32 bits ) */

  emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
  emitcode ("inc", "b");
  freeAsmop (right, NULL, ic, TRUE);
  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  /* now move the left to the result if they are not the
     same */
  if (!sameRegs (AOP (left), AOP (result)) &&
      AOP_SIZE (result) > 1)
    {

      size = AOP_SIZE (result);
      offset = 0;
      while (size--)
	{
	  l = aopGet (AOP (left), offset, FALSE, TRUE);
	  if (*l == '@' && IS_AOP_PREG (result))
	    {

	      emitcode ("mov", "a,%s", l);
	      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
	    }
	  else
	    aopPut (AOP (result), l, offset, isOperandVolatile (result, FALSE));
	  offset++;
	}
    }

  /* mov the highest order bit to OVR */
  tlbl = newiTempLabel (NULL);
  tlbl1 = newiTempLabel (NULL);

  size = AOP_SIZE (result);
  offset = size - 1;
  emitcode ("mov", "a,%s", aopGet (AOP (left), offset, FALSE, FALSE));
  emitcode ("rlc", "a");
  emitcode ("mov", "ov,c");
  /* if it is only one byte then */
  if (size == 1)
    {
      l = aopGet (AOP (left), 0, FALSE, FALSE);
      MOVA (l);
      emitcode ("sjmp", "%05d$", tlbl1->key + 100);
      emitcode ("", "%05d$:", tlbl->key + 100);
      emitcode ("mov", "c,ov");
      emitcode ("rrc", "a");
      emitcode ("", "%05d$:", tlbl1->key + 100);
      emitcode ("djnz", "b,%05d$", tlbl->key + 100);
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      goto release;
    }

  reAdjustPreg (AOP (result));
  emitcode ("sjmp", "%05d$", tlbl1->key + 100);
  emitcode ("", "%05d$:", tlbl->key + 100);
  emitcode ("mov", "c,ov");
  while (size--)
    {
      l = aopGet (AOP (result), offset, FALSE, FALSE);
      MOVA (l);
      emitcode ("rrc", "a");
      aopPut (AOP (result), "a", offset--, isOperandVolatile (result, FALSE));
    }
  reAdjustPreg (AOP (result));
  emitcode ("", "%05d$:", tlbl1->key + 100);
  emitcode ("djnz", "b,%05d$", tlbl->key + 100);

release:
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genRightShift - generate code for right shifting                */
/*-----------------------------------------------------------------*/
static void
genRightShift (iCode * ic)
{
  operand *right, *left, *result;
  sym_link *letype;
  int size, offset;
  char *l;
  symbol *tlbl, *tlbl1;

  D(emitcode (";     genRightShift",""));

  /* if signed then we do it the hard way preserve the
     sign bit moving it inwards */
  letype = getSpec (operandType (IC_LEFT (ic)));

  if (!SPEC_USIGN (letype))
    {
      genSignedRightShift (ic);
      return;
    }

  /* signed & unsigned types are treated the same : i.e. the
     signed is NOT propagated inwards : quoting from the
     ANSI - standard : "for E1 >> E2, is equivalent to division
     by 2**E2 if unsigned or if it has a non-negative value,
     otherwise the result is implementation defined ", MY definition
     is that the sign does not get propagated */

  right = IC_RIGHT (ic);
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);

  aopOp (right, ic, FALSE);

  /* if the shift count is known then do it
     as efficiently as possible */
  if (AOP_TYPE (right) == AOP_LIT)
    {
      genRightShiftLiteral (left, right, result, ic, 0);
      return;
    }

  /* shift count is unknown then we have to form
     a loop get the loop count in B : Note: we take
     only the lower order byte since shifting
     more that 32 bits make no sense anyway, ( the
     largest size of an object can be only 32 bits ) */

  emitcode ("mov", "b,%s", aopGet (AOP (right), 0, FALSE, FALSE));
  emitcode ("inc", "b");
  freeAsmop (right, NULL, ic, TRUE);
  aopOp (left, ic, FALSE);
  aopOp (result, ic, FALSE);

  /* now move the left to the result if they are not the
     same */
  if (!sameRegs (AOP (left), AOP (result)) &&
      AOP_SIZE (result) > 1)
    {

      size = AOP_SIZE (result);
      offset = 0;
      while (size--)
	{
	  l = aopGet (AOP (left), offset, FALSE, TRUE);
	  if (*l == '@' && IS_AOP_PREG (result))
	    {

	      emitcode ("mov", "a,%s", l);
	      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
	    }
	  else
	    aopPut (AOP (result), l, offset, isOperandVolatile (result, FALSE));
	  offset++;
	}
    }

  tlbl = newiTempLabel (NULL);
  tlbl1 = newiTempLabel (NULL);
  size = AOP_SIZE (result);
  offset = size - 1;

  /* if it is only one byte then */
  if (size == 1)
    {
      l = aopGet (AOP (left), 0, FALSE, FALSE);
      MOVA (l);
      emitcode ("sjmp", "%05d$", tlbl1->key + 100);
      emitcode ("", "%05d$:", tlbl->key + 100);
      CLRC;
      emitcode ("rrc", "a");
      emitcode ("", "%05d$:", tlbl1->key + 100);
      emitcode ("djnz", "b,%05d$", tlbl->key + 100);
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      goto release;
    }

  reAdjustPreg (AOP (result));
  emitcode ("sjmp", "%05d$", tlbl1->key + 100);
  emitcode ("", "%05d$:", tlbl->key + 100);
  CLRC;
  while (size--)
    {
      l = aopGet (AOP (result), offset, FALSE, FALSE);
      MOVA (l);
      emitcode ("rrc", "a");
      aopPut (AOP (result), "a", offset--, isOperandVolatile (result, FALSE));
    }
  reAdjustPreg (AOP (result));

  emitcode ("", "%05d$:", tlbl1->key + 100);
  emitcode ("djnz", "b,%05d$", tlbl->key + 100);

release:
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* emitPtrByteGet - emits code to get a byte into A through a      */
/*                  pointer register (R0, R1, or DPTR). The        */
/*                  original value of A can be preserved in B.     */
/*-----------------------------------------------------------------*/
static void
emitPtrByteGet (char *rname, int p_type, bool preserveAinB)
{
  switch (p_type)
    {
    case IPOINTER:
    case POINTER:
      if (preserveAinB)
        emitcode ("mov", "b,a");
      emitcode ("mov", "a,@%s", rname);
      break;

    case PPOINTER:
      if (preserveAinB)
        emitcode ("mov", "b,a");
      emitcode ("movx", "a,@%s", rname);
      break;

    case FPOINTER:
      if (preserveAinB)
        emitcode ("mov", "b,a");
      emitcode ("movx", "a,@dptr");
      break;

    case CPOINTER:
      if (preserveAinB)
        emitcode ("mov", "b,a");
      emitcode ("clr", "a");
      emitcode ("movc", "a,@a+dptr");
      break;

    case GPOINTER:
      if (preserveAinB)
        {
          emitcode ("push", "b");
          emitcode ("push", "acc");
        }
      emitcode ("lcall", "__gptrget");
      if (preserveAinB)
        emitcode ("pop", "b");
      break;
    }
}

/*-----------------------------------------------------------------*/
/* emitPtrByteSet - emits code to set a byte from src through a    */
/*                  pointer register (R0, R1, or DPTR).            */
/*-----------------------------------------------------------------*/
static void
emitPtrByteSet (char *rname, int p_type, char *src)
{
  switch (p_type)
    {
    case IPOINTER:
    case POINTER:
      if (*src=='@')
        {
          MOVA (src);
          emitcode ("mov", "@%s,a", rname);
        }
      else
        emitcode ("mov", "@%s,%s", rname, src);
      break;

    case PPOINTER:
      MOVA (src);
      emitcode ("movx", "@%s,a", rname);
      break;

    case FPOINTER:
      MOVA (src);
      emitcode ("movx", "@dptr,a");
      break;

    case GPOINTER:
      MOVA (src);
      emitcode ("lcall", "__gptrput");
      break;
    }
}

/*-----------------------------------------------------------------*/
/* genUnpackBits - generates code for unpacking bits               */
/*-----------------------------------------------------------------*/
static void
genUnpackBits (operand * result, char *rname, int ptype)
{
  int offset = 0;	/* result byte offset */
  int rsize;		/* result size */
  int rlen = 0;		/* remaining bitfield length */
  sym_link *etype;	/* bitfield type information */
  int blen;		/* bitfield length */
  int bstr;		/* bitfield starting bit within byte */

  D(emitcode (";     genUnpackBits",""));

  etype = getSpec (operandType (result));
  rsize = getSize (operandType (result));
  blen = SPEC_BLEN (etype);
  bstr = SPEC_BSTR (etype);

  /* If the bitfield length is less than a byte */
  if (blen < 8)
    {
      emitPtrByteGet (rname, ptype, FALSE);
      AccRsh (bstr);
      emitcode ("anl", "a,#0x%02x", ((unsigned char) -1) >> (8 - blen));
      aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
      goto finish;
    }

  /* Bit field did not fit in a byte. Copy all
     but the partial byte at the end.  */
  for (rlen=blen;rlen>=8;rlen-=8)
    {
      emitPtrByteGet (rname, ptype, FALSE);
      aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
      if (rlen>8)
        emitcode ("inc", "%s", rname);
    }

  /* Handle the partial byte at the end */
  if (rlen)
    {
      emitPtrByteGet (rname, ptype, FALSE);
      emitcode ("anl", "a,#0x%02x", ((unsigned char) -1) >> (8-rlen));
      aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
    }

finish:
  if (offset < rsize)
    {
      rsize -= offset;
      while (rsize--)
	aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
    }
}


/*-----------------------------------------------------------------*/
/* genDataPointerGet - generates code when ptr offset is known     */
/*-----------------------------------------------------------------*/
static void
genDataPointerGet (operand * left,
		   operand * result,
		   iCode * ic)
{
  char *l;
  char buffer[256];
  int size, offset = 0;

  D(emitcode (";     genDataPointerGet",""));

  aopOp (result, ic, TRUE);

  /* get the string representation of the name */
  l = aopGet (AOP (left), 0, FALSE, TRUE);
  size = AOP_SIZE (result);
  while (size--)
    {
      if (offset)
	sprintf (buffer, "(%s + %d)", l + 1, offset);
      else
	sprintf (buffer, "%s", l + 1);
      aopPut (AOP (result), buffer, offset++, isOperandVolatile (result, FALSE));
    }

  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genNearPointerGet - emitcode for near pointer fetch             */
/*-----------------------------------------------------------------*/
static void
genNearPointerGet (operand * left,
		   operand * result,
		   iCode * ic,
		   iCode * pi)
{
  asmop *aop = NULL;
  regs *preg = NULL;
  char *rname;
  sym_link *rtype, *retype;
  sym_link *ltype = operandType (left);
  char buffer[80];

  D(emitcode (";     genNearPointerGet",""));

  rtype = operandType (result);
  retype = getSpec (rtype);

  aopOp (left, ic, FALSE);

  /* if left is rematerialisable and
     result is not bitfield variable type and
     the left is pointer to data space i.e
     lower 128 bytes of space */
  if (AOP_TYPE (left) == AOP_IMMD &&
      !IS_BITFIELD (retype) &&
      DCL_TYPE (ltype) == POINTER)
    {
      genDataPointerGet (left, result, ic);
      return;
    }

 /* if the value is already in a pointer register
     then don't need anything more */
  if (!AOP_INPREG (AOP (left)))
    {
      if (IS_AOP_PREG (left))
	{
	  // Aha, it is a pointer, just in disguise.
	  rname = aopGet (AOP (left), 0, FALSE, FALSE);
	  if (*rname != '@')
	    {
	      fprintf(stderr, "probable internal error: unexpected rname @ %s:%d\n",
		      __FILE__, __LINE__);
	    }
	  else
	    {
	      // Expected case.
	      emitcode ("mov", "a%s,%s", rname + 1, rname);
	      rname++;  // skip the '@'.
	    }
	}
      else
	{
	  /* otherwise get a free pointer register */
	  aop = newAsmop (0);
	  preg = getFreePtr (ic, &aop, FALSE);
	  emitcode ("mov", "%s,%s",
		    preg->name,
		    aopGet (AOP (left), 0, FALSE, TRUE));
	  rname = preg->name;
	}
    }
  else
    rname = aopGet (AOP (left), 0, FALSE, FALSE);

  //aopOp (result, ic, FALSE);
  aopOp (result, ic, result?TRUE:FALSE);

  /* if bitfield then unpack the bits */
  if (IS_BITFIELD (retype))
    genUnpackBits (result, rname, POINTER);
  else
    {
      /* we have can just get the values */
      int size = AOP_SIZE (result);
      int offset = 0;

      while (size--)
	{
	  if (IS_AOP_PREG (result) || AOP_TYPE (result) == AOP_STK)
	    {

	      emitcode ("mov", "a,@%s", rname);
	      aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));
	    }
	  else
	    {
	      sprintf (buffer, "@%s", rname);
	      aopPut (AOP (result), buffer, offset, isOperandVolatile (result, FALSE));
	    }
	  offset++;
	  if (size || pi)
	    emitcode ("inc", "%s", rname);
	}
    }

  /* now some housekeeping stuff */
  if (aop)       /* we had to allocate for this iCode */
    {
      if (pi) { /* post increment present */
	aopPut(AOP ( left ),rname,0, isOperandVolatile (left, FALSE));
      }
      freeAsmop (NULL, aop, ic, RESULTONSTACK (ic) ? FALSE : TRUE);
    }
  else
    {
      /* we did not allocate which means left
         already in a pointer register, then
         if size > 0 && this could be used again
         we have to point it back to where it
         belongs */
      if ((AOP_SIZE (result) > 1 &&
	   !OP_SYMBOL (left)->remat &&
	   (OP_SYMBOL (left)->liveTo > ic->seq ||
	    ic->depth)) &&
	  !pi)
	{
	  int size = AOP_SIZE (result) - 1;
	  while (size--)
	    emitcode ("dec", "%s", rname);
	}
    }

  /* done */
  freeAsmop (result, NULL, ic, RESULTONSTACK (ic) ? FALSE : TRUE);
  freeAsmop (left, NULL, ic, TRUE);
  if (pi) pi->generated = 1;
}

/*-----------------------------------------------------------------*/
/* genPagedPointerGet - emitcode for paged pointer fetch           */
/*-----------------------------------------------------------------*/
static void
genPagedPointerGet (operand * left,
		    operand * result,
		    iCode * ic,
		    iCode *pi)
{
  asmop *aop = NULL;
  regs *preg = NULL;
  char *rname;
  sym_link *rtype, *retype;

  D(emitcode (";     genPagedPointerGet",""));

  rtype = operandType (result);
  retype = getSpec (rtype);

  aopOp (left, ic, FALSE);

  /* if the value is already in a pointer register
     then don't need anything more */
  if (!AOP_INPREG (AOP (left)))
    {
      /* otherwise get a free pointer register */
      aop = newAsmop (0);
      preg = getFreePtr (ic, &aop, FALSE);
      emitcode ("mov", "%s,%s",
		preg->name,
		aopGet (AOP (left), 0, FALSE, TRUE));
      rname = preg->name;
    }
  else
    rname = aopGet (AOP (left), 0, FALSE, FALSE);

  aopOp (result, ic, FALSE);

  /* if bitfield then unpack the bits */
  if (IS_BITFIELD (retype))
    genUnpackBits (result, rname, PPOINTER);
  else
    {
      /* we have can just get the values */
      int size = AOP_SIZE (result);
      int offset = 0;

      while (size--)
	{

	  emitcode ("movx", "a,@%s", rname);
	  aopPut (AOP (result), "a", offset, isOperandVolatile (result, FALSE));

	  offset++;

	  if (size || pi)
	    emitcode ("inc", "%s", rname);
	}
    }

  /* now some housekeeping stuff */
  if (aop) /* we had to allocate for this iCode */
    {
      if (pi) aopPut ( AOP (left), rname, 0, isOperandVolatile (left, FALSE));
      freeAsmop (NULL, aop, ic, TRUE);
    }
  else
    {
      /* we did not allocate which means left
         already in a pointer register, then
         if size > 0 && this could be used again
         we have to point it back to where it
         belongs */
      if ((AOP_SIZE (result) > 1 &&
	   !OP_SYMBOL (left)->remat &&
	   (OP_SYMBOL (left)->liveTo > ic->seq ||
	    ic->depth)) &&
	  !pi)
	{
	  int size = AOP_SIZE (result) - 1;
	  while (size--)
	    emitcode ("dec", "%s", rname);
	}
    }

  /* done */
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
  if (pi) pi->generated = 1;

}

/*--------------------------------------------------------------------*/
/* loadDptrFromOperand - load dptr (and optionally B) from operand op */
/*--------------------------------------------------------------------*/
static void
loadDptrFromOperand (operand *op, bool loadBToo)
{
  if (AOP_TYPE (op) != AOP_STR)
    {
      /* if this is remateriazable */
      if (AOP_TYPE (op) == AOP_IMMD)
	{
	  emitcode ("mov", "dptr,%s", aopGet (AOP (op), 0, TRUE, FALSE));
          if (loadBToo)
            {
	      if (AOP(op)->aopu.aop_immd.from_cast_remat)
		emitcode ("mov", "b,%s",aopGet(AOP (op), AOP_SIZE(op)-1, FALSE, FALSE));
	      else
                {
                  wassertl(FALSE, "need pointerCode");
                  emitcode ("", "; mov b,???");
                  /* genPointerGet and genPointerSet originally did different
                  ** things for this case. Both seem wrong.
                  ** from genPointerGet:
		  **  emitcode ("mov", "b,#%d", pointerCode (retype));
                  ** from genPointerSet:
		  **  emitcode ("mov", "b,%s + 1", aopGet (AOP (result), 0, TRUE, FALSE));
                  */
                }
            }
	}
      else if (AOP_TYPE (op) == AOP_DPTR)
	{
	  if (loadBToo)
	    {
	      MOVA (aopGet (AOP (op), 0, FALSE, FALSE));
	      emitcode ("push", "acc");
	      MOVA (aopGet (AOP (op), 1, FALSE, FALSE));
	      emitcode ("push", "acc");
	      emitcode ("mov", "b,%s", aopGet (AOP (op), 2, FALSE, FALSE));
	      emitcode ("pop", "dph");
	      emitcode ("pop", "dpl");
	    }
	  else
	    {
	      MOVA (aopGet (AOP (op), 0, FALSE, FALSE));
	      emitcode ("push", "acc");
	      emitcode ("mov", "dph,%s", aopGet (AOP (op), 1, FALSE, FALSE));
	      emitcode ("pop", "dpl");
	    }
	}
      else
	{			/* we need to get it byte by byte */
	  emitcode ("mov", "dpl,%s", aopGet (AOP (op), 0, FALSE, FALSE));
	  emitcode ("mov", "dph,%s", aopGet (AOP (op), 1, FALSE, FALSE));
	  if (loadBToo)
	    emitcode ("mov", "b,%s", aopGet (AOP (op), 2, FALSE, FALSE));
	}
    }
}

/*-----------------------------------------------------------------*/
/* genFarPointerGet - gget value from far space                    */
/*-----------------------------------------------------------------*/
static void
genFarPointerGet (operand * left,
		  operand * result, iCode * ic, iCode * pi)
{
  int size, offset;
  sym_link *retype = getSpec (operandType (result));

  D(emitcode (";     genFarPointerGet",""));

  aopOp (left, ic, FALSE);
  loadDptrFromOperand (left, FALSE);

  /* so dptr now contains the address */
  aopOp (result, ic, FALSE);

  /* if bit then unpack */
  if (IS_BITFIELD (retype))
    genUnpackBits (result, "dptr", FPOINTER);
  else
    {
      size = AOP_SIZE (result);
      offset = 0;

      while (size--)
	{
	  emitcode ("movx", "a,@dptr");
	  aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
	  if (size || pi)
	    emitcode ("inc", "dptr");
	}
    }

  if (pi && AOP_TYPE (left) != AOP_IMMD && AOP_TYPE (left) != AOP_STR) {
    aopPut ( AOP (left), "dpl", 0, isOperandVolatile (left, FALSE));
    aopPut ( AOP (left), "dph", 1, isOperandVolatile (left, FALSE));
    pi->generated = 1;
  }
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genCodePointerGet - gget value from code space                  */
/*-----------------------------------------------------------------*/
static void
genCodePointerGet (operand * left,
		    operand * result, iCode * ic, iCode *pi)
{
  int size, offset;
  sym_link *retype = getSpec (operandType (result));

  D(emitcode (";     genCodePointerGet",""));

  aopOp (left, ic, FALSE);
  loadDptrFromOperand (left, FALSE);

  /* so dptr now contains the address */
  aopOp (result, ic, FALSE);

  /* if bit then unpack */
  if (IS_BITFIELD (retype))
    genUnpackBits (result, "dptr", CPOINTER);
  else
    {
      size = AOP_SIZE (result);
      offset = 0;

      while (size--)
	{
	  if (pi)
	    {
	      emitcode ("clr", "a");
	      emitcode ("movc", "a,@a+dptr");
	      aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
   	      emitcode ("inc", "dptr");
	    }
	  else
	    {
	      emitcode ("mov", "a,#0x%02x", offset);
	      emitcode ("movc", "a,@a+dptr");
	      aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
	    }
	}
    }

  if (pi && AOP_TYPE (left) != AOP_IMMD && AOP_TYPE (left) != AOP_STR) {
    aopPut ( AOP (left), "dpl", 0, isOperandVolatile (left, FALSE));
    aopPut ( AOP (left), "dph", 1, isOperandVolatile (left, FALSE));
    pi->generated = 1;
  }
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genGenPointerGet - gget value from generic pointer space        */
/*-----------------------------------------------------------------*/
static void
genGenPointerGet (operand * left,
		  operand * result, iCode * ic, iCode *pi)
{
  int size, offset;
  sym_link *retype = getSpec (operandType (result));

  D(emitcode (";     genGenPointerGet",""));

  aopOp (left, ic, FALSE);
  loadDptrFromOperand (left, TRUE);

  /* so dptr know contains the address */
  aopOp (result, ic, FALSE);

  /* if bit then unpack */
  if (IS_BITFIELD (retype))
    genUnpackBits (result, "dptr", GPOINTER);
  else
    {
      size = AOP_SIZE (result);
      offset = 0;

      while (size--)
	{
	  emitcode ("lcall", "__gptrget");
	  aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
	  if (size || pi)
	    emitcode ("inc", "dptr");
	}
    }

  if (pi && AOP_TYPE (left) != AOP_IMMD && AOP_TYPE (left) != AOP_STR) {
    aopPut ( AOP (left), "dpl", 0, isOperandVolatile (left, FALSE));
    aopPut ( AOP (left), "dph", 1, isOperandVolatile (left, FALSE));
    pi->generated = 1;
  }
  freeAsmop (left, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genPointerGet - generate code for pointer get                   */
/*-----------------------------------------------------------------*/
static void
genPointerGet (iCode * ic, iCode *pi)
{
  operand *left, *result;
  sym_link *type, *etype;
  int p_type;

  D(emitcode (";     genPointerGet",""));

  left = IC_LEFT (ic);
  result = IC_RESULT (ic);

  /* depending on the type of pointer we need to
     move it to the correct pointer register */
  type = operandType (left);
  etype = getSpec (type);
  /* if left is of type of pointer then it is simple */
  if (IS_PTR (type) && !IS_FUNC (type->next))
    p_type = DCL_TYPE (type);
  else
    {
      /* we have to go by the storage class */
      p_type = PTR_TYPE (SPEC_OCLS (etype));
    }

  /* special case when cast remat */
  if (p_type == GPOINTER && OP_SYMBOL(left)->remat &&
      IS_CAST_ICODE(OP_SYMBOL(left)->rematiCode)) {
	  left = IC_RIGHT(OP_SYMBOL(left)->rematiCode);
	  type = operandType (left);
	  p_type = DCL_TYPE (type);
  }
  /* now that we have the pointer type we assign
     the pointer values */
  switch (p_type)
    {

    case POINTER:
    case IPOINTER:
      genNearPointerGet (left, result, ic, pi);
      break;

    case PPOINTER:
      genPagedPointerGet (left, result, ic, pi);
      break;

    case FPOINTER:
      genFarPointerGet (left, result, ic, pi);
      break;

    case CPOINTER:
      genCodePointerGet (left, result, ic, pi);
      break;

    case GPOINTER:
      genGenPointerGet (left, result, ic, pi);
      break;
    }

}



/*-----------------------------------------------------------------*/
/* genPackBits - generates code for packed bit storage             */
/*-----------------------------------------------------------------*/
static void
genPackBits (sym_link * etype,
	     operand * right,
	     char *rname, int p_type)
{
  int offset = 0;	/* source byte offset */
  int rlen = 0;		/* remaining bitfield length */
  int blen;		/* bitfield length */
  int bstr;		/* bitfield starting bit within byte */
  int litval;		/* source literal value (if AOP_LIT) */
  unsigned char mask;	/* bitmask within current byte */

  D(emitcode (";     genPackBits",""));

  blen = SPEC_BLEN (etype);
  bstr = SPEC_BSTR (etype);

  /* If the bitfield length is less than a byte */
  if (blen < 8)
    {
      mask = ((unsigned char) (0xFF << (blen + bstr)) |
	      (unsigned char) (0xFF >> (8 - bstr)));

      if (AOP_TYPE (right) == AOP_LIT)
        {
          /* Case with a bitfield length <8 and literal source
          */
          litval = (int) floatFromVal (AOP (right)->aopu.aop_lit);
          litval <<= bstr;
          litval &= (~mask) & 0xff;
          emitPtrByteGet (rname, p_type, FALSE);
          if ((mask|litval)!=0xff)
            emitcode ("anl","a,#0x%02x", mask);
          if (litval)
            emitcode ("orl","a,#0x%02x", litval);
        }
      else
        {
          if ((blen==1) && (p_type!=GPOINTER))
            {
              /* Case with a bitfield length == 1 and no generic pointer
              */
              if (AOP_TYPE (right) == AOP_CRY)
                emitcode ("mov", "c,%s", AOP(right)->aopu.aop_dir);
              else
                {
                  MOVA (aopGet (AOP (right), 0, FALSE, FALSE));
                  emitcode ("rrc","a");
                }
              emitPtrByteGet (rname, p_type, FALSE);
              emitcode ("mov","acc.%d,c",bstr);
            }
          else
            {
              /* Case with a bitfield length < 8 and arbitrary source
              */
              MOVA (aopGet (AOP (right), 0, FALSE, FALSE));
              /* shift and mask source value */
              AccLsh (bstr);
              emitcode ("anl", "a,#0x%02x", (~mask) & 0xff);

    	      /* transfer A to B and get next byte */
              emitPtrByteGet (rname, p_type, TRUE);

              emitcode ("anl", "a,#0x%02x", mask);
              emitcode ("orl", "a,b");
              if (p_type == GPOINTER)
                emitcode ("pop", "b");
           }
        }

      emitPtrByteSet (rname, p_type, "a");
      return;
    }

  /* Bit length is greater than 7 bits. In this case, copy  */
  /* all except the partial byte at the end                 */
  for (rlen=blen;rlen>=8;rlen-=8)
    {
      emitPtrByteSet (rname, p_type,
                      aopGet (AOP (right), offset++, FALSE, TRUE) );
      if (rlen>8)
        emitcode ("inc", "%s", rname);
    }

  /* If there was a partial byte at the end */
  if (rlen)
    {
      mask = (((unsigned char) -1 << rlen) & 0xff);

      if (AOP_TYPE (right) == AOP_LIT)
        {
          /* Case with partial byte and literal source
          */
          litval = (int) floatFromVal (AOP (right)->aopu.aop_lit);
          litval >>= (blen-rlen);
          litval &= (~mask) & 0xff;
          emitPtrByteGet (rname, p_type, FALSE);
          if ((mask|litval)!=0xff)
            emitcode ("anl","a,#0x%02x", mask);
          if (litval)
            emitcode ("orl","a,#0x%02x", litval);
        }
      else
        {
          /* Case with partial byte and arbitrary source
          */
          MOVA (aopGet (AOP (right), offset++, FALSE, FALSE));
          emitcode ("anl", "a,#0x%02x", (~mask) & 0xff);

	  /* transfer A to B and get next byte */
          emitPtrByteGet (rname, p_type, TRUE);

          emitcode ("anl", "a,#0x%02x", mask);
          emitcode ("orl", "a,b");
          if (p_type == GPOINTER)
            emitcode ("pop", "b");
        }
      emitPtrByteSet (rname, p_type, "a");
    }

}


/*-----------------------------------------------------------------*/
/* genDataPointerSet - remat pointer to data space                 */
/*-----------------------------------------------------------------*/
static void
genDataPointerSet (operand * right,
		   operand * result,
		   iCode * ic)
{
  int size, offset = 0;
  char *l, buffer[256];

  D(emitcode (";     genDataPointerSet",""));

  aopOp (right, ic, FALSE);

  l = aopGet (AOP (result), 0, FALSE, TRUE);
  size = AOP_SIZE (right);
  while (size--)
    {
      if (offset)
	sprintf (buffer, "(%s + %d)", l + 1, offset);
      else
	sprintf (buffer, "%s", l + 1);
      emitcode ("mov", "%s,%s", buffer,
		aopGet (AOP (right), offset++, FALSE, FALSE));
    }

  freeAsmop (right, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genNearPointerSet - emitcode for near pointer put                */
/*-----------------------------------------------------------------*/
static void
genNearPointerSet (operand * right,
		   operand * result,
		   iCode * ic,
		   iCode * pi)
{
  asmop *aop = NULL;
  regs *preg = NULL;
  char *rname, *l;
  sym_link *retype, *letype;
  sym_link *ptype = operandType (result);

  D(emitcode (";     genNearPointerSet",""));

  retype = getSpec (operandType (right));
  letype = getSpec (ptype);
  aopOp (result, ic, FALSE);

  /* if the result is rematerializable &
     in data space & not a bit variable */
  if (AOP_TYPE (result) == AOP_IMMD &&
      DCL_TYPE (ptype) == POINTER &&
      !IS_BITVAR (retype) &&
      !IS_BITVAR (letype))
    {
      genDataPointerSet (right, result, ic);
      return;
    }

  /* if the value is already in a pointer register
     then don't need anything more */
  if (!AOP_INPREG (AOP (result)))
    {
	if (
	    //AOP_TYPE (result) == AOP_STK
	    IS_AOP_PREG(result)
	    )
	{
	    // Aha, it is a pointer, just in disguise.
	    rname = aopGet (AOP (result), 0, FALSE, FALSE);
	    if (*rname != '@')
	    {
		fprintf(stderr, "probable internal error: unexpected rname @ %s:%d\n",
			__FILE__, __LINE__);
	    }
	    else
	    {
		// Expected case.
	        emitcode ("mov", "a%s,%s", rname + 1, rname);
		rname++;  // skip the '@'.
	    }
	}
	else
	{
	    /* otherwise get a free pointer register */
	    aop = newAsmop (0);
	    preg = getFreePtr (ic, &aop, FALSE);
	    emitcode ("mov", "%s,%s",
		      preg->name,
		      aopGet (AOP (result), 0, FALSE, TRUE));
	    rname = preg->name;
	}
    }
    else
    {
	rname = aopGet (AOP (result), 0, FALSE, FALSE);
    }

  aopOp (right, ic, FALSE);

  /* if bitfield then unpack the bits */
  if (IS_BITFIELD (retype) || IS_BITFIELD (letype))
    genPackBits ((IS_BITFIELD (retype) ? retype : letype), right, rname, POINTER);
  else
    {
      /* we have can just get the values */
      int size = AOP_SIZE (right);
      int offset = 0;

      while (size--)
	{
	  l = aopGet (AOP (right), offset, FALSE, TRUE);
	  if (*l == '@')
	    {
	      MOVA (l);
	      emitcode ("mov", "@%s,a", rname);
	    }
	  else
	    emitcode ("mov", "@%s,%s", rname, l);
	  if (size || pi)
	    emitcode ("inc", "%s", rname);
	  offset++;
	}
    }

  /* now some housekeeping stuff */
  if (aop) /* we had to allocate for this iCode */
    {
      if (pi)
        aopPut (AOP (result), rname, 0, isOperandVolatile (result, FALSE));
      freeAsmop (NULL, aop, ic, TRUE);
    }
  else
    {
      /* we did not allocate which means left
         already in a pointer register, then
         if size > 0 && this could be used again
         we have to point it back to where it
         belongs */
      if ((AOP_SIZE (right) > 1 &&
	   !OP_SYMBOL (result)->remat &&
	   (OP_SYMBOL (result)->liveTo > ic->seq ||
	    ic->depth)) &&
	  !pi)
	{
	  int size = AOP_SIZE (right) - 1;
	  while (size--)
	    emitcode ("dec", "%s", rname);
	}
    }

  /* done */
  if (pi) pi->generated = 1;
  freeAsmop (result, NULL, ic, TRUE);
  freeAsmop (right, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genPagedPointerSet - emitcode for Paged pointer put             */
/*-----------------------------------------------------------------*/
static void
genPagedPointerSet (operand * right,
		    operand * result,
		    iCode * ic,
		    iCode * pi)
{
  asmop *aop = NULL;
  regs *preg = NULL;
  char *rname, *l;
  sym_link *retype, *letype;

  D(emitcode (";     genPagedPointerSet",""));

  retype = getSpec (operandType (right));
  letype = getSpec (operandType (result));

  aopOp (result, ic, FALSE);

  /* if the value is already in a pointer register
     then don't need anything more */
  if (!AOP_INPREG (AOP (result)))
    {
      /* otherwise get a free pointer register */
      aop = newAsmop (0);
      preg = getFreePtr (ic, &aop, FALSE);
      emitcode ("mov", "%s,%s",
		preg->name,
		aopGet (AOP (result), 0, FALSE, TRUE));
      rname = preg->name;
    }
  else
    rname = aopGet (AOP (result), 0, FALSE, FALSE);

  aopOp (right, ic, FALSE);

  /* if bitfield then unpack the bits */
  if (IS_BITFIELD (retype) || IS_BITFIELD (letype))
    genPackBits ((IS_BITFIELD (retype) ? retype : letype), right, rname, PPOINTER);
  else
    {
      /* we have can just get the values */
      int size = AOP_SIZE (right);
      int offset = 0;

      while (size--)
	{
	  l = aopGet (AOP (right), offset, FALSE, TRUE);

	  MOVA (l);
	  emitcode ("movx", "@%s,a", rname);

	  if (size || pi)
	    emitcode ("inc", "%s", rname);

	  offset++;
	}
    }

  /* now some housekeeping stuff */
  if (aop) /* we had to allocate for this iCode */
    {
      if (pi)
        aopPut (AOP (result), rname, 0, isOperandVolatile (result, FALSE));
      freeAsmop (NULL, aop, ic, TRUE);
    }
  else
    {
      /* we did not allocate which means left
         already in a pointer register, then
         if size > 0 && this could be used again
         we have to point it back to where it
         belongs */
      if (AOP_SIZE (right) > 1 &&
	  !OP_SYMBOL (result)->remat &&
	  (OP_SYMBOL (result)->liveTo > ic->seq ||
	   ic->depth))
	{
	  int size = AOP_SIZE (right) - 1;
	  while (size--)
	    emitcode ("dec", "%s", rname);
	}
    }

  /* done */
  if (pi) pi->generated = 1;
  freeAsmop (result, NULL, ic, TRUE);
  freeAsmop (right, NULL, ic, TRUE);


}

/*-----------------------------------------------------------------*/
/* genFarPointerSet - set value from far space                     */
/*-----------------------------------------------------------------*/
static void
genFarPointerSet (operand * right,
		  operand * result, iCode * ic, iCode * pi)
{
  int size, offset;
  sym_link *retype = getSpec (operandType (right));
  sym_link *letype = getSpec (operandType (result));

  D(emitcode (";     genFarPointerSet",""));

  aopOp (result, ic, FALSE);
  loadDptrFromOperand (result, FALSE);

  /* so dptr know contains the address */
  aopOp (right, ic, FALSE);

  /* if bit then unpack */
  if (IS_BITFIELD (retype) || IS_BITFIELD (letype))
    genPackBits ((IS_BITFIELD (retype) ? retype : letype), right, "dptr", FPOINTER);
  else
    {
      size = AOP_SIZE (right);
      offset = 0;

      while (size--)
	{
	  char *l = aopGet (AOP (right), offset++, FALSE, FALSE);
	  MOVA (l);
	  emitcode ("movx", "@dptr,a");
	  if (size || pi)
	    emitcode ("inc", "dptr");
	}
    }
  if (pi && AOP_TYPE (result) != AOP_STR && AOP_TYPE (result) != AOP_IMMD) {
    aopPut (AOP(result), "dpl", 0, isOperandVolatile (result, FALSE));
    aopPut (AOP(result), "dph", 1, isOperandVolatile (result, FALSE));
    pi->generated=1;
  }
  freeAsmop (result, NULL, ic, TRUE);
  freeAsmop (right, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genGenPointerSet - set value from generic pointer space         */
/*-----------------------------------------------------------------*/
static void
genGenPointerSet (operand * right,
		  operand * result, iCode * ic, iCode * pi)
{
  int size, offset;
  sym_link *retype = getSpec (operandType (right));
  sym_link *letype = getSpec (operandType (result));

  D(emitcode (";     genGenPointerSet",""));

  aopOp (result, ic, FALSE);
  loadDptrFromOperand (result, TRUE);

  /* so dptr know contains the address */
  aopOp (right, ic, FALSE);

  /* if bit then unpack */
  if (IS_BITFIELD (retype) || IS_BITFIELD (letype))
    genPackBits ((IS_BITFIELD (retype) ? retype : letype), right, "dptr", GPOINTER);
  else
    {
      size = AOP_SIZE (right);
      offset = 0;

      while (size--)
	{
	  char *l = aopGet (AOP (right), offset++, FALSE, FALSE);
	  MOVA (l);
	  emitcode ("lcall", "__gptrput");
	  if (size || pi)
	    emitcode ("inc", "dptr");
	}
    }

  if (pi && AOP_TYPE (result) != AOP_STR && AOP_TYPE (result) != AOP_IMMD) {
    aopPut (AOP(result), "dpl", 0, isOperandVolatile (result, FALSE));
    aopPut (AOP(result), "dph", 1, isOperandVolatile (result, FALSE));
    pi->generated=1;
  }
  freeAsmop (result, NULL, ic, TRUE);
  freeAsmop (right, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genPointerSet - stores the value into a pointer location        */
/*-----------------------------------------------------------------*/
static void
genPointerSet (iCode * ic, iCode *pi)
{
  operand *right, *result;
  sym_link *type, *etype;
  int p_type;

  D(emitcode (";     genPointerSet",""));

  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);

  /* depending on the type of pointer we need to
     move it to the correct pointer register */
  type = operandType (result);
  etype = getSpec (type);
  /* if left is of type of pointer then it is simple */
  if (IS_PTR (type) && !IS_FUNC (type->next))
    {
      p_type = DCL_TYPE (type);
    }
  else
    {
      /* we have to go by the storage class */
      p_type = PTR_TYPE (SPEC_OCLS (etype));
    }

  /* special case when cast remat */
  if (p_type == GPOINTER && OP_SYMBOL(result)->remat &&
      IS_CAST_ICODE(OP_SYMBOL(result)->rematiCode)) {
	  result = IC_RIGHT(OP_SYMBOL(result)->rematiCode);
	  type = operandType (result);
	  p_type = DCL_TYPE (type);
  }
  /* now that we have the pointer type we assign
     the pointer values */
  switch (p_type)
    {

    case POINTER:
    case IPOINTER:
      genNearPointerSet (right, result, ic, pi);
      break;

    case PPOINTER:
      genPagedPointerSet (right, result, ic, pi);
      break;

    case FPOINTER:
      genFarPointerSet (right, result, ic, pi);
      break;

    case GPOINTER:
      genGenPointerSet (right, result, ic, pi);
      break;

    default:
      werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
	      "genPointerSet: illegal pointer type");
    }

}

/*-----------------------------------------------------------------*/
/* genIfx - generate code for Ifx statement                        */
/*-----------------------------------------------------------------*/
static void
genIfx (iCode * ic, iCode * popIc)
{
  operand *cond = IC_COND (ic);
  int isbit = 0;

  D(emitcode (";     genIfx",""));

  aopOp (cond, ic, FALSE);

  /* get the value into acc */
  if (AOP_TYPE (cond) != AOP_CRY)
    toBoolean (cond);
  else
    isbit = 1;
  /* the result is now in the accumulator */
  freeAsmop (cond, NULL, ic, TRUE);

  /* if there was something to be popped then do it */
  if (popIc)
    genIpop (popIc);

  /* if the condition is  a bit variable */
  if (isbit && IS_ITEMP (cond) &&
      SPIL_LOC (cond))
    genIfxJump (ic, SPIL_LOC (cond)->rname);
  else if (isbit && !IS_ITEMP (cond))
    genIfxJump (ic, OP_SYMBOL (cond)->rname);
  else
    genIfxJump (ic, "a");

  ic->generated = 1;
}

/*-----------------------------------------------------------------*/
/* genAddrOf - generates code for address of                       */
/*-----------------------------------------------------------------*/
static void
genAddrOf (iCode * ic)
{
  symbol *sym = OP_SYMBOL (IC_LEFT (ic));
  int size, offset;

  D(emitcode (";     genAddrOf",""));

  aopOp (IC_RESULT (ic), ic, FALSE);

  /* if the operand is on the stack then we
     need to get the stack offset of this
     variable */
  if (sym->onStack)
    {
      /* if it has an offset then we need to compute
         it */
      if (sym->stack)
	{
	  emitcode ("mov", "a,_bp");
	  emitcode ("add", "a,#0x%02x", ((sym->stack < 0) ?
					 ((char) (sym->stack - _G.nRegsSaved)) :
					 ((char) sym->stack)) & 0xff);
	  aopPut (AOP (IC_RESULT (ic)), "a", 0, isOperandVolatile (IC_RESULT (ic), FALSE));
	}
      else
	{
	  /* we can just move _bp */
	  aopPut (AOP (IC_RESULT (ic)), "_bp", 0, isOperandVolatile (IC_RESULT (ic), FALSE));
	}
      /* fill the result with zero */
      size = AOP_SIZE (IC_RESULT (ic)) - 1;

      offset = 1;
      while (size--)
	{
	  aopPut (AOP (IC_RESULT (ic)), zero, offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
	}

      goto release;
    }

  /* object not on stack then we need the name */
  size = AOP_SIZE (IC_RESULT (ic));
  offset = 0;

  while (size--)
    {
      char s[SDCC_NAME_MAX];
      if (offset)
	sprintf (s, "#(%s >> %d)",
		 sym->rname,
		 offset * 8);
      else
	sprintf (s, "#%s", sym->rname);
      aopPut (AOP (IC_RESULT (ic)), s, offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
    }

release:
  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);

}

/*-----------------------------------------------------------------*/
/* genFarFarAssign - assignment when both are in far space         */
/*-----------------------------------------------------------------*/
static void
genFarFarAssign (operand * result, operand * right, iCode * ic)
{
  int size = AOP_SIZE (right);
  int offset = 0;
  char *l;

  D(emitcode (";     genFarFarAssign",""));

  /* first push the right side on to the stack */
  while (size--)
    {
      l = aopGet (AOP (right), offset++, FALSE, FALSE);
      MOVA (l);
      emitcode ("push", "acc");
    }

  freeAsmop (right, NULL, ic, FALSE);
  /* now assign DPTR to result */
  aopOp (result, ic, FALSE);
  size = AOP_SIZE (result);
  while (size--)
    {
      emitcode ("pop", "acc");
      aopPut (AOP (result), "a", --offset, isOperandVolatile (result, FALSE));
    }
  freeAsmop (result, NULL, ic, FALSE);

}

/*-----------------------------------------------------------------*/
/* genAssign - generate code for assignment                        */
/*-----------------------------------------------------------------*/
static void
genAssign (iCode * ic)
{
  operand *result, *right;
  int size, offset;
  unsigned long lit = 0L;

  D(emitcode(";     genAssign",""));

  result = IC_RESULT (ic);
  right = IC_RIGHT (ic);

  /* if they are the same */
  if (operandsEqu (result, right) &&
      !isOperandVolatile (result, FALSE) &&
      !isOperandVolatile (right, FALSE))
    return;

  aopOp (right, ic, FALSE);

  /* special case both in far space */
  if (AOP_TYPE (right) == AOP_DPTR &&
      IS_TRUE_SYMOP (result) &&
      isOperandInFarSpace (result))
    {

      genFarFarAssign (result, right, ic);
      return;
    }

  aopOp (result, ic, TRUE);

  /* if they are the same registers */
  if (sameRegs (AOP (right), AOP (result)) &&
      !isOperandVolatile (result, FALSE) &&
      !isOperandVolatile (right, FALSE))
    goto release;

  /* if the result is a bit */
  if (AOP_TYPE (result) == AOP_CRY)
    {

      /* if the right size is a literal then
         we know what the value is */
      if (AOP_TYPE (right) == AOP_LIT)
	{
	  if (((int) operandLitValue (right)))
	    aopPut (AOP (result), one, 0, isOperandVolatile (result, FALSE));
	  else
	    aopPut (AOP (result), zero, 0, isOperandVolatile (result, FALSE));
	  goto release;
	}

      /* the right is also a bit variable */
      if (AOP_TYPE (right) == AOP_CRY)
	{
	  emitcode ("mov", "c,%s", AOP (right)->aopu.aop_dir);
	  aopPut (AOP (result), "c", 0, isOperandVolatile (result, FALSE));
	  goto release;
	}

      /* we need to or */
      toBoolean (right);
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      goto release;
    }

  /* bit variables done */
  /* general case */
  size = AOP_SIZE (result);
  offset = 0;
  if (AOP_TYPE (right) == AOP_LIT)
    lit = (unsigned long) floatFromVal (AOP (right)->aopu.aop_lit);
  if ((size > 1) &&
      (AOP_TYPE (result) != AOP_REG) &&
      (AOP_TYPE (right) == AOP_LIT) &&
      !IS_FLOAT (operandType (right)) &&
      (lit < 256L))
    {
      emitcode ("clr", "a");
      while (size--)
	{
	  if ((unsigned int) ((lit >> (size * 8)) & 0x0FFL) == 0)
	    aopPut (AOP (result), "a", size, isOperandVolatile (result, FALSE));
	  else
	    aopPut (AOP (result),
		    aopGet (AOP (right), size, FALSE, FALSE),
		    size,
		    isOperandVolatile (result, FALSE));
	}
    }
  else
    {
      while (size--)
	{
	  aopPut (AOP (result),
		  aopGet (AOP (right), offset, FALSE, FALSE),
		  offset,
		  isOperandVolatile (result, FALSE));
	  offset++;
	}
    }

release:
  freeAsmop (right, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genJumpTab - genrates code for jump table                       */
/*-----------------------------------------------------------------*/
static void
genJumpTab (iCode * ic)
{
  symbol *jtab;
  char *l;

  D(emitcode (";     genJumpTab",""));

  aopOp (IC_JTCOND (ic), ic, FALSE);
  /* get the condition into accumulator */
  l = aopGet (AOP (IC_JTCOND (ic)), 0, FALSE, FALSE);
  MOVA (l);
  /* multiply by three */
  emitcode ("add", "a,acc");
  emitcode ("add", "a,%s", aopGet (AOP (IC_JTCOND (ic)), 0, FALSE, FALSE));
  freeAsmop (IC_JTCOND (ic), NULL, ic, TRUE);

  jtab = newiTempLabel (NULL);
  emitcode ("mov", "dptr,#%05d$", jtab->key + 100);
  emitcode ("jmp", "@a+dptr");
  emitcode ("", "%05d$:", jtab->key + 100);
  /* now generate the jump labels */
  for (jtab = setFirstItem (IC_JTLABELS (ic)); jtab;
       jtab = setNextItem (IC_JTLABELS (ic)))
    emitcode ("ljmp", "%05d$", jtab->key + 100);

}

/*-----------------------------------------------------------------*/
/* genCast - gen code for casting                                  */
/*-----------------------------------------------------------------*/
static void
genCast (iCode * ic)
{
  operand *result = IC_RESULT (ic);
  sym_link *ctype = operandType (IC_LEFT (ic));
  sym_link *rtype = operandType (IC_RIGHT (ic));
  operand *right = IC_RIGHT (ic);
  int size, offset;

  D(emitcode(";     genCast",""));

  /* if they are equivalent then do nothing */
  if (operandsEqu (IC_RESULT (ic), IC_RIGHT (ic)))
    return;

  aopOp (right, ic, FALSE);
  aopOp (result, ic, FALSE);

  /* if the result is a bit (and not a bitfield) */
  // if (AOP_TYPE (result) == AOP_CRY)
  if (IS_BITVAR (OP_SYMBOL (result)->type)
      && !IS_BITFIELD (OP_SYMBOL (result)->type) )
    {
      /* if the right size is a literal then
         we know what the value is */
      if (AOP_TYPE (right) == AOP_LIT)
	{
	  if (((int) operandLitValue (right)))
	    aopPut (AOP (result), one, 0, isOperandVolatile (result, FALSE));
	  else
	    aopPut (AOP (result), zero, 0, isOperandVolatile (result, FALSE));

	  goto release;
	}

      /* the right is also a bit variable */
      if (AOP_TYPE (right) == AOP_CRY)
	{
	  emitcode ("mov", "c,%s", AOP (right)->aopu.aop_dir);
	  aopPut (AOP (result), "c", 0, isOperandVolatile (result, FALSE));
	  goto release;
	}

      /* we need to or */
      toBoolean (right);
      aopPut (AOP (result), "a", 0, isOperandVolatile (result, FALSE));
      goto release;
    }


  /* if they are the same size : or less */
  if (AOP_SIZE (result) <= AOP_SIZE (right))
    {

      /* if they are in the same place */
      if (sameRegs (AOP (right), AOP (result)))
	goto release;

      /* if they in different places then copy */
      size = AOP_SIZE (result);
      offset = 0;
      while (size--)
	{
	  aopPut (AOP (result),
		  aopGet (AOP (right), offset, FALSE, FALSE),
		  offset,
		  isOperandVolatile (result, FALSE));
	  offset++;
	}
      goto release;
    }


  /* if the result is of type pointer */
  if (IS_PTR (ctype))
    {

      int p_type;
      sym_link *type = operandType (right);
      sym_link *etype = getSpec (type);

      /* pointer to generic pointer */
      if (IS_GENPTR (ctype))
	{
	  if (IS_PTR (type))
	    p_type = DCL_TYPE (type);
	  else
	    {
	      if (SPEC_SCLS(etype)==S_REGISTER) {
		// let's assume it is a generic pointer
		p_type=GPOINTER;
	      } else {
		/* we have to go by the storage class */
		p_type = PTR_TYPE (SPEC_OCLS (etype));
	      }
	    }

	  /* the first two bytes are known */
	  size = GPTRSIZE - 1;
	  offset = 0;
	  while (size--)
	    {
	      aopPut (AOP (result),
		      aopGet (AOP (right), offset, FALSE, FALSE),
		      offset,
		      isOperandVolatile (result, FALSE));
	      offset++;
	    }
	  /* the last byte depending on type */
	    {
		int gpVal = pointerTypeToGPByte(p_type, NULL, NULL);
		char gpValStr[10];

		if (gpVal == -1)
		{
		    // pointerTypeToGPByte will have bitched.
		    exit(1);
		}

		sprintf(gpValStr, "#0x%d", gpVal);
		aopPut (AOP (result), gpValStr, GPTRSIZE - 1, isOperandVolatile (result, FALSE));
	    }
	  goto release;
	}

      /* just copy the pointers */
      size = AOP_SIZE (result);
      offset = 0;
      while (size--)
	{
	  aopPut (AOP (result),
		  aopGet (AOP (right), offset, FALSE, FALSE),
		  offset,
		  isOperandVolatile (result, FALSE));
	  offset++;
	}
      goto release;
    }

  /* so we now know that the size of destination is greater
     than the size of the source */
  /* we move to result for the size of source */
  size = AOP_SIZE (right);
  offset = 0;
  while (size--)
    {
      aopPut (AOP (result),
	      aopGet (AOP (right), offset, FALSE, FALSE),
	      offset,
	      isOperandVolatile (result, FALSE));
      offset++;
    }

  /* now depending on the sign of the source && destination */
  size = AOP_SIZE (result) - AOP_SIZE (right);
  /* if unsigned or not an integral type */
  if (!IS_SPEC (rtype) || SPEC_USIGN (rtype) || AOP_TYPE(right)==AOP_CRY)
    {
      while (size--)
	aopPut (AOP (result), zero, offset++, isOperandVolatile (result, FALSE));
    }
  else
    {
      /* we need to extend the sign :{ */
      char *l = aopGet (AOP (right), AOP_SIZE (right) - 1,
			FALSE, FALSE);
      MOVA (l);
      emitcode ("rlc", "a");
      emitcode ("subb", "a,acc");
      while (size--)
	aopPut (AOP (result), "a", offset++, isOperandVolatile (result, FALSE));
    }

  /* we are done hurray !!!! */

release:
  freeAsmop (right, NULL, ic, TRUE);
  freeAsmop (result, NULL, ic, TRUE);

}

/*-----------------------------------------------------------------*/
/* genDjnz - generate decrement & jump if not zero instrucion      */
/*-----------------------------------------------------------------*/
static int
genDjnz (iCode * ic, iCode * ifx)
{
  symbol *lbl, *lbl1;
  if (!ifx)
    return 0;

  D(emitcode (";     genDjnz",""));

  /* if the if condition has a false label
     then we cannot save */
  if (IC_FALSE (ifx))
    return 0;

  /* if the minus is not of the form
     a = a - 1 */
  if (!isOperandEqual (IC_RESULT (ic), IC_LEFT (ic)) ||
      !IS_OP_LITERAL (IC_RIGHT (ic)))
    return 0;

  if (operandLitValue (IC_RIGHT (ic)) != 1)
    return 0;

  /* if the size of this greater than one then no
     saving */
  if (getSize (operandType (IC_RESULT (ic))) > 1)
    return 0;

  /* otherwise we can save BIG */
  lbl = newiTempLabel (NULL);
  lbl1 = newiTempLabel (NULL);

  aopOp (IC_RESULT (ic), ic, FALSE);

  if (AOP_NEEDSACC(IC_RESULT(ic)))
  {
      /* If the result is accessed indirectly via
       * the accumulator, we must explicitly write
       * it back after the decrement.
       */
      char *rByte = aopGet(AOP(IC_RESULT(ic)), 0, FALSE, FALSE);

      if (strcmp(rByte, "a"))
      {
           /* Something is hopelessly wrong */
           fprintf(stderr, "*** warning: internal error at %s:%d\n",
           	   __FILE__, __LINE__);
           /* We can just give up; the generated code will be inefficient,
            * but what the hey.
            */
           freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
           return 0;
      }
      emitcode ("dec", "%s", rByte);
      aopPut(AOP(IC_RESULT(ic)), rByte, 0, isOperandVolatile (IC_RESULT (ic), FALSE));
      emitcode ("jnz", "%05d$", lbl->key + 100);
  }
  else if (IS_AOP_PREG (IC_RESULT (ic)))
    {
      emitcode ("dec", "%s",
		aopGet (AOP (IC_RESULT (ic)), 0, FALSE, FALSE));
      emitcode ("mov", "a,%s", aopGet (AOP (IC_RESULT (ic)), 0, FALSE, FALSE));
      emitcode ("jnz", "%05d$", lbl->key + 100);
    }
  else
    {
      emitcode ("djnz", "%s,%05d$", aopGet (AOP (IC_RESULT (ic)), 0, FALSE, FALSE),
		lbl->key + 100);
    }
  emitcode ("sjmp", "%05d$", lbl1->key + 100);
  emitcode ("", "%05d$:", lbl->key + 100);
  emitcode ("ljmp", "%05d$", IC_TRUE (ifx)->key + 100);
  emitcode ("", "%05d$:", lbl1->key + 100);

  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
  ifx->generated = 1;
  return 1;
}

/*-----------------------------------------------------------------*/
/* genReceive - generate code for a receive iCode                  */
/*-----------------------------------------------------------------*/
static void
genReceive (iCode * ic)
{
    int size = getSize (operandType (IC_RESULT (ic)));
    int offset = 0;
  D(emitcode (";     genReceive",""));

  if (ic->argreg == 1) { /* first parameter */
      if (isOperandInFarSpace (IC_RESULT (ic)) &&
	  (OP_SYMBOL (IC_RESULT (ic))->isspilt ||
	   IS_TRUE_SYMOP (IC_RESULT (ic)))) {

	  regs *tempRegs[4];
	  int receivingA = 0;
	  int roffset = 0;

	  for (offset = 0; offset<size; offset++)
	    if (!strcmp (fReturn[offset], "a"))
	      receivingA = 1;

	  if (!receivingA)
	    {
	      if (size==1 || getTempRegs(tempRegs, size-1, ic))
		{
		  for (offset = size-1; offset>0; offset--)
		    emitcode("mov","%s,%s", tempRegs[roffset++]->name, fReturn[offset]);
		  emitcode("mov","a,%s", fReturn[0]);
		  _G.accInUse++;
		  aopOp (IC_RESULT (ic), ic, FALSE);
		  _G.accInUse--;
		  aopPut (AOP (IC_RESULT (ic)), "a", offset,
		          isOperandVolatile (IC_RESULT (ic), FALSE));
		  for (offset = 1; offset<size; offset++)
		    aopPut (AOP (IC_RESULT (ic)), tempRegs[--roffset]->name, offset,
			    isOperandVolatile (IC_RESULT (ic), FALSE));
		  goto release;
		}
	    }
	  else
	    {
	      if (getTempRegs(tempRegs, size, ic))
		{
		  for (offset = 0; offset<size; offset++)
		    emitcode("mov","%s,%s", tempRegs[offset]->name, fReturn[offset]);
		  aopOp (IC_RESULT (ic), ic, FALSE);
		  for (offset = 0; offset<size; offset++)
		    aopPut (AOP (IC_RESULT (ic)), tempRegs[offset]->name, offset,
			    isOperandVolatile (IC_RESULT (ic), FALSE));
		  goto release;
		}
	    }

	  offset = fReturnSizeMCS51 - size;
	  while (size--) {
	      emitcode ("push", "%s", (strcmp (fReturn[fReturnSizeMCS51 - offset - 1], "a") ?
				       fReturn[fReturnSizeMCS51 - offset - 1] : "acc"));
	      offset++;
	  }
	  aopOp (IC_RESULT (ic), ic, FALSE);
	  size = AOP_SIZE (IC_RESULT (ic));
	  offset = 0;
	  while (size--) {
	      emitcode ("pop", "acc");
	      aopPut (AOP (IC_RESULT (ic)), "a", offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
	  }

      } else {
	  _G.accInUse++;
	  aopOp (IC_RESULT (ic), ic, FALSE);
	  _G.accInUse--;
	  assignResultValue (IC_RESULT (ic));
      }
  } else { /* second receive onwards */
      int rb1off ;
      aopOp (IC_RESULT (ic), ic, FALSE);
      rb1off = ic->argreg;
      while (size--) {
	  aopPut (AOP (IC_RESULT (ic)), rb1regs[rb1off++ -5], offset++, isOperandVolatile (IC_RESULT (ic), FALSE));
      }
  }

release:
  freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genDummyRead - generate code for dummy read of volatiles        */
/*-----------------------------------------------------------------*/
static void
genDummyRead (iCode * ic)
{
  operand *op;
  int size, offset;

  D(emitcode(";     genDummyRead",""));

  op = IC_RIGHT (ic);
  if (op && IS_SYMOP (op))
    {
      aopOp (op, ic, FALSE);

      /* if the result is a bit */
      if (AOP_TYPE (op) == AOP_CRY)
        emitcode ("mov", "c,%s", AOP (op)->aopu.aop_dir);
      else
	{
	  /* bit variables done */
	  /* general case */
	  size = AOP_SIZE (op);
	  offset = 0;
	  while (size--)
	  {
	    MOVA (aopGet (AOP (op), offset, FALSE, FALSE));
	    offset++;
	  }
	}

      freeAsmop (op, NULL, ic, TRUE);
    }

  op = IC_LEFT (ic);
  if (op && IS_SYMOP (op))
    {
      aopOp (op, ic, FALSE);

      /* if the result is a bit */
      if (AOP_TYPE (op) == AOP_CRY)
        emitcode ("mov", "c,%s", AOP (op)->aopu.aop_dir);
      else
	{
	  /* bit variables done */
	  /* general case */
	  size = AOP_SIZE (op);
	  offset = 0;
	  while (size--)
	  {
	    MOVA (aopGet (AOP (op), offset, FALSE, FALSE));
	    offset++;
	  }
	}

      freeAsmop (op, NULL, ic, TRUE);
    }
}

/*-----------------------------------------------------------------*/
/* genCritical - generate code for start of a critical sequence    */
/*-----------------------------------------------------------------*/
static void
genCritical (iCode *ic)
{
  symbol *tlbl = newiTempLabel (NULL);

  D(emitcode(";     genCritical",""));

  if (IC_RESULT (ic))
    aopOp (IC_RESULT (ic), ic, TRUE);

  emitcode ("setb", "c");
  emitcode ("jbc", "ea,%05d$", (tlbl->key + 100)); /* atomic test & clear */
  emitcode ("clr", "c");
  emitcode ("", "%05d$:", (tlbl->key + 100));

  if (IC_RESULT (ic))
    outBitC (IC_RESULT (ic)); /* save old ea in an operand */
  else
    emitcode ("push", "psw"); /* save old ea via c in psw on top of stack*/

  if (IC_RESULT (ic))
    freeAsmop (IC_RESULT (ic), NULL, ic, TRUE);
}

/*-----------------------------------------------------------------*/
/* genEndCritical - generate code for end of a critical sequence   */
/*-----------------------------------------------------------------*/
static void
genEndCritical (iCode *ic)
{
  D(emitcode(";     genEndCritical",""));

  if (IC_RIGHT (ic))
    {
      aopOp (IC_RIGHT (ic), ic, FALSE);
      if (AOP_TYPE (IC_RIGHT (ic)) == AOP_CRY)
        {
	  emitcode ("mov", "c,%s", IC_RIGHT (ic)->aop->aopu.aop_dir);
          emitcode ("mov", "ea,c");
        }
      else
        {
          MOVA (aopGet (AOP (IC_RIGHT (ic)), 0, FALSE, FALSE));
          emitcode ("rrc", "a");
          emitcode ("mov", "ea,c");
        }
      freeAsmop (IC_RIGHT (ic), NULL, ic, TRUE);
    }
  else
    {
      emitcode ("pop", "psw"); /* restore ea via c in psw on top of stack */
      emitcode ("mov", "ea,c");
    }
}


/*-----------------------------------------------------------------*/
/* gen51Code - generate code for 8051 based controllers            */
/*-----------------------------------------------------------------*/
void
gen51Code (iCode * lic)
{
  iCode *ic;
  int cln = 0;

  _G.currentFunc = NULL;
  lineHead = lineCurr = NULL;

  /* print the allocation information */
  if (allocInfo && currFunc)
    printAllocInfo (currFunc, codeOutFile);
  /* if debug information required */
  if (options.debug && currFunc)
    {
      debugFile->writeFunction(currFunc);
      _G.debugLine = 1;
      if (IS_STATIC (currFunc->etype))
	emitcode ("", "F%s$%s$0$0 ==.", moduleName, currFunc->name);
      else
	emitcode ("", "G$%s$0$0 ==.", currFunc->name);
      _G.debugLine = 0;
    }
  /* stack pointer name */
  if (options.useXstack)
    spname = "_spx";
  else
    spname = "sp";


  for (ic = lic; ic; ic = ic->next)
    {
      _G.current_iCode = ic;

      if (ic->lineno && cln != ic->lineno)
	{
	  if (options.debug)
	    {
	      _G.debugLine = 1;
	      emitcode ("", "C$%s$%d$%d$%d ==.",
			FileBaseName (ic->filename), ic->lineno,
			ic->level, ic->block);
	      _G.debugLine = 0;
	    }
	  if (!options.noCcodeInAsm) {
	    emitcode ("", ";%s:%d: %s", ic->filename, ic->lineno,
		      printCLine(ic->filename, ic->lineno));
	  }
	  cln = ic->lineno;
	}
      if (options.iCodeInAsm) {
	char regsInUse[80];
	int i;

	for (i=0; i<8; i++) {
	  sprintf (&regsInUse[i],
		   "%c", ic->riu & (1<<i) ? i+'0' : '-');
	}
	regsInUse[i]=0;
	emitcode("", "; [%s] ic:%d: %s", regsInUse, ic->seq, printILine(ic));
      }
      /* if the result is marked as
         spilt and rematerializable or code for
         this has already been generated then
         do nothing */
      if (resultRemat (ic) || ic->generated)
	continue;

      /* depending on the operation */
      switch (ic->op)
	{
	case '!':
	  genNot (ic);
	  break;

	case '~':
	  genCpl (ic);
	  break;

	case UNARYMINUS:
	  genUminus (ic);
	  break;

	case IPUSH:
	  genIpush (ic);
	  break;

	case IPOP:
	  /* IPOP happens only when trying to restore a
	     spilt live range, if there is an ifx statement
	     following this pop then the if statement might
	     be using some of the registers being popped which
	     would destory the contents of the register so
	     we need to check for this condition and handle it */
	  if (ic->next &&
	      ic->next->op == IFX &&
	      regsInCommon (IC_LEFT (ic), IC_COND (ic->next)))
	    genIfx (ic->next, ic);
	  else
	    genIpop (ic);
	  break;

	case CALL:
	  genCall (ic);
	  break;

	case PCALL:
	  genPcall (ic);
	  break;

	case FUNCTION:
	  genFunction (ic);
	  break;

	case ENDFUNCTION:
	  genEndFunction (ic);
	  break;

	case RETURN:
	  genRet (ic);
	  break;

	case LABEL:
	  genLabel (ic);
	  break;

	case GOTO:
	  genGoto (ic);
	  break;

	case '+':
	  genPlus (ic);
	  break;

	case '-':
	  if (!genDjnz (ic, ifxForOp (IC_RESULT (ic), ic)))
	    genMinus (ic);
	  break;

	case '*':
	  genMult (ic);
	  break;

	case '/':
	  genDiv (ic);
	  break;

	case '%':
	  genMod (ic);
	  break;

	case '>':
	  genCmpGt (ic, ifxForOp (IC_RESULT (ic), ic));
	  break;

	case '<':
	  genCmpLt (ic, ifxForOp (IC_RESULT (ic), ic));
	  break;

	case LE_OP:
	case GE_OP:
	case NE_OP:

	  /* note these two are xlated by algebraic equivalence
	     during parsing SDCC.y */
	  werror (E_INTERNAL_ERROR, __FILE__, __LINE__,
		  "got '>=' or '<=' shouldn't have come here");
	  break;

	case EQ_OP:
	  genCmpEq (ic, ifxForOp (IC_RESULT (ic), ic));
	  break;

	case AND_OP:
	  genAndOp (ic);
	  break;

	case OR_OP:
	  genOrOp (ic);
	  break;

	case '^':
	  genXor (ic, ifxForOp (IC_RESULT (ic), ic));
	  break;

	case '|':
	  genOr (ic, ifxForOp (IC_RESULT (ic), ic));
	  break;

	case BITWISEAND:
	  genAnd (ic, ifxForOp (IC_RESULT (ic), ic));
	  break;

	case INLINEASM:
	  genInline (ic);
	  break;

	case RRC:
	  genRRC (ic);
	  break;

	case RLC:
	  genRLC (ic);
	  break;

	case GETHBIT:
	  genGetHbit (ic);
	  break;

	case LEFT_OP:
	  genLeftShift (ic);
	  break;

	case RIGHT_OP:
	  genRightShift (ic);
	  break;

	case GET_VALUE_AT_ADDRESS:
	  genPointerGet (ic, hasInc(IC_LEFT(ic),ic,getSize(operandType(IC_RESULT(ic)))));
	  break;

	case '=':
	  if (POINTER_SET (ic))
	    genPointerSet (ic, hasInc (IC_RESULT(ic),ic,getSize(operandType(IC_RIGHT(ic)))));
	  else
	    genAssign (ic);
	  break;

	case IFX:
	  genIfx (ic, NULL);
	  break;

	case ADDRESS_OF:
	  genAddrOf (ic);
	  break;

	case JUMPTABLE:
	  genJumpTab (ic);
	  break;

	case CAST:
	  genCast (ic);
	  break;

	case RECEIVE:
	  genReceive (ic);
	  break;

	case SEND:
	  addSet (&_G.sendSet, ic);
	  break;

	case DUMMY_READ_VOLATILE:
	  genDummyRead (ic);
	  break;

	case CRITICAL:
	  genCritical (ic);
	  break;

	case ENDCRITICAL:
	  genEndCritical (ic);
	  break;

	case SWAP:
	  genSwap (ic);
	  break;

	default:
	  ic = ic;
	}
    }

  _G.current_iCode = NULL;

  /* now we are ready to call the
     peep hole optimizer */
  if (!options.nopeep)
    peepHole (&lineHead);

  /* now do the actual printing */
  printLine (lineHead, codeOutFile);
  return;
}
