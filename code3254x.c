/* code3254x.c */
/*****************************************************************************/
/* Macro Assembler AS                                                        */
/*                                                                           */
/* code generator for TI C54x DSP devices                                    */
/*                                                                           */
/* history:  2001-07-07: begun                                               */
/*           2001-07-30: added simple accumulator instructions               */
/*           2001-07-31: added address decoder                               */
/*           2001-08-03: ADD SUB                                             */
/*           2001-08-05: MemAccOrders                                        */
/*           2001-08-18: MemConstOrders                                      */
/*           2001-08-19: multiply orders begun                               */
/*           2001-08-27: MPYA SQUR                                           */
/*           2001-08-30: MACx begun                                          */
/*           2001-08-31: MACD MACP MACSU MAS MASR MASAR                      */
/*           2001-09-16: DADD                                                */
/*           2001-09-29: AND OR XOR SFTA SFTL                                */
/*           2001-09-30: FIRS BIT BITF                                       */
/*           2001-10-03: CMPR                                                */
/*           2001-10-14: B BD CALL CALLD                                     */
/*           2001-10-17: B BD CC CCD                                         */
/*           2001-10-20: FB FBD FCALL FCALLD                                 */
/*           2001-10-24: RPT RPTB RPTBD                                      */
/*           2001-10-25: RPTZ                                                */
/*           2001-10-26: FRAME                                               */
/*           2001-10-28: PSHM IDLE RSBX SSBX XC                              */
/*           2001-11-01: remaining non-parallel ops                          */
/*           2001-11-09: parallel ops                                        */
/*           2001-11-11: added pseudo ops                                    */
/*           2002-01-13: fixed undefined value of OK in some cases           */
/*                                                                           */
/*****************************************************************************/
/* $Id: code3254x.c,v 1.7 2014/03/08 21:06:35 alfred Exp $                   */
/*****************************************************************************
 * $Log: code3254x.c,v $
 * Revision 1.7  2014/03/08 21:06:35  alfred
 * - rework ASSUME framework
 *
 * Revision 1.6  2010/04/17 13:14:20  alfred
 * - address overlapping strcpy()
 *
 * Revision 1.5  2007/11/24 22:48:03  alfred
 * - some NetBSD changes
 *
 * Revision 1.4  2005/09/08 16:53:41  alfred
 * - use common PInstTable
 *
 * Revision 1.3  2005/05/21 16:22:12  alfred
 * - remove double variables, correct call-by-reference arg
 *
 * Revision 1.2  2004/05/29 12:18:05  alfred
 * - relocated DecodeTIPseudo() to separate module
 *
 *****************************************************************************/

/*-------------------------------------------------------------------------*/
/* Includes */

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "bpemu.h"
#include "strutil.h"
#include "chunks.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmallg.h"
#include "asmrelocs.h"
#include "asmcode.h"
#include "codepseudo.h"
#include "tipseudo.h"
#include "asmitree.h"
#include "codevars.h"
#include "fileformat.h"
#include "headids.h"

#include "code3254x.h"

/*-------------------------------------------------------------------------*/
/* Data Structures */

#define FixedOrderCnt 12
#define AccOrderCnt 16
#define Acc2OrderCnt 5
#define MemOrderCnt 9
#define XYOrderCnt 4
#define MemAccOrderCnt 17
#define MemConstOrderCnt 5
#define MacOrderCnt 3
#define ConditionCnt 23

typedef struct
        {
          Word Code;
          Boolean IsRepeatable;
        } FixedOrder;

typedef struct
        {
          Word Code;
          Boolean IsRepeatable, Swap;
          IntType ConstType;
        } MemConstOrder;

typedef struct
        {
          char *Name;
          Word Class, Code, Mask;
        } Condition;

typedef enum {ModNone = - 1, ModAcc, ModMem, ModImm, ModAReg} ModType;
#define MModAcc  (1 << ModAcc)
#define MModMem  (1 << ModMem)
#define MModImm  (1 << ModImm)
#define MModAReg (1 << ModAReg)

static LongInt Reg_CPL, Reg_DP, Reg_SP;

static Boolean ThisRep, LastRep, ForcePageZero;

static FixedOrder *FixedOrders, *AccOrders, *Acc2Orders, *MemOrders, *XYOrders,
                  *MemAccOrders, *MacOrders;
static MemConstOrder *MemConstOrders;
static Condition *Conditions;

static SimpProc SaveInitProc;

static CPUVar CPU320C541;

static IntType OpSize;
static ShortInt AdrMode;
static Word AdrVals[3];

static Boolean ThisPar;
static Word LastOpCode;

#define ASSUME3254xCount 3
static ASSUMERec ASSUME3254xs[ASSUME3254xCount] = 
               {{"CPL", &Reg_CPL, 0,      1,       0},
                {"DP" , &Reg_DP , 0,  0x1ff,   0x200},
                {"SP" , &Reg_SP , 0, 0xffff, 0x10000}};

/*-------------------------------------------------------------------------*/
/* Address Decoder */

static char ShortConds[4][4] = {"EQ", "LT", "GT", "NEQ"};

static Boolean IsAcc(char *Asc)
{
  return ((Asc[1] == '\0') && (mytoupper(*Asc) >= 'A') && (mytoupper(*Asc) <= 'B'));
}

static Boolean DecodeAdr(char *Asc, int Mask)
{
#define IndirCnt 16
  static char *Patterns[IndirCnt] = /* leading asterisk is omitted since constant */
              { "ARx",      "ARx-",     "ARx+",      "+ARx",
                "ARx-0B",   "ARx-0",    "ARx+0",     "ARx+0B",
                "ARx-%",    "ARx-0%",   "ARx+%",     "ARx+0%",
                "ARx(n)",   "+ARx(n)",  "+ARx(n)%",  "(n)"};
  Boolean OK;

  AdrMode = ModNone; AdrCnt = 0;

  /* accumulators */

  if (IsAcc(Asc))
  {
    AdrMode = ModAcc; *AdrVals = mytoupper(*Asc) - 'A';
    goto done;
  }

  /* aux registers */

  if ((strlen(Asc) == 3) && (!strncasecmp(Asc, "AR", 2)) && (Asc[2] >= '0') && (Asc[2] <= '7'))
  {
    AdrMode = ModAReg; *AdrVals = Asc[2] - '0';
    goto done;
  }

  /* immediate */

  if (*Asc == '#')
  {
    *AdrVals = EvalIntExpression(Asc + 1, OpSize, &OK);
    if (OK)
      AdrMode = ModImm;
    goto done;
  }

  /* indirect */

  if (*Asc == '*')
  {
    int z;
    Word RegNum;
    char *pConstStart, *pConstEnd;

    /* check all possible patterns */

    for (z = 0; z < IndirCnt; z++)
    {
      char *pPattern = Patterns[z], *pComp = Asc + 1;

      /* pattern comparison */

      RegNum = 0; pConstStart = pConstEnd = NULL; OK = TRUE;
      while ((*pPattern) && (*pComp) && (OK))
      {
        switch (*pPattern)
        {
          case 'x': /* embedded number */
            RegNum = *pComp - '0';
            OK = RegNum < 8;
            break;
          case 'n': /* constant */
            pConstStart = pComp;
            pConstEnd = QuotPos(pComp, pPattern[1]);
            if (pConstEnd)
              pComp = pConstEnd - 1;
            else
              OK = False;
            break;
          default:  /* compare verbatim */
            if (mytoupper(*pPattern) != mytoupper(*pComp))
              OK = False;
        }
        if (OK)
        {
          pPattern++; pComp++;
        }
      }

      /* for a successful comparison, we must have reached the end of both strings
         simultaneously. */

      OK = OK && (!*pPattern) && (!*pComp);
      if (OK)
        break;
    }

    if (!OK) WrError(1350);
    else
    {
      /* decode offset ? pConst... /must/ be set if such a pattern was successfully
         decoded! */

      if (strchr(Patterns[z], 'n'))
      {
        /* MMR-style instructions do not allow an extension word */

        if (ForcePageZero)
        {
          WrError(1350);
          OK = False;
        }
        else
        {
          char Save = *pConstEnd;
          *pConstEnd = '\0';
          AdrVals[1] = EvalIntExpression(pConstStart, Int16, &OK);
          *pConstEnd = Save;
          if (OK)
            AdrCnt = 1;
        }
      }

      /* all fine until now? Then do the rest... */

      if (OK)
      {
        AdrMode = ModMem; AdrVals[0] = 0x80 | (z << 3) | RegNum;
      }
    }

    goto done;
  }

  /* then try absolute resp. immediate if absolute not allowed */

  if (Mask & MModMem)
  {
    FirstPassUnknown = FALSE;
    *AdrVals = EvalIntExpression(Asc, UInt16, &OK);
    if (OK)
    {
      if (Reg_CPL) /* short address rel. to SP? */
      {
        *AdrVals -= Reg_SP;
        if ((NOT FirstPassUnknown) && (*AdrVals > 127))
          WrError(110);
      }
      else         /* on DP page ? */
      {
        if ((NOT FirstPassUnknown) && ((*AdrVals >> 7) != (Reg_DP)))
          WrError(110);
      }
      AdrVals[0] &= 127;
      AdrMode = ModMem;
    }
  }
  else
  {
    *AdrVals = EvalIntExpression(Asc, OpSize, &OK);
    if (OK)
      AdrMode = ModImm;
  }

done:
  if ((AdrMode != ModNone) && (!(Mask & (1 << AdrMode))))
  {
    AdrMode = ModNone; AdrCnt = 0;
    WrError(1350);
  }
  return (AdrMode != ModNone);
}

static Boolean MakeXY(Word *Dest, Boolean Quarrel)
{
  Boolean Result = False;

  if (AdrMode != ModMem);  /* should never occur, if address mask specified correctly before */
  else
  {
    Word Mode = (*AdrVals >> 3) & 15, Reg = *AdrVals & 7;

    if ((Reg < 2) || (Reg > 5));
    else if ((Mode != 0) && (Mode != 1) && (Mode != 2) && (Mode != 11));
    else
    {
      *Dest = (Reg - 2) | ((Mode & 3) << 2);
      Result = True;
    }
  }

  if ((Quarrel) && (!Result))
    WrError(1350);

  return Result;
}

static Boolean DecodeCondition(int StartIndex, Word *Result, int *errindex, Boolean *ErrUnknown)
{
  int z, z2;
  Word CurrClass, CurrMask;
  
  *Result = CurrMask = 0; CurrClass = 0xffff; *ErrUnknown = False;

  for (z = StartIndex; z <= ArgCnt; z++)
  {
    for (z2 = 0; z2 < ConditionCnt; z2++)
      if (!strcasecmp(ArgStr[z], Conditions[z2].Name))
        break;
    if (z2 >= ConditionCnt)
    {
      *ErrUnknown = True;
      break;
    }

    if (CurrClass == 0xffff)
      CurrClass = Conditions[z2].Class;
    else if (CurrClass != Conditions[z2].Class)
      break;

    if (Conditions[z2].Mask & CurrMask)
      break;

    CurrMask |= Conditions[z2].Mask;
    *Result |= Conditions[z2].Code;
  }

  *errindex = z;
  return (CurrClass != 0xffff) && (z > ArgCnt);
}

/*-------------------------------------------------------------------------*/
/* Decoders */

static void DecodeFixed(Word Index)
{
  FixedOrder *POrder = FixedOrders + Index;

  if (ArgCnt != 0) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if ((LastRep) && (!POrder->IsRepeatable)) WrError(1560);
  else
  {
    WAsmCode[0] = POrder->Code; CodeLen = 1;
  }
}

static void DecodeAcc(Word Index)
{
  FixedOrder *POrder = AccOrders + Index;

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if ((LastRep) && (!POrder->IsRepeatable)) WrError(1560);
  else
  {
    if (DecodeAdr(ArgStr[1], MModAcc))
    {
      WAsmCode[0] = POrder->Code | (AdrVals[0] << 8); CodeLen = 1;
    }
  }
}

static void DecodeAcc2(Word Index)
{
  FixedOrder *POrder = Acc2Orders + Index;
  Boolean OK;

  if ((ArgCnt < 1) || (ArgCnt > 2)) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if ((LastRep) && (!POrder->IsRepeatable)) WrError(1560);
  else
  {
    if (((OK = DecodeAdr(ArgStr[1], MModAcc))))
    {
      WAsmCode[0] = POrder->Code | (AdrVals[0] << 9);
      if (ArgCnt == 2)
        OK = DecodeAdr(ArgStr[2], MModAcc);
      if (OK)
      {
        WAsmCode[0] |= (AdrVals[0] << 8);
        CodeLen = 1;
      }
    }
  }
}

static void DecodeMem(Word Index)
{
  FixedOrder *POrder = MemOrders + Index;

  if (ArgCnt < 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if ((LastRep) && (!POrder->IsRepeatable)) WrError(1560);
  else if (DecodeAdr(ArgStr[1], MModMem))
  {
    memcpy(WAsmCode, AdrVals, (AdrCnt + 1) << 1);
    WAsmCode[0] |= POrder->Code;
    CodeLen = 1 + AdrCnt;
  }
}

static void DecodeXY(Word Index)
{
  FixedOrder *POrder = XYOrders + Index;
  Word TmpX, TmpY;

  if (ArgCnt != 2) WrError(1350);
  else if (ThisPar) WrError(1950);
  else if ((LastRep) && (!POrder->IsRepeatable)) WrError(1560);
  else
  {
    if (DecodeAdr(ArgStr[1], MModMem))
      if (MakeXY(&TmpX, True))
        if (DecodeAdr(ArgStr[2], MModMem))
          if (MakeXY(&TmpY, True))
          {
            WAsmCode[0] = POrder->Code | (TmpX << 4) | TmpY;
            CodeLen = 1;
          }
  }
}

static void DecodeADDSUB(Word Index)
{
  Boolean OK;
  Integer Shift;
  Word DestAcc;

  if ((ArgCnt < 2) || (ArgCnt > 4)) WrError(1110);
  else
  {
    OpSize = SInt16;
    DecodeAdr(ArgStr[1], MModAcc | MModMem | MModImm);
    switch (AdrMode)
    {
      case ModAcc:  /* ADD src, SHIFT|ASM [,dst] */
        if (ArgCnt == 4) WrError(1110);
        else if (ThisPar) WrError(1950);
        else
        {
          Word SrcAcc = *AdrVals;

          /* SrcAcc remains in AdrVals[0] if no 3rd operand, therefore
             no extra assignment needed! */

          if (ArgCnt == 3)
          {
            if (!DecodeAdr(ArgStr[3], MModAcc))
              break;
          }

          /* distinguish variants of shift specification: */

          if (!strcasecmp(ArgStr[2], "ASM"))
          {
            WAsmCode[0] = 0xf480 | Index | (SrcAcc << 9) | (*AdrVals << 8);
            CodeLen = 1;
          }
          else
          {
            WAsmCode[0] = EvalIntExpression(ArgStr[2], SInt5, &OK);
            if (OK)
            {
              WAsmCode[0] = (WAsmCode[0] & 0x1f) | 0xf400 | (Index << 5) | (SrcAcc << 9) | (*AdrVals << 8);
              CodeLen = 1;
            }
          }
        }
        break;

      case ModMem: /* ADD mem[, TS | SHIFT | Ymem], src[, dst] */
      {
        int HCnt;

        /* rescue decoded address values */

        memcpy(WAsmCode, AdrVals, (AdrCnt + 1) << 1);
        HCnt = AdrCnt;

        /* no shift? this is the case for two operands or three operands and the second is an accumulator */

        if (ArgCnt == 2)
          Shift = 0;
        else if ((ArgCnt == 3) && (IsAcc(ArgStr[2])))
          Shift = 0;

        /* special shift value ? */

        else if (!strcasecmp(ArgStr[2], "TS"))
          Shift = 255;

        /* shift address operand ? */

        else if (*ArgStr[2] == '*')
        {
          Word Tmp;

          /* break down source operand to reduced variant */

          if (!MakeXY(WAsmCode, True))
            break;
          WAsmCode[0] = WAsmCode[0] << 4;

          /* merge in second operand */

          if (!DecodeAdr(ArgStr[2], MModMem))
            break;
          if (!MakeXY(&Tmp, True))
            break;
          WAsmCode[0] |= Tmp;
          Shift = 254;
        }

        /* normal immediate shift */

        else
        {
          Shift = EvalIntExpression(ArgStr[2], SInt6, &OK);
          if (!OK)
            break;
          if ((FirstPassUnknown) && (Shift > 16))
            Shift &= 15;
          if (!ChkRange(Shift, -16 ,16))
            break;
        }

        /* decode destination accumulator */

        if (!DecodeAdr(ArgStr[ArgCnt], MModAcc))
          break;
        DestAcc = *AdrVals;

        /* optionally decode source accumulator.  If no second accumulator, result
           again remains in AdrVals */

        if ((ArgCnt == 4) || ((ArgCnt == 3) && (IsAcc(ArgStr[2]))))
        {
          if (!DecodeAdr(ArgStr[ArgCnt - 1], MModAcc))
            break;
        }

	/* now start applying the variants */        

        if (Shift == 255) /* TS case */
        {
          if (*AdrVals != DestAcc) WrError(1350);
          else if (ThisPar) WrError(1950);
          else
          {
            WAsmCode[0] |= 0x0400 | (Index << 11) | (DestAcc << 8);
            CodeLen = 1 + HCnt;
          }
        }

        else if (Shift == 254) /* XY case */
        {
          if (*AdrVals != DestAcc) WrError(1350);
          else if (ThisPar) WrError(1950);
          else
          {   
            WAsmCode[0] |= 0xa000 | (Index << 9) | (DestAcc << 8);
            CodeLen = 1;
          }
        }

        else if (Shift == 16) /* optimization for 16 shifts */
        {
          if (ThisPar) WrError(1950);
          else
          {
            WAsmCode[0] |= (0x3c00 + (Index << 10)) | (*AdrVals << 9) | (DestAcc << 8);
            CodeLen = 1 + HCnt;
          }
        }

        else if ((DestAcc == *AdrVals) && (Shift == 0)) /* shortform without shift and with one accu only */
        {
          if (ThisPar)
          {
            AdrMode = ModMem; AdrVals[0] = WAsmCode[0];
            if (MakeXY(AdrVals, True))
            {
              /* prev. operation must be STH src,0,Xmem */
              if ((LastOpCode & 0xfe0f) != 0x9a00) WrError(1950);
              else
              {
                RetractWords(1);
                WAsmCode[0] = 0xc000 | (Index << 10) | (DestAcc << 8) | ((LastOpCode & 0x0100) << 1) | ((LastOpCode & 0x00f0) >> 4) | (*AdrVals << 4);
                CodeLen = 1;
              }
            }
          }
          else
          {
            WAsmCode[0] |= 0x0000 | (Index << 11) | (DestAcc << 8);
            CodeLen = 1 + HCnt;
          }
        }

        else if (ThisPar) WrError(1950);
        else
        {
          Word SrcAcc = *AdrVals;

          /* fool MakeXY a bit */

          AdrMode = ModMem; AdrVals[0] = WAsmCode[0];

          if ((Shift >= 0) && (DestAcc == SrcAcc) && (MakeXY(WAsmCode, False))) /* X-Addr and positive shift */
          {
            WAsmCode[0] = 0x9000 | (Index << 9) | (WAsmCode[0] << 4) | (DestAcc << 8) | Shift;
            CodeLen = 1;
          }
          else /* last resort... */
          {
            WAsmCode[0] |= 0x6f00;
            WAsmCode[2] = WAsmCode[1]; /* shift optional address offset */
            WAsmCode[1] = 0x0c00 | (Index << 5) | (SrcAcc << 9) | (DestAcc << 8) | (Shift & 0x1f);
            CodeLen = 2 + HCnt;
          }
        }

        break;
      }

      case ModImm:  /* ADD #lk[, SHIFT|16], src[, dst] */
      {
        if (ThisPar) WrError(1950);
        {
          /* store away constant */

          WAsmCode[1] = *AdrVals;

          /* no shift? this is the case for two operands or three operands and the second is an accumulator */

          if (ArgCnt == 2)
            Shift = 0;
          else if ((ArgCnt == 3) && (IsAcc(ArgStr[2])))
            Shift = 0;

          /* otherwise shift is second argument */

          else
          {
            FirstPassUnknown = False;
            Shift = EvalIntExpression(ArgStr[2], UInt5, &OK);
            if (!OK)
              break;
            if ((FirstPassUnknown) && (Shift > 16))
              Shift &= 15;
            if (!ChkRange(Shift, 0 ,16))
              break;
          }

          /* decode destination accumulator */

          if (!DecodeAdr(ArgStr[ArgCnt], MModAcc))
            break;
          DestAcc = *AdrVals;

          /* optionally decode source accumulator.  If no second accumulator, result
             again remains in AdrVals */

          if ((ArgCnt == 4) || ((ArgCnt == 3) && (IsAcc(ArgStr[2]))))
          {
            if (!DecodeAdr(ArgStr[ArgCnt - 1], MModAcc))
              break;
          }

          /* distinguish according to shift count */

          if (Shift == 16)
          {
            WAsmCode[0] = 0xf060 | Index | (DestAcc << 8) | (*AdrVals << 9);
            CodeLen = 2;
          }
          else
          {
            WAsmCode[0] = 0xf000 | (Index << 5) | (DestAcc << 8) | (*AdrVals << 9) | (Shift & 15);
            CodeLen = 2;
          }
        }
        break;
      }
    }
  }
}

static void DecodeMemAcc(Word Index)
{
  FixedOrder *POrder = MemAccOrders + Index;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if ((LastRep) && (!POrder->IsRepeatable)) WrError(1560);
  else if (DecodeAdr(ArgStr[2], MModAcc))
  {
    WAsmCode[0] = POrder->Code | (AdrVals[0] << 8);
    if (DecodeAdr(ArgStr[1], MModMem))
    {
      WAsmCode[0] |= *AdrVals;
      if (AdrCnt)
        WAsmCode[1] = AdrVals[1];
      CodeLen = 1 + AdrCnt;
    }
  }
}

static void DecodeMemConst(Word Index)
{
  MemConstOrder *POrder = MemConstOrders + Index;
  int HCnt;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if ((LastRep) && (!POrder->IsRepeatable)) WrError(1560);
  else if (DecodeAdr(ArgStr[2 - POrder->Swap], MModMem))
  {
    WAsmCode[0] = POrder->Code | 0[AdrVals];
    if (((HCnt = AdrCnt)))
      WAsmCode[1] = AdrVals[1];
    OpSize = POrder->ConstType;
    if (DecodeAdr(ArgStr[1 +  POrder->Swap], MModImm))
    {
      WAsmCode[1 + HCnt] = *AdrVals;
      CodeLen = 2 + HCnt;
    }
  }
}

static void DecodeMPY(Word Index)
{
  Word DestAcc;

  (void)Index;

  if ((ArgCnt != 2) && (ArgCnt != 3)) WrError(1110);
  else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
  {
    DestAcc = (*AdrVals) << 8;
    if (ArgCnt == 3)
    {
      Word XMode;

      if (ThisPar) WrError(1950);
      else if (DecodeAdr(ArgStr[1], MModMem))
        if (MakeXY(&XMode, True))
          if (DecodeAdr(ArgStr[2], MModMem))
            if (MakeXY(WAsmCode, True))
            {
              *WAsmCode |= 0xa400 | DestAcc | (XMode << 4);
              CodeLen = 1;
            }
    }
    else
    {
      OpSize = SInt16;
      DecodeAdr(ArgStr[1], MModImm | MModMem);
      switch (AdrMode)
      {
        case ModImm:
          if (ThisPar) WrError(1950);
          else
          {
            WAsmCode[0] = 0xf066 | DestAcc;
            WAsmCode[1] = *AdrVals;
            CodeLen = 2;
          }
          break;
        case ModMem:
          if (ThisPar)
          {
            if (MakeXY(AdrVals, True))
            {
              /* previous op ST src, Ym */
              if ((LastOpCode & 0xfe0f) != 0x9a00) WrError(1950);
              else
              {
                RetractWords(1);
                *WAsmCode = 0xcc00 | DestAcc | ((LastOpCode & 0x0100) << 1) | ((LastOpCode & 0x00f0) >> 4) | (*AdrVals);
                CodeLen = 1;
              }
            }
          }
          else
          {
            WAsmCode[0] = 0x2000 | DestAcc | 0[AdrVals];
            if (AdrCnt)
              WAsmCode[1] = AdrVals[1];
            CodeLen = 1 + AdrCnt;
          }
          break;
      }
    }
  }
}

static void DecodeMPYA(Word Index)
{
  (void) Index;

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else
  {
    DecodeAdr(ArgStr[1], MModAcc | MModMem);
    switch (AdrMode)
    {
      case ModMem:
        WAsmCode[0] = 0x3100 | AdrVals[0];
        if (AdrCnt)
          WAsmCode[1] = AdrVals[1];
        CodeLen = 1 + AdrCnt;
        break;
      case ModAcc:
        WAsmCode[0] = 0xf48c | (*AdrVals << 8);
        CodeLen = 1;
        break;
    }
  }
}

static void DecodeSQUR(Word Index)
{
  (void)Index;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[2], MModAcc))
  {
    0[WAsmCode] = *AdrVals << 8;
    DecodeAdr(ArgStr[1], MModAcc | MModMem);
    switch (AdrMode)
    {
      case ModAcc:
        if (*AdrVals) WrError(1350);
        else
        {
          WAsmCode[0] |= 0xf48d;
          CodeLen = 1;
        }
        break;
      case ModMem:
        WAsmCode[0] |= 0x2600 | *AdrVals;
        if (AdrCnt)
          WAsmCode[1] = AdrVals[1];
        CodeLen = 1 + AdrCnt;
        break;
    }
  }
}

static void DecodeMAC(Word Index)
{
  (void) Index;

  if ((ArgCnt < 2) || (ArgCnt > 4)) WrError(1110);
  else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
  {
    *WAsmCode = (*AdrVals) << 8;
    OpSize = SInt16;
    DecodeAdr(ArgStr[1], MModImm | MModMem);

    /* handle syntax 3: immediate op first */

    if (AdrMode == ModImm)
    {
      if (ArgCnt == 4) WrError(1110);
      else if (ThisPar) WrError(1950);
      else
      {
        *WAsmCode |= 0xf067; WAsmCode[1] = *AdrVals;
        if (ArgCnt == 2)
        {
          *WAsmCode |= ((*WAsmCode & 0x100) << 1);
          CodeLen = 2;
        }
        else if (DecodeAdr(ArgStr[2], MModAcc))
        {
          *WAsmCode |= ((*AdrVals) << 9);
          CodeLen = 2;
        }
      }
    }

    /* syntax 1/2/4 have memory operand in front */

    else if (AdrMode == ModMem)
    {
      /* save [first] memory operand */

      Word HMode = *AdrVals, HCnt = AdrCnt;
      if (AdrCnt)
        WAsmCode[1] = AdrVals[1];
     
      /* syntax 2+4 have at least 3 operands, handle syntax 1 */

      if (ArgCnt == 2)
      {
        if (ThisPar)
        {
          if (MakeXY(AdrVals, True))
          {
            if ((LastOpCode & 0xfe0f) == 0x9400) /* previous op LD Xmem, src */
            {
              if ((LastOpCode & 0x0100) == (*WAsmCode & 0x0100)) WrError(1950);
              else
              {
                RetractWords(1);
                *WAsmCode = 0xa800 | (LastOpCode & 0x01f0) | (*AdrVals);
                CodeLen = 1;
              }
            }
            else if ((LastOpCode & 0xfe0f) == 0x9a00) /* previous op ST src, Ymem */
            {
              RetractWords(1);
              *WAsmCode |= 0xd000 | ((LastOpCode & 0x0100) << 1) | ((LastOpCode & 0x00f0) >> 4) | (*AdrVals << 4);
              CodeLen = 1;
            }
            else WrError(1950);
          }
        }
        else
        {
          *WAsmCode |= 0x2800 | HMode;
          CodeLen = 1 + AdrCnt;
        }
      }
      else if (ThisPar) WrError(1950);
      else
      {
        /* both syntax 2+4 have optional second accumulator */

        if (ArgCnt == 3)
          *WAsmCode |= ((*WAsmCode & 0x100) << 1);
        else if (DecodeAdr(ArgStr[3], MModAcc))
          *WAsmCode |= ((*AdrVals) << 9);

        /* if no second accu, AdrMode is still set from previous decode */

        if (AdrMode != ModNone)
        {
          /* differentiate & handle syntax 2 & 4. OpSize still set from above! */

          DecodeAdr(ArgStr[2], MModMem | MModImm);
          switch (AdrMode)
          {
            case ModMem:
              if (MakeXY(AdrVals, TRUE))
              {
                WAsmCode[0] |= (*AdrVals);
                *AdrVals = HMode;
                if (MakeXY(&HMode, TRUE))
                {
                  WAsmCode[0] |= 0xb000 | (HMode << 4);
                  CodeLen = 1;
                }
              }
              break;
            case ModImm:
              WAsmCode[1 + HCnt] = *AdrVals;
              WAsmCode[0] |= 0x6400 | HMode;
              CodeLen = 2 + HCnt;
              break;
          }
        }
      }
    }
  }
}

static void DecodeMACDP(Word Index)
{
  Boolean OK;

  if (ArgCnt != 3) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[3], MModAcc))
  {
    *WAsmCode = Index | (0[AdrVals] << 8);
    if (DecodeAdr(ArgStr[1], MModMem))
    {
      *WAsmCode |= *AdrVals;
      if (AdrCnt)
        WAsmCode[1] = AdrVals[1];
      WAsmCode[1 + AdrCnt] = EvalIntExpression(ArgStr[2], UInt16, &OK);
      if (OK)
      {
        ChkSpace(Index & 0x200 ? SegData : SegCode);
        CodeLen = 2 + AdrCnt;
      }
    }
  }
}

static void DecodeFIRS(Word Index)
{
  Boolean OK;

  (void)Index;
 
  if (ArgCnt != 3) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModMem))
    if (MakeXY(WAsmCode, TRUE))
    {
      0[WAsmCode] = 0xe000 | ((*WAsmCode) << 4);
      if (DecodeAdr(ArgStr[2], MModMem))
        if (MakeXY(AdrVals, TRUE))
        {
          0[WAsmCode] |= *AdrVals;
          WAsmCode[1] = EvalIntExpression(ArgStr[3], UInt16, &OK);
          if (OK)
          {
            ChkSpace(SegCode);
            CodeLen = 2;
          }
        }
    }
}

static void DecodeBIT(Word Index)
{
  Boolean OK;

  (void)Index;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModMem))
    if (MakeXY(AdrVals, TRUE))
    {
      WAsmCode[0] = EvalIntExpression(ArgStr[2], UInt4, &OK);
      if (OK)
      {
        WAsmCode[0] |= 0x9600 | (AdrVals[0] << 4);
        CodeLen = 1;
      }
    }
}

static void DecodeBITF(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else
  {
    OpSize = UInt16;
    if (DecodeAdr(ArgStr[2], MModImm))
    {
      WAsmCode[1] = *AdrVals;
      if (DecodeAdr(ArgStr[1], MModMem))
      {
        *WAsmCode = 0x6100 | *AdrVals;
        if (AdrCnt)
        {
          WAsmCode[2] = WAsmCode[1];
          WAsmCode[1] = AdrVals[1];
        }
        CodeLen = 2 + AdrCnt;
      }
    }
  }
}

static void DecodeMACR(Word Index)
{
  (void) Index;

  if ((ArgCnt < 2) || (ArgCnt > 4)) WrError(1110);
  else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
  {
    *WAsmCode = *AdrVals << 8;
    if (DecodeAdr(ArgStr[1], MModMem))
    {
      if (ArgCnt == 2)
      {
        if (ThisPar)
        {
          if (MakeXY(AdrVals, True))
          {
            if ((LastOpCode & 0xfe0f) == 0x9400) /* previous op LD Xmem, src */
            {
              if ((LastOpCode & 0x0100) == (*WAsmCode & 0x0100)) WrError(1950);
              else
              {
                RetractWords(1);
                *WAsmCode = 0xaa00 | (LastOpCode & 0x01f0) | (*AdrVals);
                CodeLen = 1;
              }
            }
            else if ((LastOpCode & 0xfe0f) == 0x9a00) /* previous op ST src, Ymem */
            {
              RetractWords(1);
              *WAsmCode |= 0xd400 | ((LastOpCode & 0x0100) << 1) | ((LastOpCode & 0x00f0) >> 4) | (*AdrVals << 4);
              CodeLen = 1;
            }
            else WrError(1950);
          }
        }
        else
        {
          WAsmCode[0] |= 0x2a00 | *AdrVals;
          if (AdrCnt)
            WAsmCode[1] = AdrVals[1];
          CodeLen = 1 + AdrCnt;
        }
      }
      else if (ThisPar) WrError(1950);
      else
      {
        if (MakeXY(AdrVals, True))
        {
          WAsmCode[0] |= 0xb400 | ((*AdrVals) << 4);
          if (DecodeAdr(ArgStr[2], MModMem))
            if (MakeXY(AdrVals, True))
            {
              WAsmCode[0] |= *AdrVals;
              if (ArgCnt == 4)
              {
                if (DecodeAdr(ArgStr[3], MModAcc))
                  WAsmCode[0] |= (*AdrVals) << 9;
              }
              else
                *WAsmCode |= ((*WAsmCode & 0x100) << 1);
              if (AdrMode != ModNone)
                CodeLen = 1;
            }
        }
      }
    }
  }
}

static void DecodeMac(Word Index)
{
  FixedOrder *POrder = MacOrders + Index;

  if (!ArgCnt) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (!strcasecmp(ArgStr[1], "T"))
  {
    if (ArgCnt > 3) WrError(1110);
    else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
    {
      WAsmCode[0] = 0xf480 | (POrder->Code & 0xff) | ((*AdrVals) << 8);
      if (ArgCnt == 3)
        DecodeAdr(ArgStr[2], MModAcc);
      if (AdrMode != ModNone)
      {
        WAsmCode[0] |= ((*AdrVals) << 9);
        CodeLen = 1;
      }
    }
  }
  else if (ArgCnt > 2) WrError(1110);
  else
  {
    if ((ArgCnt == 2) && (strcasecmp(ArgStr[2], "B"))) WrError(1350);
    else if (DecodeAdr(ArgStr[1], MModMem))
    {
      WAsmCode[0] = (POrder->Code & 0xff00) | (*AdrVals);
      if (AdrCnt)
        WAsmCode[1] = AdrVals[1];
      CodeLen = 1 + AdrCnt;
    }
  }
}

static void DecodeMACSU(Word Index)
{
  (void)Index;

  if (ArgCnt != 3) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[3], MModAcc))
  {
    *WAsmCode = 0xa600 | ((*AdrVals) << 8);
    if (DecodeAdr(ArgStr[1], MModMem))
      if (MakeXY(AdrVals, TRUE))
      {
        *WAsmCode |= ((*AdrVals) << 4);
        if (DecodeAdr(ArgStr[2], MModMem))
          if (MakeXY(AdrVals, TRUE))
          {
            *WAsmCode |= *AdrVals;
            CodeLen = 1;
          }
      }
  }
}

static void DecodeMAS(Word Index)
{
  if ((ArgCnt < 2) || (ArgCnt > 4)) WrError(1110);
  else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
  {
    *WAsmCode = ((*AdrVals) << 8);
    if (DecodeAdr(ArgStr[1], MModMem))
    {
      if (ArgCnt == 2)
      {
        if (ThisPar)
        {
          if (MakeXY(AdrVals, True))
          {
            if ((LastOpCode & 0xfe0f) == 0x9400) /* previous op LD Xmem, src */
            {
              if ((LastOpCode & 0x0100) == (*WAsmCode & 0x0100)) WrError(1950);
              else
              {
                RetractWords(1);
                *WAsmCode = 0xac00 | Index | (LastOpCode & 0x01f0) | (*AdrVals);
                CodeLen = 1;
              }
            }
            else if ((LastOpCode & 0xfe0f) == 0x9a00) /* previous op ST src, Ymem */
            {
              RetractWords(1);
              *WAsmCode |= 0xd800 | (Index << 1) | ((LastOpCode & 0x0100) << 1) | ((LastOpCode & 0x00f0) >> 4) | (*AdrVals << 4);
              CodeLen = 1;
            }
            else WrError(1950);
          }
        }
        else
        {
          *WAsmCode |= 0x2c00 | Index | *AdrVals;
          if (AdrCnt)
            1[WAsmCode] = AdrVals[1];
          CodeLen = 1 + AdrCnt;
        }
      }
      else if (ThisPar) WrError(950);
      else if (MakeXY(AdrVals, TRUE))
      {
        *WAsmCode |= 0xb800 | (Index << 1) | ((*AdrVals) << 4);
        if (DecodeAdr(ArgStr[2], MModMem))
          if (MakeXY(AdrVals, TRUE))
          {
            *WAsmCode |= *AdrVals;
            if (ArgCnt == 4)
            {
              if (DecodeAdr(ArgStr[3], MModAcc))
                *WAsmCode |= ((*AdrVals) << 9);
            }
            else
              *WAsmCode |= ((*WAsmCode & 0x100) << 1);
            if (AdrMode != ModNone)
              CodeLen = 1;            
          }
      }
    }
  }
}

static void DecodeMASAR(Word Index)
{
  (void)Index;

  if ((ArgCnt < 2) || (ArgCnt > 3)) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (strcasecmp(ArgStr[1], "T")) WrError(1350);
  else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
  {
    WAsmCode[0] = (*AdrVals << 8);
    if (ArgCnt == 3)
      DecodeAdr(ArgStr[2], MModAcc);
    if (AdrMode != ModNone)
    {
      *WAsmCode |= 0xf48b | ((*AdrVals) << 9);
      CodeLen = 1;
    }
  }
}

static void DecodeDADD(Word Index)
{
  (void)Index;

  if ((ArgCnt < 2) || (ArgCnt > 3)) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
  {
    WAsmCode[0] = 0x5000 | (AdrVals[0] << 8);
    if (ArgCnt == 3)
      DecodeAdr(ArgStr[2], MModAcc);
    if (AdrMode != ModNone)
    {
      WAsmCode[0] |= (AdrVals[0] << 9);
      if (DecodeAdr(ArgStr[1], MModMem))
      {
        WAsmCode[0] |= AdrVals[0];
        if (AdrCnt)
          WAsmCode[1] = AdrVals[1];
        CodeLen = 1 + AdrCnt;
      }
    }
  }
}

static void DecodeLog(Word Index)
{
  Word Acc, Shift;
  Boolean OK;

  if ((ArgCnt < 1) || (ArgCnt > 4)) WrError(1110);
  else if (ThisPar) WrError(1950);
  else
  {
    OpSize = UInt16;
    DecodeAdr(ArgStr[1], MModAcc | MModMem | MModImm);
    switch (AdrMode)
    {
      case ModAcc:  /* Variant 4 */
        if (ArgCnt == 4) WrError(1110);
        else
        {
          Acc = *AdrVals << 9;
          *WAsmCode = 0xf080 | Acc | (Index << 5);
          Shift = 0; OK = True;
          if (((ArgCnt == 2) && IsAcc(ArgStr[2])) || (ArgCnt == 3))
          {
            OK = DecodeAdr(ArgStr[ArgCnt], MModAcc);
            if (OK)
              Acc = *AdrVals << 8;
          }
          else
            Acc = Acc >> 1;
          if (OK)
            if (((ArgCnt == 2) && (!IsAcc(ArgStr[2]))) || (ArgCnt == 3))
            {
              Shift = EvalIntExpression(ArgStr[2], SInt5, &OK);
            }
          if (OK)
          {
            *WAsmCode |= Acc | (Shift & 0x1f);
            CodeLen = 1;
          }
        }
        break;

      case ModMem:  /* Variant 1 */
        if (ArgCnt != 2) WrError(1110);
        else  
        {
          *WAsmCode = 0x1800 | (*AdrVals) | (Index << 9);
          if (AdrCnt)
            WAsmCode[1] = AdrVals[1];
          CodeLen = AdrCnt + 1;
          if (DecodeAdr(ArgStr[2], MModAcc))
            *WAsmCode |= (*AdrVals) << 8;
          else
            CodeLen = 0;
        }
        break;

      case ModImm:  /* Variant 2,3 */
        if (ArgCnt == 1) WrError(1110);
        else  
        {     
          WAsmCode[1] = *AdrVals;
          if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
          {
            *WAsmCode = Acc = *AdrVals << 8;
            Shift = 0; OK = True;
            if (((ArgCnt == 3) && IsAcc(ArgStr[2])) || (ArgCnt == 4))
            {
               OK = DecodeAdr(ArgStr[ArgCnt - 1], MModAcc);
               if (OK)
                Acc = (*AdrVals) << 9;
            }
            else
              Acc = Acc << 1;
            if (OK)
              if (((ArgCnt == 3) && (!IsAcc(ArgStr[2]))) || (ArgCnt == 4))
              {
                FirstPassUnknown = False;
                Shift = EvalIntExpression(ArgStr[2], UInt5, &OK);
                if (FirstPassUnknown)
                  Shift &= 15;
                OK = ChkRange(Shift, 0, 16);
              }
            if (OK)
            {
              *WAsmCode |= Acc;
              if (Shift == 16) /* Variant 3 */
              {
                *WAsmCode |= 0xf063 + Index;
              }
              else             /* Variant 2 */
              {
                *WAsmCode |= 0xf000 | ((Index + 3) << 4) | Shift;
              }
              CodeLen = 2;
            }
          }
        }
        break;
    }
  }
}

static void DecodeSFT(Word Index)
{
  Boolean OK;
  int Shift;

  if ((ArgCnt != 2) && (ArgCnt != 3)) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModAcc))
  {
    0[WAsmCode] = Index | ((*AdrVals) << 9);
    if (ArgCnt == 3)
      DecodeAdr(ArgStr[3], MModAcc);
    if (AdrMode != ModNone)
    {
      0[WAsmCode] |= ((*AdrVals) << 8);
      Shift = EvalIntExpression(ArgStr[2], SInt5, &OK);
      if (OK)
      {
        0[WAsmCode] |= (Shift & 0x1f);
        CodeLen = 1;
      }
    }
  }
}

static void DecodeCMPR(Word Index)
{
  Word z;
  Boolean OK;

  (void) Index;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[2], MModAReg))
  {
    OK = False;
    for (z = 0; z < 4; z++)
      if (!strcasecmp(ArgStr[1], ShortConds[z]))
      {
        OK = True;
        break;
      }
    if (!OK)
      z = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (OK)
    {
      0[WAsmCode] = 0xf4a8 | (*AdrVals) | (z << 8);
      CodeLen = 1;
    }
  }
}

static void DecodePMAD(Word Index)
{
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else
  {
    WAsmCode[1] = EvalIntExpression(ArgStr[1], UInt16, &OK);
    if (OK)
    {
      ChkSpace(SegCode);
      0[WAsmCode] = Index;
      CodeLen = 2;
    }
  }
}

static void DecodeBANZ(Word Index)
{
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else if (DecodeAdr(ArgStr[2], MModMem))
  {
    if (CodeLen)
      WAsmCode[1] = 1[AdrVals];
    WAsmCode[1 + CodeLen] = EvalIntExpression(ArgStr[1], UInt16, &OK);
    if (OK)
    {
      ChkSpace(SegCode);
      0[WAsmCode] = Index | (*AdrVals);
      CodeLen = 2 + AdrCnt;
    }
  }
}

static void DecodePMADCond(Word Index)
{
  Boolean OK;
  int index;

  if (ArgCnt < 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else if (!DecodeCondition(2, WAsmCode, &index, &OK))
    WrXError(OK ? 1360 : 1365, ArgStr[index]);
  else
  {
    WAsmCode[1] = EvalIntExpression(ArgStr[1], UInt16, &OK);
    if (OK)
    {
      ChkSpace(SegCode);
      0[WAsmCode] |= Index;
      CodeLen = 2;
    }
  }
}

static void DecodeFPMAD(Word Index)
{
  Boolean OK;
  LongWord Addr;

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else
  {
    Addr = EvalIntExpression(ArgStr[1], UInt23, &OK);
    if (OK)
    {
      ChkSpace(SegCode);
      0[WAsmCode] = Index | (Addr >> 16);
      1[WAsmCode] = Addr & 0xffff;
      CodeLen = 2;
    }
  }
}

static void DecodeINTR(Word Index)
{
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else
  {   
    *WAsmCode = Index | EvalIntExpression(ArgStr[1], UInt5, &OK);
    if (OK)
      CodeLen = 1;
  }
}

static void DecodeRPT(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else
  {
    OpSize = UInt16;
    DecodeAdr(ArgStr[1], MModImm | MModMem);
    switch (AdrMode)
    {
      case ModImm:
        if (!Hi(*AdrVals))
        {
          *WAsmCode = 0xec00 | (*AdrVals);
          CodeLen = 1;
        }
        else
        {
          *WAsmCode = 0xf070;
          WAsmCode[1] = *AdrVals;
          CodeLen = 2;
        }
        ThisRep = True;
        break;
      case ModMem:
        WAsmCode[0] = 0x4700 | (*AdrVals);
        if (AdrCnt)
          WAsmCode[1] = AdrVals[1];
        CodeLen = 1 + AdrCnt;
        ThisRep = True;
        break;
    }
  }
}

static void DecodeRPTZ(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else if (DecodeAdr(ArgStr[1], MModAcc))
  {
    *WAsmCode = 0xf071 | (AdrVals[0] << 8);
    OpSize = UInt16;
    if (DecodeAdr(ArgStr[2], MModImm))
    {
      WAsmCode[1] = *AdrVals;
      CodeLen = 2;
      ThisRep = True;
    }
  }
}

static void DecodeFRAME(Word Index)
{
  Boolean OK;

  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else
  {
    *WAsmCode = 0xee00 | (EvalIntExpression(ArgStr[1], SInt8, &OK) & 0xff);
    if (OK)
      CodeLen = 1;
  }
}

static void DecodeIDLE(Word Index)
{
  Boolean OK;

  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else
  {
    FirstPassUnknown = False;
    *WAsmCode = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (FirstPassUnknown)
      *WAsmCode = 1;
    if ((OK) && (ChkRange(*WAsmCode, 1, 3)))
    {
      if (*WAsmCode != 2)
        *WAsmCode = (*WAsmCode) >> 1;
      *WAsmCode = 0xf4e1 | (*WAsmCode << 8);
      CodeLen = 1;
    }
  }
}

static void DecodeSBIT(Word Index)
{
  Boolean OK;
  Word Bit;

  if ((ArgCnt < 1) || (ArgCnt > 2)) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else
  {
    if (ArgCnt == 1)
      Bit = EvalIntExpression(ArgStr[1], UInt5, &OK);
    else
    {
      Bit = EvalIntExpression(ArgStr[1], UInt1, &OK) << 4;
      if (OK)
        Bit |= EvalIntExpression(ArgStr[2], UInt4, &OK);
    }
    if (OK)
    {
      *WAsmCode = Index | ((Bit & 16) << 5) | (Bit & 15);
      CodeLen = 1;
    }
  }
}

static void DecodeXC(Word Index)
{
  Boolean OK;
  int errindex;

  UNUSED(Index);

  if (ArgCnt < 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (LastRep) WrError(1560);
  else if (!DecodeCondition(2, WAsmCode, &errindex, &OK))
    WrXError(OK ? 1360 : 1365, ArgStr[errindex]);
  else
  {
    FirstPassUnknown = False;
    errindex = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (FirstPassUnknown)
      errindex = 1;
    if ((OK) && (ChkRange(errindex, 1, 2)))
    {
      errindex--;
      0[WAsmCode] |= 0xfd00 | (errindex << 9);
      CodeLen = 1;
    }
  }
}

static void DecodeLD(Word Index)
{
  Integer Shift;
  Boolean OK;

  UNUSED(Index);

  if ((ArgCnt < 2) || (ArgCnt > 3)) WrError(1110);

  else if (!strcasecmp(ArgStr[ArgCnt], "T"))
  {
    if (ArgCnt != 2) WrError(1110);
    else if (DecodeAdr(ArgStr[1], MModMem))
    {
      if (ThisPar)
      {
        if (MakeXY(AdrVals, TRUE))
        {
          /* prev. operation must be STH src,0,Xmem */
          if ((LastOpCode & 0xfe0f) != 0x9a00) WrError(1950);
          else
          {
            RetractWords(1);
            *WAsmCode = 0xe400 | ((LastOpCode & 0x0100) << 1)
                        | ((LastOpCode & 0x00f0) >> 4) | ((*AdrVals) << 4);
            CodeLen = 1;
          }
        }
      }
      else
      {
        WAsmCode[0] = 0x3000 | AdrVals[0];
        if (AdrCnt)
          WAsmCode[1] = AdrVals[1];
        CodeLen = 1 + AdrCnt;
      }
    }
  }
  
  else if (!strcasecmp(ArgStr[ArgCnt], "DP"))
  {
    if (ArgCnt != 2) WrError(1110);
    else if (ThisPar) WrError(1950);
    else
    {
      OpSize = UInt9;
      DecodeAdr(ArgStr[1], MModMem | MModImm);
      switch (AdrMode)
      {
        case ModMem:
          WAsmCode[0] = 0x2600 | AdrVals[0];
          if (AdrCnt)
            WAsmCode[1] = AdrVals[1];
          CodeLen = 1 + AdrCnt;
          break;
        case ModImm:
          WAsmCode[0] = 0xea00 | (AdrVals[0] & 0x1ff);
          CodeLen = 1;
          break;
      }
    }
  }

  else if (!strcasecmp(ArgStr[ArgCnt], "ARP"))
  {
    if (ArgCnt != 2) WrError(1110);
    else if (ThisPar) WrError(1950);
    else
    {   
      OpSize = UInt3;
      if (DecodeAdr(ArgStr[1], MModImm))
      {
        WAsmCode[0] = 0xf4a0 | (AdrVals[0] & 7);
        CodeLen = 1;
      }
    }
  }

  else if (!strcasecmp(ArgStr[2], "ASM"))
  {
    if (ThisPar) WrError(1950);
    else
    {
      OpSize = SInt5;
      DecodeAdr(ArgStr[1], MModAcc | MModMem | MModImm);
      switch (AdrMode)
      {
        case ModAcc:
          WAsmCode[0] = *AdrVals << 9;
          if (ArgCnt == 3)
            DecodeAdr(ArgStr[3], MModAcc);
          if (AdrMode == ModAcc)
          {
            WAsmCode[0] |= 0xf482 | (AdrVals[0] << 8);
            CodeLen = 1;
          }
          break;
        case ModMem:
          if (ArgCnt != 2) WrError(1110);
          else
          {
            WAsmCode[0] = 0x3200 | AdrVals[0];
            if (AdrCnt)
              WAsmCode[1] = AdrVals[1];
            CodeLen = 1 + AdrCnt;
          }
          break;
        case ModImm:
          if (ArgCnt != 2) WrError(1110);
          else
          {   
            WAsmCode[0] = 0xed00 | (AdrVals[0] & 0x1f);
            CodeLen = 1;
          }
          break;
      }
    }
  }

  else if (DecodeAdr(ArgStr[ArgCnt], MModAcc))
  {
    *WAsmCode = *AdrVals << 8;
    if (ArgCnt == 3)
    {
      if (!strcasecmp(ArgStr[2], "TS"))
      {
        Shift = 0xff;
        OK = True;
      }
      else
      {
        FirstPassUnknown = 0;
        Shift = EvalIntExpression(ArgStr[2], SInt6, &OK);
        if (FirstPassUnknown)
          Shift = 0;
        if (OK)
          OK = ChkRange(Shift, -16, 16);
      }
    }
    else
    {
      Shift = 0;
      OK = True;
    }
    if (OK)
    {
      OpSize = UInt16;
      DecodeAdr(ArgStr[1], MModAcc | MModMem | MModImm);
      switch (AdrMode)
      {
        case ModAcc:
          if (ThisPar) WrError(1950);
          else if (ChkRange(Shift, -16, 15))
          {
            *WAsmCode |= 0xf440 | (AdrVals[0] << 9) | (Shift & 0x1f);
            CodeLen = 1;
          }
          break;
        case ModMem:
          if (Shift == 0xff) /* TS ? */
          {
            if (ThisPar) WrError(1950);
            else
            {
              *WAsmCode |= 0x1400 | AdrVals[0];
              if (AdrCnt)
                WAsmCode[1] = AdrVals[1];
              CodeLen = 1 + AdrCnt;
            }
          }
          else if ((Shift >= 0) && (MakeXY(WAsmCode + 1, False)))
          {
            if (ThisPar)
            {
              if (Shift) WrError(1950);
              /* prev. operation must be STH src,0,Xmem */
              else if ((LastOpCode & 0xfe0f) != 0x9a00) WrError(1950);
              else
              {
                RetractWords(1);
                *WAsmCode |= 0xc800 | ((LastOpCode & 0x0100) << 1)
                             | ((LastOpCode & 0x00f0) >> 4) | (WAsmCode[1] << 4);
                CodeLen = 1;
              }
            }
            else
            {
              WAsmCode[0] |= 0x9400 | (WAsmCode[1] << 4) | Shift;
              CodeLen = 1;
            }
          }
          else if (Shift == 16)
          {
            if (ThisPar) WrError(1950);
            else
            {   
              WAsmCode[0] |= 0x4400 | AdrVals[0];
              if (AdrCnt)
                WAsmCode[1] = AdrVals[1];
              CodeLen = 1 + AdrCnt;
            }
          }
          else if (!Shift)
          {
            if (ThisPar) WrError(1950);
            else
            {
              WAsmCode[0] |= 0x1000 | AdrVals[0];
              if (AdrCnt)
                WAsmCode[1] = AdrVals[1];
              CodeLen = 1 + AdrCnt;
            }
          }
          else
          {
            if (ThisPar) WrError(1950);
            else
            {
              WAsmCode[1 + AdrCnt] = 0x0c40 | WAsmCode[0] | (Shift & 0x1f);
              WAsmCode[0] = 0x6f00 | AdrVals[0];
              if (AdrCnt)
                WAsmCode[1] = AdrVals[1];
              CodeLen = 2 + AdrCnt;
            }
          }
          break;
        case ModImm:
          if (Shift == 0xff) WrError(1350);
          else if (ThisPar) WrError(1950);
          else if (ChkRange(Shift, 0, 16))
          {
            if ((Hi(AdrVals[0]) == 0) && (!Shift))
            {
              WAsmCode[0] |= 0xe800 | Lo(AdrVals[0]);
              CodeLen = 1;
            }
            else if (Shift == 16)
            {
              WAsmCode[0] |= 0xf062;
              WAsmCode[1] = AdrVals[0];
              CodeLen = 2;
            }
            else
            {
              WAsmCode[0] |= 0xf020 | (Shift & 15);
              WAsmCode[1] = AdrVals[0];
              CodeLen = 2;
            }
          }
          break;
      }
    }
  }
}

static void DecodePSHM(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else if (ThisPar) WrError(1950);
  else
  {
    ForcePageZero = True;
    if (DecodeAdr(ArgStr[1], MModMem))
    {
      *WAsmCode = 0x4a00 | (*AdrVals);
      CodeLen = 1;
    }
  }
}

static void DecodeLDM(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[2], MModAcc))
  {
    *WAsmCode = 0x4800 | (*AdrVals << 8);
    ForcePageZero = True;
    if (DecodeAdr(ArgStr[1], MModMem))
    {
      *WAsmCode |= *AdrVals;
      CodeLen = 1;
    }
  }
}

static void DecodeSTLM(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModAcc))
  {
    *WAsmCode = 0x8800 | (*AdrVals << 8);
    ForcePageZero = True;
    if (DecodeAdr(ArgStr[2], MModMem))
    {
      *WAsmCode |= *AdrVals;
      CodeLen = 1;
    }
  }
}

static void DecodeSTM(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModImm))
  {
    WAsmCode[1] = *AdrVals;
    ForcePageZero = True;
    if (DecodeAdr(ArgStr[2], MModMem))
    {
      *WAsmCode = 0x7700 | (*AdrVals);
      CodeLen = 2;
    }
  }
}

static void DecodeDST(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModAcc))
  {
    *WAsmCode = 0x4e00 | (*AdrVals << 8);
    if (DecodeAdr(ArgStr[2], MModMem))
    {
      *WAsmCode |= *AdrVals;
      if (AdrCnt)
       1[WAsmCode] = 1[AdrVals];
      CodeLen = 1 + AdrCnt;
    }
  }
}

static void DecodeST(Word Index)
{
  Word Acc;

  UNUSED(Index);

  /* NOTE: we also allow the form 'ST src,Xmem' as an alias
     for 'STH src,Ymem' since the first form is used in parallel
     load-store instructions. */

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[2], MModMem))
  {
    *WAsmCode = *AdrVals;
    if (AdrCnt)
      1[WAsmCode] = 1[AdrVals];
    CodeLen = 1 + AdrCnt;
    OpSize = SInt16;
    if (!strcasecmp(ArgStr[1], "T"))
      *WAsmCode |= 0x8c00;
    else if (!strcasecmp(ArgStr[1], "TRN"))
      *WAsmCode |= 0x8d00;
    else
    {
      DecodeAdr(ArgStr[1], MModImm | MModAcc);
      switch (AdrMode)
      {
        case ModImm:
          *WAsmCode |= 0x7600;
          (CodeLen++)[WAsmCode] = *AdrVals;
          break;
        case ModAcc:
          Acc = *AdrVals;
          *AdrVals = *WAsmCode; AdrMode = ModMem;
          if (MakeXY(AdrVals, True))
          {
            *WAsmCode = 0x9a00 | (Acc << 8) | (*AdrVals << 4);
            CodeLen = 1;
          }
          else
            CodeLen = 0;
          break;
        default:
          CodeLen = 0;
      }
    }
  }
}

static void DecodeSTLH(Word Index)
{
  Integer Shift;
  Boolean OK;

  if ((ArgCnt < 2) || (ArgCnt > 3)) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModAcc))
  {
    *WAsmCode = Index | (*AdrVals << 8);
    OK = True; 
    if (ArgCnt == 2)
      Shift = 0;
    else if (!strcasecmp(ArgStr[2], "ASM"))
      Shift = 0xff;
    else
      Shift = EvalIntExpression(ArgStr[2], SInt5, &OK);
    if ((OK) && (DecodeAdr(ArgStr[ArgCnt], MModMem)))
    {
      if (AdrCnt)
        1[WAsmCode] = 1[AdrVals];
      CodeLen = 1 + AdrCnt;
      if (!Shift)
        *WAsmCode |= 0x8000 | *AdrVals;
      else if (Shift == 0xff)
        *WAsmCode |= 0x8400 | *AdrVals;
      else if ((MakeXY(WAsmCode + 2, False)) && (Shift > 0))
        *WAsmCode |= 0x9800 | (2[WAsmCode] << 4) | Shift;
      else
      {
        CodeLen[WAsmCode] = (0x0c80 | ((*WAsmCode) & 0x100) | (Shift & 0x1f)) - (Index >> 4);
        *WAsmCode = 0x6f00 | *AdrVals;
        CodeLen++;
      }
    }
  }
}

static void DecodeCMPS(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModAcc))
  {
    *WAsmCode = 0x8e00 | (*AdrVals << 8);
    if (DecodeAdr(ArgStr[2], MModMem))
    {
      *WAsmCode |= *AdrVals;
      if (AdrCnt)
       1[WAsmCode] = 1[AdrVals];
      CodeLen = 1 + AdrCnt;
    }
  }
}

static void DecodeSACCD(Word Index)
{
  int index;
  Boolean OK;

  UNUSED(Index);

  if (ArgCnt != 3) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModAcc))
  {
    *WAsmCode = 0x9e00 | ((*AdrVals) << 8);
    if (DecodeAdr(ArgStr[2], MModMem))
      if (MakeXY(AdrVals, True))
      {
        *WAsmCode |= ((*AdrVals) << 4);
        if (!DecodeCondition(3, WAsmCode + 1, &index, &OK)) WrXError(1360, ArgStr[index]);
        else if ((WAsmCode[1] & 0xf0) != 0x40) WrXError(1360, ArgStr[index]);
        else
        {
          *WAsmCode |= WAsmCode[1] & 15;
          CodeLen = 1;
        }
      }
  }
}

static void DecodeStoreCC(Word Index)
{
  int index;
  Boolean OK;

  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModMem))
    if (MakeXY(AdrVals, True))
    {
      *WAsmCode = Index | ((*AdrVals) << 4);
      if (!DecodeCondition(2, WAsmCode + 1, &index, &OK)) WrXError(1360, ArgStr[index]);
      else if ((WAsmCode[1] & 0xf0) != 0x40) WrXError(1360, ArgStr[index]);
      else
      {
        *WAsmCode |= WAsmCode[1] & 15;
        CodeLen = 1;
      }
    }
}

static void DecodeMVDabs(Word Index)
{
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[1], MModMem))
  {
    *WAsmCode = Index | *AdrVals;
    if (AdrCnt)
      1[WAsmCode] = 1[AdrVals];
    WAsmCode[1 + AdrCnt] = EvalIntExpression(ArgStr[2], UInt16, &OK);
    if (OK)
    {
      ChkSpace((Index == 0x7100) ? SegData : SegCode);
      CodeLen = 2 + AdrCnt;
    }
  }
}

static void DecodeMVabsD(Word Index)
{
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if (DecodeAdr(ArgStr[2], MModMem))
  {
    *WAsmCode = Index | *AdrVals;
    if (AdrCnt)
      1[WAsmCode] = 1[AdrVals];
    WAsmCode[1 + AdrCnt] = EvalIntExpression(ArgStr[1], UInt16, &OK);
    if (OK)
    {
      ChkSpace((Index == 0x7000) ? SegData : SegCode);
      CodeLen = 2 + AdrCnt;
    }
  }
}

static void DecodeMVdmadmmr(Word Index)
{
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else
  {
    if (Index & 0x0100)
    {
      strcpy(ArgStr[3], ArgStr[1]);
      strcpy(ArgStr[1], ArgStr[2]);
      strcpy(ArgStr[2], ArgStr[3]);
    }
    ForcePageZero = True;
    if (DecodeAdr(ArgStr[2], MModMem))
    {
      *WAsmCode = Index | *AdrVals;
      WAsmCode[1] = EvalIntExpression(ArgStr[1], UInt16, &OK);
      if (OK)
      { 
        ChkSpace(SegData);
        CodeLen = 2;
      }
    }
  }
}

static Boolean GetReg(char *Asc, Word *Res)
{
  Boolean OK;

  FirstPassUnknown = False;
  *Res = EvalIntExpression(Asc, UInt8, &OK);
  if (OK)
  {
    if (FirstPassUnknown)
      *Res = 0x10;
    ChkSpace(SegData);
    OK = ChkRange(*Res, 0x10, 0x1f);
    if (OK)
      *Res -= 0x10;
  }
  return OK;
}

static void DecodeMVMM(Word Index)
{
  Word XReg, YReg;
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else if ((GetReg(ArgStr[1], &XReg)) && (GetReg(ArgStr[2], &YReg)))
  {
    *WAsmCode = 0xe700 | (XReg << 4) | YReg;
    CodeLen = 1;
  }
}

static void DecodePort(Word Index)
{
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else if (ThisPar) WrError(1950);
  else
  {
    if (Index & 0x0100)
    {
      strcpy(ArgStr[3], ArgStr[1]);
      strcpy(ArgStr[1], ArgStr[2]);
      strcpy(ArgStr[2], ArgStr[3]);
    }
    if (DecodeAdr(ArgStr[2], MModMem))
    {
      *WAsmCode = Index | *AdrVals;
      if (AdrCnt)
        1[WAsmCode] = 1[AdrVals];
      WAsmCode[1 + AdrCnt] = EvalIntExpression(ArgStr[1], UInt16, &OK);
      if (OK)
      {
        ChkSpace(SegIO);
        CodeLen = 2 + AdrCnt;
      }
    }
  }
}

/*-------------------------------------------------------------------------*/
/* Pseudo Instructions */

static Boolean DecodePseudo(void)
{
  if (Memo("PORT"))
  {
    CodeEquate(SegIO,0,65535);
    return True;
  }

  return False;
}

/*-------------------------------------------------------------------------*/
/* Code Table Handling */

static void AddFixed(char *Name, Word Code, Boolean IsRepeatable)
{
  if (InstrZ >= FixedOrderCnt)
    exit(0);

  FixedOrders[InstrZ].Code         = Code;
  FixedOrders[InstrZ].IsRepeatable = IsRepeatable;
  AddInstTable(InstTable, Name, InstrZ++, DecodeFixed);
}

static void AddAcc(char *Name, Word Code, Boolean IsRepeatable)
{
  if (InstrZ >= AccOrderCnt)
    exit(0);

  AccOrders[InstrZ].Code         = Code;
  AccOrders[InstrZ].IsRepeatable = IsRepeatable;
  AddInstTable(InstTable, Name, InstrZ++, DecodeAcc);
}

static void AddAcc2(char *Name, Word Code, Boolean IsRepeatable)
{
  if (InstrZ >= Acc2OrderCnt)
    exit(0);

  Acc2Orders[InstrZ].Code         = Code;
  Acc2Orders[InstrZ].IsRepeatable = IsRepeatable;
  AddInstTable(InstTable, Name, InstrZ++, DecodeAcc2);
}

static void AddMem(char *Name, Word Code, Boolean IsRepeatable)
{
  if (InstrZ >= MemOrderCnt)
    exit(0);

  MemOrders[InstrZ].Code         = Code;
  MemOrders[InstrZ].IsRepeatable = IsRepeatable;
  AddInstTable(InstTable, Name, InstrZ++, DecodeMem);
}

static void AddXY(char *Name, Word Code, Boolean IsRepeatable)
{
  if (InstrZ >= XYOrderCnt)
    exit(0);

  XYOrders[InstrZ].Code         = Code;
  XYOrders[InstrZ].IsRepeatable = IsRepeatable;
  AddInstTable(InstTable, Name, InstrZ++, DecodeXY);
}

static void AddMemAcc(char *Name, Word Code, Boolean IsRepeatable)
{
  if (InstrZ >= MemAccOrderCnt)
    exit(0);

  MemAccOrders[InstrZ].Code         = Code;
  MemAccOrders[InstrZ].IsRepeatable = IsRepeatable;
  AddInstTable(InstTable, Name, InstrZ++, DecodeMemAcc);
}

static void AddMemConst(char *Name, Word Code, Boolean IsRepeatable, Boolean Swap, IntType ConstType)
{
  if (InstrZ >= MemConstOrderCnt)
    exit(0);

  MemConstOrders[InstrZ].Code         = Code;
  MemConstOrders[InstrZ].IsRepeatable = IsRepeatable;
  MemConstOrders[InstrZ].Swap         = Swap;
  MemConstOrders[InstrZ].ConstType    = ConstType;
  AddInstTable(InstTable, Name, InstrZ++, DecodeMemConst);
}

static void AddMac(char *Name, Word Code, Boolean IsRepeatable)
{
  if (InstrZ >= MacOrderCnt)
    exit(0);

  MacOrders[InstrZ].Code         = Code;
  MacOrders[InstrZ].IsRepeatable = IsRepeatable;
  AddInstTable(InstTable, Name, InstrZ++, DecodeMac);
}

static void AddCondition(char *NName, Word NClass, Word NCode, Word NMask)
{
  if (InstrZ >= ConditionCnt)
    exit(0);
  Conditions[InstrZ  ].Name  = NName;
  Conditions[InstrZ  ].Class = NClass;
  Conditions[InstrZ  ].Code  = NCode;
  Conditions[InstrZ++].Mask  = NMask;
}

static void InitFields(void)
{
  InstTable = CreateInstTable(203);

  AddInstTable(InstTable, "LD", 0, DecodeLD);
  AddInstTable(InstTable, "ST", 0, DecodeST);
  AddInstTable(InstTable, "STH", 0x200, DecodeSTLH);
  AddInstTable(InstTable, "STL", 0x000, DecodeSTLH);

  FixedOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * FixedOrderCnt); InstrZ = 0;
  AddFixed("FRET"  , 0xf4e4, FALSE);
  AddFixed("FRETD" , 0xf6e4, FALSE);
  AddFixed("FRETE" , 0xf4e5, FALSE);
  AddFixed("FRETED", 0xf6e5, FALSE);
  AddFixed("NOP"   , NOPCode, TRUE);
  AddFixed("RESET" , 0xf7e0, FALSE);
  AddFixed("RET"   , 0xfc00, FALSE);
  AddFixed("RETD"  , 0xfe00, FALSE);
  AddFixed("RETE"  , 0xf4eb, FALSE);
  AddFixed("RETED" , 0xf6eb, FALSE);
  AddFixed("RETF"  , 0xf49b, FALSE);
  AddFixed("RETFD" , 0xf69b, FALSE);

  AccOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * AccOrderCnt); InstrZ = 0;
  AddAcc  ("EXP"   , 0xf48e, TRUE );
  AddAcc  ("MAX"   , 0xf486, TRUE );
  AddAcc  ("MIN"   , 0xf487, TRUE );
  AddAcc  ("SAT"   , 0xf483, TRUE );
  AddAcc  ("ROL"   , 0xf491, TRUE );
  AddAcc  ("ROLTC" , 0xf492, TRUE );
  AddAcc  ("ROR"   , 0xf490, TRUE );
  AddAcc  ("SFTC"  , 0xf494, TRUE );
  AddAcc  ("CALA"  , 0xf4e3, FALSE);
  AddAcc  ("CALAD" , 0xf6e3, FALSE);
  AddAcc  ("FCALA" , 0xf4e7, FALSE);
  AddAcc  ("FCALAD", 0xf6e7, FALSE);
  AddAcc  ("BACC"  , 0xf4e2, FALSE);
  AddAcc  ("BACCD" , 0xf6e2, FALSE);
  AddAcc  ("FBACC" , 0xf4e6, FALSE);
  AddAcc  ("FBACCD", 0xf6e6, FALSE);

  Acc2Orders = (FixedOrder*) malloc(sizeof(FixedOrder) * Acc2OrderCnt); InstrZ = 0;
  AddAcc2 ("NEG"   , 0xf484, TRUE );
  AddAcc2 ("NORM"  , 0xf48f, TRUE );
  AddAcc2 ("RND"   , 0xf49f, TRUE );
  AddAcc2 ("ABS"   , 0xf485, TRUE );
  AddAcc2 ("CMPL"  , 0xf493, TRUE );

  MemOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * MemOrderCnt); InstrZ = 0;
  AddMem  ("DELAY" , 0x4d00, TRUE );
  AddMem  ("POLY"  , 0x3600, TRUE );
  AddMem  ("BITT"  , 0x3400, TRUE );
  AddMem  ("POPD"  , 0x8b00, TRUE );
  AddMem  ("PSHD"  , 0x4b00, TRUE );
  AddMem  ("MAR"   , 0x6d00, TRUE );
  AddMem  ("LTD"   , 0x4c00, TRUE );
  AddMem  ("READA" , 0x7e00, TRUE );
  AddMem  ("WRITA" , 0x7f00, TRUE );

  XYOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * XYOrderCnt); InstrZ = 0;
  AddXY   ("ABDST" , 0xe300, TRUE );
  AddXY   ("LMS"   , 0xe100, TRUE );
  AddXY   ("SQDST" , 0xe200, TRUE );
  AddXY   ("MVDD"  , 0xe500, TRUE );

  AddInstTable(InstTable, "ADD", 0, DecodeADDSUB);
  AddInstTable(InstTable, "SUB", 1, DecodeADDSUB);

  MemAccOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * MemAccOrderCnt); InstrZ = 0;
  AddMemAcc("ADDC" , 0x0600, TRUE );
  AddMemAcc("ADDS" , 0x0200, TRUE );
  AddMemAcc("SUBB" , 0x0e00, TRUE );
  AddMemAcc("SUBC" , 0x1e00, TRUE );
  AddMemAcc("SUBS" , 0x0a00, TRUE );
  AddMemAcc("MPYR" , 0x2200, TRUE );
  AddMemAcc("MPYU" , 0x2400, TRUE );
  AddMemAcc("SQURA", 0x3800, TRUE );
  AddMemAcc("SQURS", 0x3a00, TRUE );
  AddMemAcc("DADST", 0x5a00, TRUE );
  AddMemAcc("DRSUB", 0x5800, TRUE );
  AddMemAcc("DSADT", 0x5e00, TRUE );
  AddMemAcc("DSUB" , 0x5400, TRUE );
  AddMemAcc("DSUBT", 0x5c00, TRUE );
  AddMemAcc("DLD"  , 0x5600, TRUE );
  AddMemAcc("LDR"  , 0x1600, TRUE );
  AddMemAcc("LDU"  , 0x1200, TRUE );

  MacOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * MacOrderCnt); InstrZ = 0;
  AddMac("MACA"  , 0x3508, TRUE );
  AddMac("MACAR" , 0x3709, TRUE );
  AddMac("MASA"  , 0x330a, TRUE );

  MemConstOrders = (MemConstOrder*) malloc(sizeof(MemConstOrder) * MemConstOrderCnt); InstrZ = 0;
  AddMemConst("ADDM", 0x6b00, FALSE, FALSE, SInt16);
  AddMemConst("ANDM", 0x6800, FALSE, FALSE, UInt16);
  AddMemConst("CMPM", 0x6000, TRUE , TRUE , SInt16);
  AddMemConst("ORM" , 0x6900, FALSE, FALSE, UInt16);
  AddMemConst("XORM", 0x6a00, FALSE, FALSE, UInt16);

  AddInstTable(InstTable, "AND"  , 0, DecodeLog);
  AddInstTable(InstTable, "OR"   , 1, DecodeLog);
  AddInstTable(InstTable, "XOR"  , 2, DecodeLog);
  AddInstTable(InstTable, "MPY"  , 0, DecodeMPY);
  AddInstTable(InstTable, "MPYA" , 0, DecodeMPYA);
  AddInstTable(InstTable, "SQUR" , 0, DecodeSQUR);
  AddInstTable(InstTable, "MAC"  , 0, DecodeMAC);
  AddInstTable(InstTable, "MACR" , 0, DecodeMACR);
  AddInstTable(InstTable, "MACD" , 0x7a00, DecodeMACDP);
  AddInstTable(InstTable, "MACP" , 0x7800, DecodeMACDP);
  AddInstTable(InstTable, "MACSU", 0, DecodeMACSU);
  AddInstTable(InstTable, "MAS"  , 0x000, DecodeMAS);
  AddInstTable(InstTable, "MASR" , 0x200, DecodeMAS);
  AddInstTable(InstTable, "MASAR", 0, DecodeMASAR);
  AddInstTable(InstTable, "DADD" , 0, DecodeDADD);
  AddInstTable(InstTable, "FIRS" , 0, DecodeFIRS);
  AddInstTable(InstTable, "SFTA" , 0xf460, DecodeSFT);
  AddInstTable(InstTable, "SFTL" , 0xf0e0, DecodeSFT);
  AddInstTable(InstTable, "BIT"  , 0, DecodeBIT);
  AddInstTable(InstTable, "BITF" , 0, DecodeBITF);
  AddInstTable(InstTable, "CMPR" , 0, DecodeCMPR);

  AddInstTable(InstTable, "B"    , 0xf073, DecodePMAD);
  AddInstTable(InstTable, "BD"   , 0xf273, DecodePMAD);
  AddInstTable(InstTable, "CALL" , 0xf074, DecodePMAD);
  AddInstTable(InstTable, "CALLD", 0xf274, DecodePMAD);
  AddInstTable(InstTable, "RPTB" , 0xf072, DecodePMAD);
  AddInstTable(InstTable, "RPTBD", 0xf272, DecodePMAD);

  AddInstTable(InstTable, "BC"   , 0xf800, DecodePMADCond);
  AddInstTable(InstTable, "BCD"  , 0xfa00, DecodePMADCond);
  AddInstTable(InstTable, "CC"   , 0xf900, DecodePMADCond);
  AddInstTable(InstTable, "CCD"  , 0xfb00, DecodePMADCond);

  AddInstTable(InstTable, "FB"    , 0xf880, DecodeFPMAD);
  AddInstTable(InstTable, "FBD"   , 0xfa80, DecodeFPMAD);
  AddInstTable(InstTable, "FCALL" , 0xf980, DecodeFPMAD);
  AddInstTable(InstTable, "FCALLD", 0xfb80, DecodeFPMAD);

  AddInstTable(InstTable, "BANZ" , 0x6c00, DecodeBANZ);
  AddInstTable(InstTable, "BANZD", 0x6e00, DecodeBANZ);

  AddInstTable(InstTable, "INTR" , 0xf7c0, DecodeINTR);
  AddInstTable(InstTable, "TRAP" , 0xf4c0, DecodeINTR);
  AddInstTable(InstTable, "PSHM" , 0     , DecodePSHM);
  AddInstTable(InstTable, "LDM"  , 0     , DecodeLDM);
  AddInstTable(InstTable, "STLM" , 0     , DecodeSTLM);
  AddInstTable(InstTable, "STM"  , 0     , DecodeSTM);
  AddInstTable(InstTable, "CMPS" , 0     , DecodeCMPS);

  AddInstTable(InstTable, "RPT"  , 0     , DecodeRPT);
  AddInstTable(InstTable, "RPTZ" , 0     , DecodeRPTZ);

  AddInstTable(InstTable, "FRAME", 0     , DecodeFRAME);
  AddInstTable(InstTable, "IDLE" , 0     , DecodeIDLE);

  AddInstTable(InstTable, "RSBX" , 0xf4b0, DecodeSBIT);
  AddInstTable(InstTable, "SSBX" , 0xf5b0, DecodeSBIT);

  AddInstTable(InstTable, "XC"   , 0     , DecodeXC);

  AddInstTable(InstTable, "DST"  , 0     , DecodeDST);

  AddInstTable(InstTable, "SACCD", 0     , DecodeSACCD);

  AddInstTable(InstTable, "SRCCD", 0x9d00, DecodeStoreCC);
  AddInstTable(InstTable, "STRCD", 0x9c00, DecodeStoreCC);

  AddInstTable(InstTable, "MVDK" , 0x7100, DecodeMVDabs);
  AddInstTable(InstTable, "MVDP" , 0x7d00, DecodeMVDabs);
  AddInstTable(InstTable, "MVKD" , 0x7000, DecodeMVabsD);
  AddInstTable(InstTable, "MVPD" , 0x7c00, DecodeMVabsD);
  AddInstTable(InstTable, "MVDM" , 0x7200, DecodeMVdmadmmr);
  AddInstTable(InstTable, "MVMD" , 0x7300, DecodeMVdmadmmr);
  AddInstTable(InstTable, "MVMM" , 0x7300, DecodeMVMM);

  AddInstTable(InstTable, "PORTR", 0x7400, DecodePort);
  AddInstTable(InstTable, "PORTW", 0x7500, DecodePort);

  Conditions = (Condition*) malloc(sizeof(Condition) * ConditionCnt); InstrZ = 0;
  AddCondition("BIO"  , 0, 0x0003, 0x0003);
  AddCondition("NBIO" , 0, 0x0002, 0x0003);
  AddCondition("C"    , 0, 0x000c, 0x000c);
  AddCondition("NC"   , 0, 0x0008, 0x000c);
  AddCondition("TC"   , 0, 0x0030, 0x0030);
  AddCondition("NTC"  , 0, 0x0020, 0x0030);
  AddCondition("AEQ"  , 1, 0x0045, 0x000f);
  AddCondition("ANEQ" , 1, 0x0044, 0x000f);
  AddCondition("AGT"  , 1, 0x0046, 0x000f);
  AddCondition("AGEQ" , 1, 0x0042, 0x000f);
  AddCondition("ALT"  , 1, 0x0043, 0x000f);
  AddCondition("ALEQ" , 1, 0x0047, 0x000f);
  AddCondition("AOV"  , 1, 0x0070, 0x00f0);
  AddCondition("ANOV" , 1, 0x0060, 0x00f0);
  AddCondition("BEQ"  , 2, 0x004d, 0x000f);
  AddCondition("BNEQ" , 2, 0x004c, 0x000f);
  AddCondition("BGT"  , 2, 0x004e, 0x000f);
  AddCondition("BGEQ" , 2, 0x004a, 0x000f);
  AddCondition("BLT"  , 2, 0x004b, 0x000f);
  AddCondition("BLEQ" , 2, 0x004f, 0x000f);
  AddCondition("BOV"  , 2, 0x0078, 0x00f0);
  AddCondition("BNOV" , 2, 0x0068, 0x00f0);
  AddCondition("UNC"  , 3, 0x0000, 0x00ff);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
  free(FixedOrders);
  free(AccOrders);
  free(Acc2Orders);
  free(MemOrders);
  free(XYOrders);
  free(MemAccOrders);
  free(MemConstOrders);
  free(MacOrders);
  free(Conditions);
}

/*-------------------------------------------------------------------------*/
/* Linking Routines */

static void MakeCode_32054x(void)
{
  CodeLen = 0; DontPrint = False;

   ThisPar = !strcmp(LabPart, "||");
   if ((strlen(OpPart) > 2) && (!strncmp(OpPart, "||", 2)))
   {
     ThisPar = True; strmov(OpPart, OpPart + 2);
   }

  /* zu ignorierendes */

  if (*OpPart == '\0') return;

  if (DecodePseudo()) return;

  if (DecodeTIPseudo()) return;

  /* search */

  ThisRep = False; ForcePageZero = False;
  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200, OpPart);
  else
    LastOpCode = *WAsmCode;
  LastRep = ThisRep;
}

static void InitCode_32054x(void)
{
  SaveInitProc();
  Reg_CPL = 0;
  Reg_DP = 0;
  Reg_SP = 0;
}

static Boolean IsDef_32054x(void)
{
  return (!strcmp(LabPart, "||"));
}

static void SwitchFrom_32054x(void)
{
  DeinitFields();
}

static void SwitchTo_32054x(void)
{
  PFamilyDescr FoundDescr;

  FoundDescr = FindFamilyByName("TMS320C54x");

  TurnWords = False; ConstMode = ConstModeIntel; SetIsOccupied = False;

  PCSymbol = "$"; HeaderID = FoundDescr->Id; NOPCode = 0xf495;
  DivideChars = ","; HasAttrs = False;

  ValidSegs = (1 << SegCode) | (1 << SegData) | (1 << SegIO);
  Grans[SegCode] = 2; ListGrans[SegCode] = 2; SegInits[SegCode] = 2; SegLimits[SegCode] = 0xffff;
  Grans[SegData] = 2; ListGrans[SegData] = 2; SegInits[SegData] = 2; SegLimits[SegData] = 0xffff;
  Grans[SegIO  ] = 2; ListGrans[SegIO  ] = 2; SegInits[SegIO  ] = 2; SegLimits[SegIO  ] = 0xffff;

  MakeCode = MakeCode_32054x; IsDef = IsDef_32054x;
  
  pASSUMERecs = ASSUME3254xs;
  ASSUMERecCnt = ASSUME3254xCount;

  InitFields(); SwitchFrom = SwitchFrom_32054x;
  ThisRep = LastRep = False;
  LastOpCode = 0;
}

/*-------------------------------------------------------------------------*/
/* Global Interface */

void code32054x_init(void)
{
  CPU320C541 = AddCPU("320C541", SwitchTo_32054x);

   SaveInitProc=InitPassProc; InitPassProc=InitCode_32054x;
}
