/* code97c241.c */
/*****************************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only                     */
/*                                                                           */
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator TLCS-9000                                                   */
/*                                                                           */
/*****************************************************************************/

#include "stdinc.h"

#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "nls.h"
#include "strutil.h"
#include "bpemu.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmitree.h"
#include "codepseudo.h"
#include "intpseudo.h"
#include "codevars.h"
#include "errmsg.h"

#include "code97c241.h"

typedef struct 
{
  Byte Code;
  Byte Mask;     /* B0..2=OpSizes, B4=-MayImm, B5=-MayReg */
} RMWOrder;

typedef struct
{
  char *Name;
  Byte Code;
  Byte Mask;     /* B7: DD in A-Format gedreht */
  enum { Equal, FirstCounts, SecondCounts, Op2Half } SizeType;
  Boolean ImmKorr, ImmErl, RegErl;
} GAOrder;
           

#define ConditionCount 20
#define RMWOrderCount 14


static CPUVar CPU97C241;

static int OpSize, OpSize2;
static Integer LowLim4, LowLim8;

static Boolean AdrOK;
static Word AdrMode, AdrMode2;
static Byte AdrCnt2;
static Word AdrVals[2], AdrVals2[2];
static int AdrInc;
static Word Prefs[2];
static Boolean PrefUsed[2];
static char Format;
static Boolean MinOneIs0;

static RMWOrder *RMWOrders;
static char **Conditions;

/*--------------------------------------------------------------------------*/

static int CheckForcePrefix(const char *pArg, Boolean *pForce)
{
  if (*pArg == '>')
  {
    *pForce = True;
    return 1;
  }
  return 0;
}

static void AddSignedPrefix(Byte Index, Byte MaxBits, LongInt Value, Boolean Force)
{
  LongInt Max;

  Max = 1l << (MaxBits -1);
  if (Force || ((Value < -Max) || (Value >= Max)))
  {
    PrefUsed[Index] = True;
    Prefs[Index] = (Value >> MaxBits) & 0x7ff;
  }
}

static Boolean AddRelPrefix(Byte Index, Byte MaxBits, LongInt *Value, Boolean Force)
{
  LongInt Max1,Max2;

  Max1 = 1l << (MaxBits - 1);
  Max2 = 1l << (MaxBits + 10);
  if ((*Value < -Max2) || (*Value >= Max2)) WrError(ErrNum_JmpDistTooBig);
  else
  {
    if (Force || ((*Value < -Max1) || (*Value >= Max1)))
    {
      PrefUsed[Index] = True;
      Prefs[Index] = ((*Value) >> MaxBits) & 0x7ff;
    }
    return True;
  }
  return False;
}

static void AddAbsPrefix(Byte Index, Byte MaxBits, LongInt Value, Boolean Force)
{
  LongInt Dist;

  Dist = 1l << (MaxBits - 1);
  if (Force || ((Value >= Dist) && (Value < 0x1000000 - Dist)))
  {
    PrefUsed[Index] = True;
    Prefs[Index] = (Value >> MaxBits) & 0x7ff;
  }
}

static void InsertSinglePrefix(Byte Index)
{
  if (PrefUsed[Index])
  {
    memmove(WAsmCode + 1, WAsmCode + 0, CodeLen);
    WAsmCode[0] = Prefs[Index] + 0xd000 + (((Word)Index) << 11);
    CodeLen += 2;
  }
}

static Boolean DecodeReg(char *Asc, Byte *Result)
{
  Byte tmp;
  Boolean Err;
  int l = strlen(Asc);

  if ((l > 4) || (l < 3) || (mytoupper(*Asc) != 'R')) return False;
  switch (mytoupper(Asc[1]))
  {
    case 'B': 
      *Result = 0x00;
      break;
    case 'W':
      *Result = 0x40;
      break;
    case 'D':
      *Result = 0x80;
      break;
    default:
      return False;
  }
  tmp = ConstLongInt(Asc + 2, &Err, 10);
  if ((!Err) || (tmp > 15))
    return False;
  if ((*Result == 0x80) && (Odd(tmp)))
    return False;
  *Result += tmp;
  return True;
}

static Boolean DecodeSpecReg(char *Asc, Byte *Result)
{
  if (!strcasecmp(Asc, "SP")) *Result = 0x8c;
  else if (!strcasecmp(Asc, "ISP")) *Result = 0x81;
  else if (!strcasecmp(Asc, "ESP")) *Result = 0x83;
  else if (!strcasecmp(Asc, "PBP")) *Result = 0x05;
  else if (!strcasecmp(Asc, "CBP")) *Result = 0x07;
  else if (!strcasecmp(Asc, "PSW")) *Result = 0x89;
  else if (!strcasecmp(Asc, "IMC")) *Result = 0x0b;
  else if (!strcasecmp(Asc, "CC"))  *Result = 0x0e;
  else return False;
  return True;
}

static Boolean DecodeRegAdr(char *Asc, Byte *Erg)
{
  if (!DecodeReg(Asc,Erg))
    return False;
  if (OpSize == -1)
    OpSize = (*Erg) >> 6;
  if (((*Erg) >> 6) != OpSize)
  {
    WrError(ErrNum_UndefOpSizes);
    return False;
  }
  *Erg &= 0x3f;
  return True;
}

static void DecodeAdr(const tStrComp *pArg, Byte PrefInd, Boolean MayImm, Boolean MayReg)
{
#define FreeReg 0xff
#define SPReg 0xfe
#define PCReg 0xfd

  Byte Reg;
  String AdrPartStr;
  tStrComp AdrPart, Remainder;
  Boolean OK;
  int ArgLen;

  AdrCnt = 0; AdrOK = False;
  StrCompMkTemp(&AdrPart, AdrPartStr);

   /* I. Speicheradresse */

  if (IsIndirect(pArg->Str))
  {
    Boolean ForcePrefix = False, MinFlag, NMinFlag;
    tStrComp Arg;
    String tmp;
    char *PMPos, *EPos;
    Byte BaseReg, IndReg, ScaleFact;
    LongInt DispAcc;

    /* I.1. vorkonditionieren */

    StrCompRefRight(&Arg, pArg, 1);
    StrCompShorten(&Arg, 1);
    KillPrefBlanksStrCompRef(&Arg);
    KillPostBlanksStrComp(&Arg);

    /* I.2. Predekrement */

    if ((*Arg.Str == '-')
     && (Arg.Str[1] == '-')
     && (DecodeReg(Arg.Str + 2, &Reg)))
    {
      switch (Reg >> 6)
      {
        case 0:
          WrError(ErrNum_InvAddrMode);
           break;
        case 1:
          AdrMode = 0x50 + (Reg & 15);
          AdrOK = True;
          break;
        case 2:
          AdrMode = 0x71 + (Reg & 14);
          AdrOK = True;
          break;
      }
      return;
    }

    /* I.3. Postinkrement */

    ArgLen = strlen(Arg.Str);
    if ((Arg.Str[ArgLen - 1] == '+') && (Arg.Str[ArgLen - 2] == '+'))
    {
      StrCompCopy(&AdrPart, &Arg);
      StrCompShorten(&AdrPart, 2);
      if (DecodeReg(AdrPart.Str, &Reg))
      {
        switch (Reg >> 6)
        {
          case 0:
            WrError(ErrNum_InvAddrMode);
            break;
          case 1:
            AdrMode = 0x40 + (Reg & 15);
            AdrOK = True;
            break;
          case 2:
            AdrMode = 0x70 + (Reg & 14);
            AdrOK = True;
            break;
        }
        return;
      }
    }

    /* I.4. Adresskomponenten zerlegen */

    BaseReg = FreeReg;
    IndReg = FreeReg;
    ScaleFact = 0;
    DispAcc = AdrInc;
    MinFlag = False;
    do
    {
      /* I.4.a. Trennzeichen suchen */

      PMPos = QuotMultPos(Arg.Str, "-+");
      NMinFlag = (PMPos && (*PMPos == '-'));
      if (PMPos)
      {
        StrCompSplitRef(&Arg, &Remainder, &Arg, PMPos);
        KillPostBlanksStrComp(&Arg);
        KillPrefBlanksStrCompRef(&Remainder);
      }

      /* I.4.b. Indexregister mit Skalierung */

      EPos = QuotPos(Arg.Str, '*');
      if (EPos)
      {
        strcpy(tmp, Arg.Str);
        tmp[EPos - Arg.Str] = '\0';
        KillPostBlanks(tmp);
      }
      ArgLen = strlen(Arg.Str);
      if ((EPos == Arg.Str + ArgLen - 2) 
       && ((Arg.Str[ArgLen - 1] == '1') || (Arg.Str[ArgLen - 1] == '2') || (Arg.Str[ArgLen - 1] == '4') || (Arg.Str[ArgLen - 1] == '8'))
       && (DecodeReg(tmp, &Reg)))
      {
        if (((Reg >> 6) == 0) || (MinFlag) || (IndReg != FreeReg))
        {
          WrError(ErrNum_InvAddrMode);
          return;
        }
        IndReg = Reg;
        switch (Arg.Str[ArgLen - 1])
        {
          case '1':
            ScaleFact = 0;
            break;
          case '2':
            ScaleFact = 1;
            break;
          case '4':
            ScaleFact = 2;
            break;
          case '8':
            ScaleFact = 3;
            break;
        }
      }

      /* I.4.c. Basisregister */

      else if (DecodeReg(Arg.Str, &Reg))
      {
        if (((Reg >> 6) == 0) || (MinFlag))
        {
          WrError(ErrNum_InvAddrMode);
          return;
        }
        if (BaseReg == FreeReg)
          BaseReg = Reg;
        else if (IndReg == FreeReg)
        {
          IndReg = Reg;
          ScaleFact = 0;
        }
        else
        {
          WrError(ErrNum_InvAddrMode);
          return;
        }
      }

      /* I.4.d. Sonderregister */

      else if ((!strcasecmp(Arg.Str, "PC")) || (!strcasecmp(Arg.Str, "SP")))
      {
        if ((BaseReg != FreeReg) && (IndReg == FreeReg))
        {
          IndReg = BaseReg;
          BaseReg = FreeReg;
          ScaleFact = 0;
        };
        if ((BaseReg != FreeReg) || (MinFlag))
        {
          WrError(ErrNum_InvAddrMode);
          return;
        }
//#warning here
        BaseReg = strcasecmp(Arg.Str, "SP") ? PCReg : SPReg;
      }

      /* I.4.e. Displacement */

      else
      {
        LongInt DispPart;

        FirstPassUnknown = False;
        DispPart = EvalStrIntExpressionOffs(&Arg, CheckForcePrefix(Arg.Str, &ForcePrefix), Int32, &OK);
        if (!OK)
          return;
        if (FirstPassUnknown)
          DispPart = 1;
        DispAcc = MinFlag ? DispAcc - DispPart : DispAcc + DispPart;
      }
       
      if (PMPos)
        Arg = Remainder;
      MinFlag = NMinFlag;
    }
    while (PMPos);

    /* I.5. Indexregister mit Skalierung 1 als Basis behandeln */

    if ((BaseReg == FreeReg) && (IndReg != FreeReg) && (ScaleFact == 0))
    {
      BaseReg = IndReg; IndReg = FreeReg;
    }

    /* I.6. absolut */

    if ((BaseReg == FreeReg) && (IndReg == FreeReg))
    {
      AdrMode = 0x20; /* 0x60 should be equivalent: adding 0 as RW0 or RD0 is irrelvant */
      AdrVals[0] = 0xe000 + (DispAcc & 0x1fff); AdrCnt = 2;
      AddAbsPrefix(PrefInd, 13, DispAcc, ForcePrefix);
      AdrOK = True;
      return;
    }

    /* I.7. Basis [mit Displacement] */

    if ((BaseReg != FreeReg) && (IndReg == FreeReg))
    {
      /* I.7.a. Basis ohne Displacement */

      if (DispAcc == 0)
      {
        if ((BaseReg >> 6) == 1)
          AdrMode = 0x10 + (BaseReg & 15);
        else
          AdrMode = 0x61 + (BaseReg & 14);
        AdrOK = True;
        return;
      }

      /* I.7.b. Nullregister mit Displacement muss in Erweiterungswort */

      else if ((BaseReg & 15) == 0)
      {
        if (DispAcc > 0x7ffff) WrError(ErrNum_OverRange);
        else if (DispAcc < -0x80000) WrError(ErrNum_UnderRange);
        else
        {
          AdrMode = 0x20;
          if ((BaseReg >> 6) == 1)
            AdrVals[0] = ((Word)BaseReg & 15) << 11;
          else
            AdrVals[0] = (((Word)BaseReg & 14) << 11) + 0x8000;
          AdrVals[0] += DispAcc & 0x1ff;
          AdrCnt = 2;
          AddSignedPrefix(PrefInd, 9, DispAcc, ForcePrefix);
          AdrOK = True;
        }
        return;
      }

      /* I.7.c. Stack mit Displacement: Optimierung moeglich */

      else if (BaseReg == SPReg)
      {
        if (DispAcc > 0x7ffff) WrError(ErrNum_OverRange);
        else if (DispAcc < -0x80000) WrError(ErrNum_UnderRange);
        else if ((DispAcc >= 0) && (DispAcc <= 127))
        {
          AdrMode = 0x80 + (DispAcc & 0x7f);
          AdrOK = True;
        }
        else
        {
          AdrMode = 0x20;
          AdrVals[0] = 0xd000 + (DispAcc & 0x1ff); AdrCnt = 2;
          AddSignedPrefix(PrefInd, 9, DispAcc, ForcePrefix);
          AdrOK = True;
        }
        return;
      }

      /* I.7.d. Programmzaehler mit Displacement: keine Optimierung */

      else if (BaseReg == PCReg)
      {
        if (DispAcc > 0x7ffff) WrError(ErrNum_OverRange);
        else if (DispAcc < -0x80000) WrError(ErrNum_UnderRange);
        else
        {
          AdrMode = 0x20;
          AdrVals[0] = 0xd800 + (DispAcc & 0x1ff);
          AdrCnt = 2;
          AddSignedPrefix(PrefInd, 9, DispAcc, ForcePrefix);
          AdrOK = True;
        }
        return;
      }

      /* I.7.e. einfaches Basisregister mit Displacement */

      else
      {
        if (DispAcc > 0x7fffff) WrError(ErrNum_OverRange);
        else if (DispAcc < -0x800000) WrError(ErrNum_UnderRange);
        else
        {
          if ((BaseReg >> 6) == 1)
            AdrMode = 0x20 + (BaseReg & 15);
          else
            AdrMode = 0x60 + (BaseReg & 14);
          AdrVals[0] = 0xe000 + (DispAcc & 0x1fff);
          AdrCnt = 2;
          AddSignedPrefix(PrefInd, 13, DispAcc, ForcePrefix);
          AdrOK = True;
        }
        return;
      }
    }

    /* I.8. Index- [und Basisregister] */

    else
    {
      if (DispAcc > 0x7ffff) WrError(ErrNum_OverRange);
      else if (DispAcc < -0x80000) WrError(ErrNum_UnderRange);
      else if ((IndReg & 15) == 0) WrError(ErrNum_InvAddrMode);
      else
      {
        if ((IndReg >> 6) == 1)
          AdrMode = 0x20 + (IndReg & 15);
        else
          AdrMode = 0x60 + (IndReg & 14);
        switch (BaseReg)
        {
          case FreeReg:
            AdrVals[0] = 0xc000; break;
          case SPReg:
            AdrVals[0] = 0xd000; break;
          case PCReg:
            AdrVals[0] = 0xd800; break;
          case 0x40: case 0x41: case 0x42: case 0x43:
          case 0x44: case 0x45: case 0x46: case 0x47:
          case 0x48: case 0x49: case 0x4a: case 0x4b:
          case 0x4c: case 0x4d: case 0x4e: case 0x4f:
            AdrVals[0] = ((Word)BaseReg & 15) << 11; break;
          case 0x80: case 0x81: case 0x82: case 0x83:
          case 0x84: case 0x85: case 0x86: case 0x87:
          case 0x88: case 0x89: case 0x8a: case 0x8b:
          case 0x8c: case 0x8d: case 0x8e:
            AdrVals[0] = 0x8000 + (((Word)BaseReg & 14) << 10); break;
        }
        AdrVals[0] += (((Word)ScaleFact) << 9) + (DispAcc & 0x1ff);
        AdrCnt = 2;
        AddSignedPrefix(PrefInd, 9, DispAcc, ForcePrefix);
        AdrOK = True;
      }
      return;
    }
  }

  /* II. Arbeitsregister */

  else if (DecodeReg(pArg->Str, &Reg))
  {
    if (!MayReg) WrError(ErrNum_InvAddrMode);
    else
    {
      if (OpSize == -1)
        OpSize = Reg >> 6;
      if ((Reg >> 6) != OpSize) WrError(ErrNum_ConfOpSizes);
      else
      {
        AdrMode = Reg & 15;
        AdrOK = True;
      }
    }
    return;
  }

  /* III. Spezialregister */

  else if (DecodeSpecReg(pArg->Str, &Reg))
  {
    if (!MayReg) WrError(ErrNum_InvAddrMode);
    else
    {
      if (OpSize == -1)
        OpSize=Reg >> 6;
      if ((Reg >> 6) != OpSize) WrError(ErrNum_ConfOpSizes);
      else
      {
        AdrMode = 0x30 + (Reg & 15);
        AdrOK = True;
      }
    }
    return;
  }

  else if (!MayImm) WrError(ErrNum_InvAddrMode);
  else
  {
    if ((OpSize == -1) && (MinOneIs0))
      OpSize = 0;
    if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
    else
    {
      AdrMode = 0x30;
      switch (OpSize)
      {
        case 0:
          AdrVals[0] = EvalStrIntExpression(pArg, Int8, &OK) & 0xff;
          if (OK)
          {
            AdrCnt = 2;
            AdrOK = True;
          }
          break;
        case 1:
          AdrVals[0] = EvalStrIntExpression(pArg, Int16, &OK);
          if (OK)
          {
            AdrCnt = 2;
            AdrOK = True;
          }
          break;
        case 2:
        {
          LongInt DispAcc = EvalStrIntExpression(pArg, Int32, &OK);

          if (OK)
          {
            AdrVals[0] = DispAcc & 0xffff;
            AdrVals[1] = DispAcc >> 16;
            AdrCnt = 4;
            AdrOK = True;
          }
          break;
        }
      }
    }
  }
}

static void CopyAdr(void)
{
  OpSize2 = OpSize;
  AdrMode2 = AdrMode;
  AdrCnt2 = AdrCnt;
  memcpy(AdrVals2, AdrVals, AdrCnt);
}

static Boolean IsReg(void)
{
  return (AdrMode <= 15);
}

static Boolean Is2Reg(void)
{
  return (AdrMode2 <= 15);
}

static Boolean IsImmediate(void)
{
  return (AdrMode == 0x30);
}

static Boolean Is2Immediate(void)
{
  return (AdrMode2 == 0x30);
}

static LongInt ImmVal(void)
{
  LongInt Tmp1;
  Integer Tmp2;
  ShortInt Tmp3;

  switch (OpSize)
  {
    case 0:
      Tmp3 = AdrVals[0] & 0xff;
      return Tmp3;
    case 1:
      Tmp2 = AdrVals[0];
      return Tmp2;
    case 2:
      Tmp1 = (((LongInt)AdrVals[1]) << 16) + AdrVals[0];
      return Tmp1;
    default:
      WrError(ErrNum_InternalError);
      return 0;
  }
}

static LongInt ImmVal2(void)
{
  LongInt Tmp1;
  Integer Tmp2;
  ShortInt Tmp3;

  switch (OpSize)
  {
    case 0:
      Tmp3 = AdrVals2[0] & 0xff;
      return Tmp3;
    case 1:
      Tmp2 = AdrVals2[0];
      return Tmp2;
    case 2:
      Tmp1 = (((LongInt)AdrVals2[1]) << 16) + AdrVals2[0];
      return Tmp1;
    default:
      WrError(ErrNum_InternalError);
      return 0;
  }
}

static Boolean IsAbsolute(void)
{
  return (((AdrMode == 0x20) || (AdrMode == 0x60))
       && (AdrCnt == 2)
       && ((AdrVals[0] & 0xe000) == 0xe000));
}

static Boolean Is2Absolute(void)
{
  return (((AdrMode2 == 0x20) || (AdrMode2 == 0x60))
       && (AdrCnt2 == 2)
       && ((AdrVals2[0] & 0xe000) == 0xe000));
}

static Boolean IsShort(void)
{
  if (AdrMode < 0x30)
    return True;
  else if (AdrMode == 0x30)
  {
    LongInt ImmValue = ImmVal();

    return ((ImmValue >= LowLim4) && (ImmValue <= 7));
  }
  else
    return False;
}

static Boolean Is2Short(void)
{
  if (AdrMode2 < 0x30)
    return True;
  else if (AdrMode2 == 0x30)
  {
    LongInt ImmValue = ImmVal2();

    return ((ImmValue >= LowLim4) && (ImmValue <= 7));
  }
  else
    return False;
}

static void ConvertShort(void)
{
  if (AdrMode == 0x30)
  {
    AdrMode += ImmVal() & 15;
    AdrCnt = 0;
  }
}

static void Convert2Short(void)
{
  if (AdrMode2 == 0x30)
  {
    AdrMode2 += ImmVal2() & 15;
    AdrCnt2 = 0;
  }
}

static void SetULowLims(void)
{
  LowLim4 = 0;
  LowLim8 = 0;
}

static Boolean DecodePseudo(void)
{
  return False;
}

static void AddPrefixes(void)
{
  if (CodeLen != 0)
  {
    InsertSinglePrefix(1);
    InsertSinglePrefix(0);
  }
}

static Boolean DecodeCondition(const char *pAsc, Word *pCondition)
{
  int z;

  for (z = 0; z < ConditionCount; z++)
    if (!strcasecmp(pAsc, Conditions[z]))
    {
      *pCondition = z;
      return True;
    }
  return False;
}

static char DecideGA(void)
{
  if (((IsShort()) && (Is2Absolute()))
   || ((Is2Short()) && (IsAbsolute())))
    return 'A';
  else
    return 'G';
}

/*--------------------------------------------------------------------------*/

static void DecodeFixed(Word Code)
{
  if (!ChkArgCnt(0, 0));
  else if (*AttrPart.Str) WrError(ErrNum_UseLessAttr);
  else
  {
    WAsmCode[0] = Code;
    CodeLen = 2;
  }
}

static void DecodeRMW(Word Index)
{
  const RMWOrder *pOrder = RMWOrders + Index;

  if ((OpSize == -1) && (pOrder->Mask & 0x20))
    OpSize = 2;
  if (ChkArgCnt(1, 1))
  {
    tStrComp *pArg = &ArgStr[1];

    if ((!IsIndirect(ArgStr[1].Str)) && (pOrder->Mask & 0x20))
    {
      StrCompReset(&ArgStr[2]);
      sprintf(ArgStr[2].Str, "(%s)", ArgStr[1].Str);
      pArg = &ArgStr[2];
    }
    DecodeAdr(pArg, 0, !(pOrder->Mask & 0x10), !(pOrder->Mask & 0x20));
    if (AdrOK)
    {
      if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
      else if (!(pOrder->Mask & (1 << OpSize))) WrError(ErrNum_InvOpsize);
      else
      {
        WAsmCode[0] = (((Word)OpSize + 1) << 14) + (((Word)pOrder->Code) << 8) + AdrMode;
        memcpy(WAsmCode + 1, AdrVals, AdrCnt);
        CodeLen = 2 + AdrCnt;
      }
    }
  }
}

static void DecodeGASI1(Word Code)
{
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, False, True);
    if (AdrOK)
    {
      CopyAdr();
      DecodeAdr(&ArgStr[2], 0, True, True);
      if (AdrOK)
      {
        if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
        else
        {
          LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

          if (Format == ' ')
          {
            if (((IsReg()) && (Is2Short()))
             || ((Is2Reg()) && (IsShort())))
              Format = 'S';
            else if ((IsImmediate()) && (OpSize > 0) && ((ImmValue > 127) || (ImmValue < -128)))
              Format = 'I';
            else
              Format = DecideGA();
          }
          switch (Format)
          {
            case 'G':
              WAsmCode[0] = 0x700 + (((Word)OpSize + 1) << 14);
              if ((IsImmediate()) && (ImmValue <= 127) && (ImmValue >= -128))
              {
                AdrMode = ImmValue & 0xff;
                AdrCnt = 0;
              }
              else
                WAsmCode[0] += 0x800;
              WAsmCode[0] += AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = 0x8400 + (Code << 8) + AdrMode2;
              memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            case 'A':
              if ((IsShort()) && (Is2Absolute()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3900
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff) + (Code << 13);
                CodeLen = 4 + AdrCnt;
              }
              else if ((Is2Short()) && (IsAbsolute()))
              {
                Convert2Short();
                WAsmCode[0] = 0x3980
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff) + (Code << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            case 'S':
              if ((IsShort()) && (Is2Reg()))
              {
                ConvertShort();
                WAsmCode[0] = 0x0000
                            + (((Word)OpSize + 1) << 14)
                            + (AdrMode & 15)
                            + ((AdrMode & 0xf0) << 5)
                            + ((AdrMode2 & 1) << 12)
                            + ((AdrMode2 & 14) << 4)
                            + ((Code & 1) << 4)
                            + ((Code & 2) << 10);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                CodeLen = 2 + AdrCnt;
              }
              else if ((Is2Short()) && (IsReg()))
              {
                Convert2Short();
                WAsmCode[0] = 0x0100
                            + (((Word)OpSize + 1) << 14)
                            + (AdrMode2 & 15)
                            + ((AdrMode2 & 0xf0) << 5)
                            + ((AdrMode & 1) << 12)
                            + ((AdrMode & 14) << 4)
                            + ((Code & 1) << 4)
                            + ((Code & 2) << 11);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                CodeLen = 2 + AdrCnt2;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            case 'I':
              if ((!IsImmediate()) || (OpSize == 0)) WrError(ErrNum_InvAddrMode);
              else
              {
                WAsmCode[0] = AdrMode2 + (((Word)OpSize-1) << 11) + (Code << 8);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                memcpy(WAsmCode + 1 + (AdrCnt2 >> 1), AdrVals, AdrCnt);
                CodeLen = 2 + AdrCnt + AdrCnt2;
              }
              break;
            default:
               WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeGASI2(Word Code)
{
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, False, True);
    if (AdrOK)
    {
      CopyAdr();
      DecodeAdr(&ArgStr[2], 0, True, True);
      if (AdrOK)
      {
        if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
        else
        {
          LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

          if (Format == ' ')
          {
            if ((IsReg()) && (Is2Reg()))
              Format = 'S';
            else if ((IsImmediate()) && (OpSize > 0) && ((ImmValue > 127) || (ImmValue < -128)))
              Format = 'I';
            else
              Format = DecideGA();
          }
          switch (Format)
          {
            case 'G':
              WAsmCode[0] = 0x700 + (((Word)OpSize + 1) << 14);
              if ((IsImmediate()) && (ImmValue <= 127) && (ImmValue >= -128))
              {
                AdrMode = ImmValue & 0xff;
                AdrCnt = 0;
              }
              else
                WAsmCode[0] += 0x800;
              WAsmCode[0] += AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = 0xc400 + (Code << 8) + AdrMode2;
              memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            case 'A':
              if ((IsShort()) && (Is2Absolute()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3940
                            + (((Word)OpSize+1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff) + (Code << 13);
                CodeLen = 4 + AdrCnt;
              }
              else if ((Is2Short()) && (IsAbsolute()))
              {
                Convert2Short();
                WAsmCode[0] = 0x39c0
                            + (((Word)OpSize+1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff) + (Code << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            case 'S':
              if ((IsReg()) && (Is2Reg()))
              {
                WAsmCode[0] = 0x3800
                            + (((Word)OpSize+1) << 14)
                            + (AdrMode & 15)
                            + (AdrMode2 << 4)
                            + (Code << 9);
                CodeLen = 2;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            case 'I':
              if ((!IsImmediate()) || (OpSize == 0)) WrError(ErrNum_InvAddrMode);
              else
              {
                WAsmCode[0] = 0x400 + AdrMode2 + (((Word)OpSize-1) << 11) + (Code << 8);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                memcpy(WAsmCode + 1 + (AdrCnt2 >> 1), AdrVals, AdrCnt);
                CodeLen = 2 + AdrCnt + AdrCnt2;
              }
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeTrinom(Word Code)
{
  int Cnt;
  Byte Reg;

  if (Code == 2) /* MAC */
    LowLim8 = 0;
  if (!ChkArgCnt(3, 3));
  else if (!DecodeRegAdr(ArgStr[1].Str, &Reg)) WrError(ErrNum_InvAddrMode);
  else
  {
    if (Code >= 2)
      OpSize--;
    if (OpSize < 0) WrError(ErrNum_InvOpsize);
    else
    {
      DecodeAdr(&ArgStr[3], 0, True, True);
      if (AdrOK)
      {
        LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

        WAsmCode[0] = 0x700;
        if ((IsImmediate()) && (ImmValue < 127) && (ImmValue > LowLim8))
        {
          AdrMode = ImmValue & 0xff;
          AdrCnt = 0;
        }
        else
          WAsmCode[0] += 0x800;
        WAsmCode[0] += (((Word)OpSize + 1) << 14) + AdrMode;
        memcpy(WAsmCode + 1, AdrVals, AdrCnt);
        Cnt = AdrCnt;
        DecodeAdr(&ArgStr[2], 1, False, True);
        if (AdrOK)
        {
          WAsmCode[1 + (Cnt >> 1)] = AdrMode + (Code << 8) + (((Word)Reg) << 11);
          memcpy(WAsmCode + 2 + (Cnt >> 1), AdrVals, AdrCnt);
          CodeLen = 4 + Cnt + AdrCnt;
        }
      }
    }
  }
}

static void DecodeRLM_RRM(Word Code)
{
  int Cnt;
  Byte Reg;

  if (!ChkArgCnt(3, 3));
  else if (!DecodeReg(ArgStr[2].Str, &Reg)) WrError(ErrNum_InvAddrMode);
  else if ((Reg >> 6) != 1) WrError(ErrNum_InvOpsize);
  else
  {
    Reg &= 0x3f;
    DecodeAdr(&ArgStr[3], 0, True, True);
    if (AdrOK)
    {
      LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

      WAsmCode[0] = 0x700;
      if ((IsImmediate()) && (ImmValue < 127) && (ImmValue > -128))
      {
        AdrMode = ImmValue & 0xff; AdrCnt = 0;
      }
      else
        WAsmCode[0] += 0x800;
      WAsmCode[0] += AdrMode;
      memcpy(WAsmCode + 1, AdrVals, AdrCnt);
      Cnt = AdrCnt;
      DecodeAdr(&ArgStr[1], 1, False, True);
      if (AdrOK)
      {
        if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
        else
        {
          WAsmCode[0] += ((Word)OpSize + 1) << 14;
          WAsmCode[1 + (Cnt >> 1)] = Code + (((Word)Reg) << 11)+AdrMode;
          memcpy(WAsmCode + 2 + (Cnt >> 1), AdrVals, AdrCnt);
          CodeLen = 4 + AdrCnt + Cnt;
        }
      }
    }
  }
}

static void DecodeBit(Word Code)
{
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, False, True);
    if (AdrOK)
    {
      if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
      else
      {
        CopyAdr();
        OpSize = -1;
        MinOneIs0 = True;
        DecodeAdr(&ArgStr[2], 0, True, True);
        if (AdrOK)
        {
          LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

          OpSize = OpSize2;
          if (Format==' ')
          {
            if ((Is2Reg()) && (IsImmediate())
             && (ImmValue > 0)
             && (ImmValue < (1 << (OpSize + 3))))
              Format = 'S';
            else 
              Format = DecideGA();
          }
          switch (Format)
          {
            case 'G':
              WAsmCode[0] = 0x700 + (((Word)OpSize + 1) << 14);
              if ((IsImmediate()) && (ImmValue >= LowLim8) && (ImmValue < 127))
              {
                AdrMode = ImmValue & 0xff;
                AdrCnt = 0;
              }
              else
                WAsmCode[0] += 0x800;
              WAsmCode[0] += AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = 0xd400 + (Code << 8) + AdrMode2;
              memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            case 'A':
              if ((IsAbsolute()) && (Is2Short()))
              {
                Convert2Short();
                WAsmCode[0] = 0x39d0
                            + (((Word)OpSize+1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff) + (Code << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else if ((Is2Absolute()) && (IsShort()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3950
                            + (((Word)OpSize+1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff) + (Code << 13);
                CodeLen = 4 + AdrCnt;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            case 'S':
              if ((Is2Reg())
               && (IsImmediate())
               && (ImmVal() >= 0)
               && (ImmVal() < (1 << (3 + OpSize))))
              {
                if (OpSize == 2)
                {
                  if (ImmVal() >= 16)
                  {
                    AdrVals[0] -= 16;
                    AdrMode2++;
                  }
                  OpSize = 1;
                }
                if (OpSize == 1)
                {
                  if (ImmVal() < 8)
                    OpSize=0;
                  else
                    AdrVals[0] -= 8;
                }
                WAsmCode[0] = 0x1700
                            + (((Word)OpSize + 1) << 14)
                            + ((Code & 1) << 7)
                            + ((Code & 2) << 10)
                            + (ImmVal() << 4)
                            + AdrMode2;
                CodeLen = 2;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeShift(Word Code)
{
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, False, True);
    if (AdrOK)
    {
      if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
      else
      {
        CopyAdr();
        OpSize = -1;
        MinOneIs0 = True;
        DecodeAdr(&ArgStr[2], 0, True, True);
        if (AdrOK)
        {
          LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

          OpSize = OpSize2;
          if (Format==' ')
          {
            if ((IsImmediate()) && (ImmValue == 1))
              Format = 'S';
            else
              Format = DecideGA();
          }
          switch (Format)
          {
            case 'G':
              WAsmCode[0] = 0x700 + (((Word)OpSize + 1) << 14);
              if ((IsImmediate()) && (ImmValue >= LowLim8) && (ImmVal() < 127))
              {
                AdrMode = ImmValue & 0xff;
                AdrCnt = 0;
              }
              else
                WAsmCode[0] += 0x800;
              WAsmCode[0] += AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = 0xb400
                                          + ((Code & 3) << 8)
                                          + ((Code & 4) << 9)
                                          + AdrMode2;
              memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            case 'A':
              if ((IsAbsolute()) && (Is2Short()))
              {
                Convert2Short();
                WAsmCode[0] = 0x39b0
                            + (((Word)OpSize+1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff) + (Code << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else if ((Is2Absolute()) && (IsShort()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3930
                            + (((Word)OpSize+1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff)+ (Code << 13);
                CodeLen = 4 + AdrCnt;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            case 'S':
              if ((IsImmediate()) && (ImmValue == 1))
              {
                WAsmCode[0] = 0x2400
                            + (((Word)OpSize+1) << 14)
                            + AdrMode2
                            + ((Code & 3) << 8)
                            + ((Code & 4) << 9);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                CodeLen =2 + AdrCnt2;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeBField(Word Code)
{
  Byte Reg, Num1, Num2;
  Boolean OK;

  if (ChkArgCnt(4, 4))
  {
    tStrComp *pArg1 = (Code == 2) ? &ArgStr[2] : &ArgStr[1],
             *pArg2 = (Code == 2) ? &ArgStr[1] : &ArgStr[2];

    if (!DecodeReg(pArg1->Str, &Reg)) WrError(ErrNum_InvAddrMode);
    else if ((Reg >> 6) != 1) WrError(ErrNum_InvOpsize);
    else
    {
      Reg &= 0x3f;
      Num2 = EvalStrIntExpression(&ArgStr[4], Int5, &OK);
      if (OK)
      {
        if (FirstPassUnknown)
          Num2 &= 15;
        Num2--;
        if (Num2 > 15) WrError(ErrNum_OverRange);
        else if ((OpSize == -1) && (!DecodeRegAdr(pArg2->Str, &Num1))) WrError(ErrNum_UndefOpSizes);
        else
        {
          switch (OpSize)
          {
            case 0: Num1 = EvalStrIntExpression(&ArgStr[3], UInt3, &OK) & 7; break;
            case 1: Num1 = EvalStrIntExpression(&ArgStr[3], Int4, &OK) & 15; break;
            case 2: Num1 = EvalStrIntExpression(&ArgStr[3], Int5, &OK) & 31; break;
            default: assert(0);
          }
          if (OK)
          {
            if ((OpSize == 2) && (Num1 > 15))
              AdrInc = 2;
            DecodeAdr(pArg2, 1, False, True);
            if (AdrOK)
            {
              if ((OpSize == 2) && (Num1 > 15))
              {
                Num1 -= 16;
                OpSize--;
                if (!(AdrMode & 0xf0))
                  AdrMode++;
              }
              WAsmCode[0] = 0x7000 + (((Word)OpSize + 1) << 8) + AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = (((Word)Reg) << 11)
                                          + Num2
                                          + (((Word)Num1) << 5)
                                          + ((Code & 1) << 10)
                                          + ((Code & 2) << 14);
              CodeLen = 4 + AdrCnt;
            }
          }
        }
      }
    }
  }
}

static void DecodeGAEq(Word Code)
{
  if (Hi(Code))
    SetULowLims();
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, False, True);
    if (AdrOK)
    {
      CopyAdr();
      DecodeAdr(&ArgStr[2], 0, True, True);
      if (AdrOK)
      {
        if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
        else
        {
          if (OpSize == 0)
            LowLim8 = -128;
          if (Format == ' ')
            Format = DecideGA();
          switch (Format)
          {
            case 'G':
            {
              LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

              WAsmCode[0] = 0x700 + (((Word)OpSize + 1) << 14);
              if ((IsImmediate()) && (ImmValue < 127) && (ImmValue > LowLim8))
              {
                AdrMode = ImmValue & 0xff;
                AdrCnt = 0;
              }
              else
                WAsmCode[0] += 0x800;
              WAsmCode[0] += AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = 0x8400
                                          + AdrMode2
                                          + ((Code & 0xf0) << 8)
                                          + ((Code & 4) << 9)
                                          + ((Code & 3) << 8);
              memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            }
            case 'A':
              if ((IsAbsolute()) && (Is2Short()))
              {
                Convert2Short();
                WAsmCode[0] = 0x3980
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15)
                            + (Code & 0xf0);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff)
                                             + ((Code & 15) << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else if ((Is2Absolute()) && (IsShort()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3900
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15)
                            + (Code & 0xf0);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff)
                                            + ((Code & 15) << 13);
                CodeLen = 4 + AdrCnt;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeGAHalf(Word Code)
{
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, False, True);
    if (AdrOK)
    {
      if (OpSize == 0) WrError(ErrNum_InvOpsize);
      else
      {
        if (OpSize != -1)
          OpSize--;
        CopyAdr();
        DecodeAdr(&ArgStr[2], 0, True, True);
        if (AdrOK)
        {
          if (OpSize == 2) WrError(ErrNum_InvOpsize);
          else if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
          else
          {
            if (Format == ' ')
              Format = DecideGA();
            switch (Format)
            {
              case 'G':
              {
                LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

                WAsmCode[0] = 0x700 + (((Word)OpSize + 1) << 14);
                if ((IsImmediate()) && (ImmValue < 127) && (ImmValue > LowLim8))
                {
                  AdrMode = ImmValue & 0xff;
                  AdrCnt = 0;
                }
                else
                  WAsmCode[0] += 0x800;
                WAsmCode[0] += AdrMode;
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = 0x8400
                                            + AdrMode2
                                            + ((Code & 0xf0) << 8)
                                            + ((Code & 4) << 9)
                                            + ((Code & 3) << 8);
                memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
                CodeLen = 4 + AdrCnt + AdrCnt2;
                break;
              }
              case 'A':
                if ((IsAbsolute()) && (Is2Short()))
                {
                  Convert2Short();
                  WAsmCode[0] = 0x3980
                              + (((Word)OpSize + 1) << 14)
                              + ((AdrMode2 & 0xf0) << 5)
                              + (AdrMode2 & 15)
                              + (Code & 0xf0);
                  memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                  WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff)
                                               + ((Code & 15) << 13);
                  CodeLen = 4 + AdrCnt2;
                }
                else if ((Is2Absolute()) && (IsShort()))
                {
                  ConvertShort();
                  WAsmCode[0] = 0x3900
                              + (((Word)OpSize + 1) << 14)
                              + ((AdrMode & 0xf0) << 5)
                              + (AdrMode & 15)
                              + (Code & 0xf0);
                  memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                  WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff)
                                              + ((Code & 15) << 13);
                  CodeLen = 4 + AdrCnt;
                }
                else WrError(ErrNum_InvAddrMode);
                break;
              default:
                WrError(ErrNum_InvFormat);
            }
          }
        }
      }
    }
  }
}

static void DecodeGAFirst(Word Code)
{
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, !(Memo("STCF") || Memo("TSET")), True);
    if (AdrOK)
    {
      if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
      else
      {
        CopyAdr();
        OpSize = -1;
        MinOneIs0 = True;
        DecodeAdr(&ArgStr[2], 0, True, True);
        OpSize = OpSize2;
        if (AdrOK)
        {
          if (Format == ' ')
            Format = DecideGA();
          switch (Format)
          {
            case 'G':
            {
              LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

              WAsmCode[0] = 0x700
                          + (((Word)OpSize + 1) << 14);
              if ((IsImmediate()) && (ImmValue < 127) && (ImmValue > LowLim8))
              {
                AdrMode = ImmValue & 0xff;
                AdrCnt = 0;
              }
              else WAsmCode[0] += 0x800;
              WAsmCode[0] += AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = 0x8400
                                          + AdrMode2
                                          + ((Code & 0xf0) << 8)
                                          + ((Code & 4) << 9)
                                          + ((Code & 3) << 8);
              memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            }
            case 'A':
              if ((IsAbsolute()) && (Is2Short()))
              {
                Convert2Short();
                WAsmCode[0] = 0x3980
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15)
                            + (Code & 0xf0);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff)
                                             + ((Code & 15) << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else if ((Is2Absolute()) && (IsShort()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3900
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15)
                            + (Code & 0xf0);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff)
                                            + ((Code & 15) << 13);
                CodeLen = 4 + AdrCnt;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeGASecond(Word Code)
{
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[2], 0, True, True);
    if (AdrOK)
    {
      if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
      else
      {
        CopyAdr();
        OpSize = -1;
        DecodeAdr(&ArgStr[1], 1, False, True);
        OpSize = OpSize2;
        if (AdrOK)
        {
          if (Format == ' ')
            Format = DecideGA();
          switch (Format)
          {
            case 'G':
              WAsmCode[0] = 0x700 + (((Word)OpSize + 1) << 14);
              if ((Is2Immediate()) && (ImmVal2() < 127) && (ImmVal2() > LowLim8))
              {
                AdrMode2 = ImmVal2() & 0xff;
                AdrCnt = 0;
              }
              else
                WAsmCode[0] += 0x800;
              WAsmCode[0] += AdrMode2;
              memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
              WAsmCode[1 + (AdrCnt2 >> 1)] = 0x8400
                                           + AdrMode
                                           + ((Code & 0xf0) << 8)
                                           + ((Code & 4) << 9)
                                           + ((Code & 3) << 8);
              memcpy(WAsmCode + 2 + (AdrCnt2 >> 1), AdrVals, AdrCnt);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            case 'A':
              if ((IsAbsolute()) && (Is2Short()))
              {
                Convert2Short();
                WAsmCode[0] = 0x3900
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15)
                            + (Code & 0xf0);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = (AdrVals[0] & 0x1fff)
                                             + ((Code & 15) << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else if ((Is2Absolute()) && (IsShort()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3980 + (((Word)OpSize + 1) << 14)
                                     + ((AdrMode & 0xf0) << 5)
                                     + (AdrMode & 15)
                                     + (Code & 0xf0);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = (AdrVals2[0] & 0x1fff)
                                            + ((Code & 15) << 13);
                CodeLen = 4 + AdrCnt;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeCHK_CHKS(Word IsSigned)
{
  if (!IsSigned)
    SetULowLims();
  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[2], 1, False, True);
    if (AdrOK)
    {
      if ((OpSize != 1) && (OpSize != 2)) WrError(ErrNum_InvOpsize);
      else if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
      else
      {
        CopyAdr();
        DecodeAdr(&ArgStr[1], 0, False, False);
        if (AdrOK)
        {
          if (OpSize == 0)
            LowLim8 = -128;
          if (Format == ' ')
            Format = DecideGA();
          switch (Format)
          {
            case 'G':
              WAsmCode[0] = 0xf00 + (((Word)OpSize + 1) << 14) + AdrMode2;
              memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
              WAsmCode[1 + (AdrCnt2 >> 1)] = 0xa600 + AdrMode
                                           + (IsSigned << 8);
              memcpy(WAsmCode + 2 + (AdrCnt2 >> 1), AdrVals, AdrCnt);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            case 'A':
              if ((IsAbsolute()) && (Is2Short()))
              {
                Convert2Short();
                WAsmCode[0] = 0x3920
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = 0x4000
                                             + (AdrVals[0] & 0x1fff)
                                             + (IsSigned << 13);
                CodeLen = 4 + AdrCnt2;
              }
              else if ((Is2Absolute()) && (IsShort()))
              {
                ConvertShort();
                WAsmCode[0] = 0x39a0
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = 0x4000
                                            + (AdrVals2[0] & 0x1fff)
                                            + (IsSigned << 13);
                CodeLen = 4 + AdrCnt;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeString(Word Code)
{
  Byte Reg;
  int Cnt;

  if (!ChkArgCnt(3, 3));
  else if (!DecodeReg(ArgStr[3].Str, &Reg)) WrError(ErrNum_InvAddrMode);
  else if ((Reg >> 6) != 1) WrError(ErrNum_InvOpsize);
  else
  {
    Reg &= 0x3f;
    DecodeAdr(&ArgStr[2], 0, True, True);
    if (AdrOK)
    {
      LongInt ImmValue = IsImmediate() ? ImmVal() : 0;

      WAsmCode[0] = 0x700;
      if ((IsImmediate()) && (ImmValue < 127) && (ImmValue > LowLim8))
      {
        AdrMode = ImmValue & 0xff;
        AdrCnt = 0;
      }
      else
        WAsmCode[0] += 0x800;
      WAsmCode[0] += AdrMode;
      memcpy(WAsmCode + 1, AdrVals, AdrCnt);
      Cnt = AdrCnt;
      DecodeAdr(&ArgStr[1], 1, True, True);
      if (AdrOK)
      {
        if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
        else
        {
          WAsmCode[0] += ((Word)OpSize + 1) << 14;
          WAsmCode[1 + (Cnt >> 1)] = 0x8000 + AdrMode + (Code << 8) + (((Word)Reg) << 11);
          memcpy(WAsmCode + 2 + (Cnt >> 1), AdrVals, AdrCnt);
          CodeLen = 4 + AdrCnt + Cnt;
        }
      }
    }
  }
}

static void DecodeEX(Word Code)
{
  UNUSED(Code);

  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 1, False, True);
    if (AdrOK)
    {
      CopyAdr();
      DecodeAdr(&ArgStr[2], 0, False, True);
      if (AdrOK)
      {
        if (OpSize == -1) WrError(ErrNum_UndefOpSizes);
        else
        {
          if (Format == ' ')
          {
            if ((IsReg()) && (Is2Reg()))
              Format = 'S';
            else
              Format = DecideGA();
          }
          switch (Format)
          {
            case 'G':
              WAsmCode[0] = 0x0f00 + (((Word)OpSize + 1) << 14) + AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = 0x8f00 + AdrMode2;
              memcpy(WAsmCode + 2 + (AdrCnt >> 1), AdrVals2, AdrCnt2);
              CodeLen = 4 + AdrCnt + AdrCnt2;
              break;
            case 'A':
              if ((IsAbsolute()) && (Is2Short()))
              {
                Convert2Short();
                WAsmCode[0] = 0x3980
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode2 & 0xf0) << 5)
                            + (AdrMode2 & 15);
                memcpy(WAsmCode + 1, AdrVals2, AdrCnt2);
                WAsmCode[1 + (AdrCnt2 >> 1)] = AdrVals[0];
                CodeLen = 4 + AdrCnt2;
              }
              else if ((Is2Absolute()) && (IsShort()))
              {
                ConvertShort();
                WAsmCode[0] = 0x3900
                            + (((Word)OpSize + 1) << 14)
                            + ((AdrMode & 0xf0) << 5)
                            + (AdrMode & 15);
                memcpy(WAsmCode + 1, AdrVals, AdrCnt);
                WAsmCode[1 + (AdrCnt >> 1)] = AdrVals2[0];
                CodeLen = 4 + AdrCnt;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            case 'S':
              if ((IsReg()) && (Is2Reg()))
              {
                WAsmCode[0] = 0x3e00
                            + (((Word)OpSize + 1) << 14)
                            + (AdrMode2 << 4)
                            + AdrMode;
                CodeLen = 2;
              }
              else WrError(ErrNum_InvAddrMode);
              break;
            default:
              WrError(ErrNum_InvFormat);
          }
        }
      }
    }
  }
}

static void DecodeCALR_JR(Word Code)
{
  if (!ChkArgCnt(1, 1));
  else if (*AttrPart.Str) WrError(ErrNum_UseLessAttr);
  else
  {
    LongInt AdrInt;
    Boolean OK, ForcePrefix = False;

    AdrInt = EvalStrIntExpressionOffs(&ArgStr[1], CheckForcePrefix(ArgStr[1].Str, &ForcePrefix), Int32, &OK) - EProgCounter();
    if ((OK) && (AddRelPrefix(0, 13, &AdrInt, ForcePrefix)))
    {
      if (Odd(AdrInt)) WrError(ErrNum_DistIsOdd);
      else
      {
        WAsmCode[0] = Code + (AdrInt & 0x1ffe);
        CodeLen = 2;
      }
    }
  }
}

static void DecodeJRC(Word Code)
{
  UNUSED(Code);

  if (!ChkArgCnt(2, 2));
  else if (*AttrPart.Str) WrError(ErrNum_UseLessAttr);
  else
  {
    Word Condition;

    if (!DecodeCondition(ArgStr[1].Str, &Condition)) WrStrErrorPos(ErrNum_UndefCond, &ArgStr[1]);
    else
    {
      LongInt AdrInt;
      Boolean OK, ForcePrefix = False;

      Condition %= 16;
      AdrInt = EvalStrIntExpressionOffs(&ArgStr[2], CheckForcePrefix(ArgStr[2].Str, &ForcePrefix), Int32, &OK) - EProgCounter();
      if ((OK) && (AddRelPrefix(0, 9, &AdrInt, ForcePrefix)))
      {
        if (Odd(AdrInt)) WrError(ErrNum_DistIsOdd);
        else
        {
          WAsmCode[0] = 0x1000 + ((Condition & 14) << 8) + (AdrInt & 0x1fe) + (Condition & 1);
          CodeLen = 2;
        }
      }
    }
  }
}

static void DecodeJRBC_JRBS(Word Code)
{
  if (!ChkArgCnt(3, 3));
  else if (*AttrPart.Str) WrError(ErrNum_UseLessAttr);
  else
  {
    int z;
    Boolean OK;

    z = EvalStrIntExpression(&ArgStr[1], UInt3, &OK);
    if (OK)
    {
      Boolean AdrLongPrefix = False;
      LongInt AdrLong;

      FirstPassUnknown = False;
      AdrLong = EvalStrIntExpressionOffs(&ArgStr[2], CheckForcePrefix(ArgStr[2].Str, &AdrLongPrefix), Int24, &OK);
      if (OK)
      {
        LongInt AdrInt;
        Boolean AdrIntPrefix = False;

        AddAbsPrefix(1, 13, AdrLong, AdrLongPrefix);
        AdrInt = EvalStrIntExpressionOffs(&ArgStr[3], CheckForcePrefix(ArgStr[3].Str, &AdrIntPrefix), Int32, &OK) - EProgCounter();
        if ((OK) && (AddRelPrefix(0, 9, &AdrInt, AdrIntPrefix)))
        {
          if (Odd(AdrInt)) WrError(ErrNum_DistIsOdd);
          else
          {
            CodeLen = 4;
            WAsmCode[1] = (z << 13) + (AdrLong & 0x1fff);
            WAsmCode[0] = Code + (AdrInt & 0x1fe);
          }
        }
      }
    }
  }
}

static void DecodeDJNZ(Word Code)
{
  UNUSED(Code);

  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[1], 0, False, True);
    if (AdrOK)
    {
      if ((OpSize != 1) && (OpSize != 2)) WrError(ErrNum_InvOpsize);
      else
      {
        LongInt AdrInt;
        Boolean OK, ForcePrefix = False;

        AdrInt = EvalStrIntExpressionOffs(&ArgStr[2], CheckForcePrefix(ArgStr[2].Str, &ForcePrefix), Int32, &OK) - (EProgCounter() + 4 + AdrCnt +2 * Ord(PrefUsed[0]));
        if ((OK) && (AddRelPrefix(1, 13, &AdrInt, ForcePrefix)))
        {
          if (Odd(AdrInt)) WrError(ErrNum_DistIsOdd);
          else
          {
            WAsmCode[0] = 0x3700 + (((Word)OpSize + 1) << 14) + AdrMode;
            memcpy(WAsmCode + 1, AdrVals, AdrCnt);
            WAsmCode[1 + (AdrCnt >> 1)] = 0xe000 + (AdrInt & 0x1ffe);
            CodeLen = 4 + AdrCnt;
          }
        }
      }
    }
  }
}

static void DecodeDJNZC(Word Code)
{
  UNUSED(Code);

  if (ChkArgCnt(3, 3))
  {
    Word Condition;

    if (!DecodeCondition(ArgStr[2].Str, &Condition)) WrStrErrorPos(ErrNum_UndefCond, &ArgStr[2]);
    else
    {
      Condition %= 16;
      DecodeAdr(&ArgStr[1], 0, False, True);
      if (AdrOK)
      {
        if ((OpSize != 1) && (OpSize != 2)) WrError(ErrNum_InvOpsize);
        else
        {
          Boolean OK, ForcePrefix = False;
          LongInt AdrInt;

          AdrInt = EvalStrIntExpressionOffs(&ArgStr[3], CheckForcePrefix(ArgStr[3].Str, &ForcePrefix), Int32, &OK) - EProgCounter();
          if ((OK) && (AddRelPrefix(1, 13, &AdrInt, ForcePrefix)))
          {
            if (Odd(AdrInt)) WrError(ErrNum_DistIsOdd);
            else
            {
              WAsmCode[0] = 0x3700 + (((Word)OpSize+1) << 14) + AdrMode;
              memcpy(WAsmCode + 1, AdrVals, AdrCnt);
              WAsmCode[1 + (AdrCnt >> 1)] = ((Condition & 14) << 12) + (AdrInt & 0x1ffe) + (Condition & 1);
              CodeLen =4 + AdrCnt;
            }
          }
        }
      }
    }
  }
}

static void DecodeLINK_RETD(Word Code)
{
  if (!ChkArgCnt(1, 1));
  else if (*AttrPart.Str) WrError(ErrNum_UseLessAttr);
  else
  {
    LongInt AdrInt;
    Boolean OK, ForcePrefix = False;

    FirstPassUnknown = False;
    AdrInt = EvalStrIntExpressionOffs(&ArgStr[1], CheckForcePrefix(ArgStr[1].Str, &ForcePrefix), Int32, &OK);
    if (FirstPassUnknown)
      AdrInt &= 0x1fe;
    if (ChkRange(AdrInt, -0x80000, 0x7ffff))
    {
      if (Odd(AdrInt)) WrError(ErrNum_NotAligned);
      else
      {
        WAsmCode[0] = Code + (AdrInt & 0x1fe);
        AddSignedPrefix(0, 9, AdrInt, ForcePrefix);
        CodeLen = 2;
      }
    }
  }
}

static void DecodeSWI(Word Code)
{
  UNUSED(Code);

  if (!ChkArgCnt(1, 1));
  else if (*AttrPart.Str) WrError(ErrNum_UseLessAttr);
  else
  {
    Boolean OK;

    WAsmCode[0] = EvalStrIntExpression(&ArgStr[1], Int4, &OK) + 0x7f90;
    if (OK)
      CodeLen = 2;
  }
}

static void DecodeLDA(Word Code)
{
  UNUSED(Code);

  if (ChkArgCnt(2, 2))
  {
    DecodeAdr(&ArgStr[2], 0, False, False);
    if (AdrOK)
    {
      int z;

      WAsmCode[0] = 0x3000 + AdrMode;
      z = AdrCnt;
      memcpy(WAsmCode + 1, AdrVals, z);
      DecodeAdr(&ArgStr[1], 1, False, True);
      if (AdrOK)
      {
        if ((OpSize != 1) && (OpSize != 2)) WrError(ErrNum_InvOpsize);
        else
        {
          WAsmCode[0] += ((Word)OpSize) << 14;
          WAsmCode[1 + (z >> 1)] = 0x9700 + AdrMode;
          memcpy(WAsmCode + 2 + (z >> 1), AdrVals, AdrCnt);
          CodeLen = 4 + z + AdrCnt;
        }
      }
    }
  }
}

/*--------------------------------------------------------------------------*/

static void AddFixed(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeFixed);
}

static void AddRMW(char *NName, Byte NCode, Byte NMask)
{
  if (InstrZ >= RMWOrderCount) exit(255);
  RMWOrders[InstrZ].Mask = NMask;
  RMWOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ++, DecodeRMW);
}

static void AddGAEq(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeGAEq);
}

static void AddGAHalf(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeGAHalf);
}

static void AddGAFirst(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeGAFirst);
}

static void AddGASecond(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeGASecond);
}                               

static void InitFields(void)
{
  InstTable = CreateInstTable(301);
  AddInstTable(InstTable, "RLM", 0x0400, DecodeRLM_RRM);
  AddInstTable(InstTable, "RRM", 0x0500, DecodeRLM_RRM);
  AddInstTable(InstTable, "CHK", 0, DecodeCHK_CHKS);
  AddInstTable(InstTable, "CHKS", 1, DecodeCHK_CHKS);
  AddInstTable(InstTable, "EX", 0, DecodeEX);
  AddInstTable(InstTable, "CALR", 0x2001, DecodeCALR_JR);
  AddInstTable(InstTable, "JR", 0x2000, DecodeCALR_JR);
  AddInstTable(InstTable, "JRC", 0, DecodeJRC);
  AddInstTable(InstTable, "JRBC", 0x1e00, DecodeJRBC_JRBS);
  AddInstTable(InstTable, "JRBS", 0x1e01, DecodeJRBC_JRBS);
  AddInstTable(InstTable, "DJNZ", 0, DecodeDJNZ);
  AddInstTable(InstTable, "DJNZC", 0, DecodeDJNZC);
  AddInstTable(InstTable, "LINK", 0xc001, DecodeLINK_RETD);
  AddInstTable(InstTable, "RETD", 0xc801, DecodeLINK_RETD);
  AddInstTable(InstTable, "SWI", 0, DecodeSWI);
  AddInstTable(InstTable, "LDA", 0, DecodeLDA);

  AddFixed("CCF" , 0x7f82);
  AddFixed("CSF" , 0x7f8a);
  AddFixed("CVF" , 0x7f86);
  AddFixed("CZF" , 0x7f8e);
  AddFixed("DI"  , 0x7fa1);
  AddFixed("EI"  , 0x7fa3);
  AddFixed("HALT", 0x7fa5);
  AddFixed("NOP" , 0x7fa0);
  AddFixed("RCF" , 0x7f80);
  AddFixed("RET" , 0x7fa4);
  AddFixed("RETI", 0x7fa9);
  AddFixed("RETS", 0x7fab);
  AddFixed("RSF" , 0x7f88);
  AddFixed("RVF" , 0x7f84);
  AddFixed("RZF" , 0x7f8c);
  AddFixed("SCF" , 0x7f81);
  AddFixed("SSF" , 0x7f89);
  AddFixed("SVF" , 0x7f85);
  AddFixed("SZF" , 0x7f8b);
  AddFixed("UNLK", 0x7fa2);

  RMWOrders = (RMWOrder *) malloc(sizeof(RMWOrder) * RMWOrderCount); InstrZ = 0;
  AddRMW("CALL" , 0x35, 0x36);
  AddRMW("CLR"  , 0x2b, 0x17);
  AddRMW("CPL"  , 0x28, 0x17);
  AddRMW("EXTS" , 0x33, 0x16);
  AddRMW("EXTZ" , 0x32, 0x16);
  AddRMW("JP"   , 0x34, 0x36);
  AddRMW("MIRR" , 0x23, 0x17);
  AddRMW("NEG"  , 0x29, 0x17);
  AddRMW("POP"  , 0x20, 0x17);
  AddRMW("PUSH" , 0x21, 0x07);
  AddRMW("PUSHA", 0x31, 0x36);
  AddRMW("RVBY" , 0x22, 0x17);
  AddRMW("TJP"  , 0x36, 0x16);
  AddRMW("TST"  , 0x2a, 0x17);

  InstrZ = 0;
  AddInstTable(InstTable, "ADD", InstrZ++, DecodeGASI1);
  AddInstTable(InstTable, "SUB", InstrZ++, DecodeGASI1);
  AddInstTable(InstTable, "CP" , InstrZ++, DecodeGASI1);
  AddInstTable(InstTable, "LD" , InstrZ++, DecodeGASI1);

  InstrZ = 0;
  AddInstTable(InstTable, "AND", InstrZ++, DecodeGASI2);
  AddInstTable(InstTable, "OR" , InstrZ++, DecodeGASI2);
  AddInstTable(InstTable, "XOR", InstrZ++, DecodeGASI2);

  InstrZ = 0;
  AddInstTable(InstTable, "ADD3", InstrZ++, DecodeTrinom);
  AddInstTable(InstTable, "SUB3", InstrZ++, DecodeTrinom);
  AddInstTable(InstTable, "MAC" , InstrZ++, DecodeTrinom);
  AddInstTable(InstTable, "MACS", InstrZ++, DecodeTrinom);

  InstrZ = 0;
  AddInstTable(InstTable, "BRES", InstrZ++, DecodeBit);
  AddInstTable(InstTable, "BSET", InstrZ++, DecodeBit);
  AddInstTable(InstTable, "BCHG", InstrZ++, DecodeBit);
  AddInstTable(InstTable, "BTST", InstrZ++, DecodeBit);

  InstrZ = 0;
  AddInstTable(InstTable, "SLL", InstrZ++, DecodeShift);
  AddInstTable(InstTable, "SRL", InstrZ++, DecodeShift);
  AddInstTable(InstTable, "SLA", InstrZ++, DecodeShift);
  AddInstTable(InstTable, "SRA", InstrZ++, DecodeShift);
  AddInstTable(InstTable, "RL" , InstrZ++, DecodeShift);
  AddInstTable(InstTable, "RR" , InstrZ++, DecodeShift);
  AddInstTable(InstTable, "RLC", InstrZ++, DecodeShift);
  AddInstTable(InstTable, "RRC", InstrZ++, DecodeShift);

  InstrZ = 0;
  AddInstTable(InstTable, "BFEX" , InstrZ++, DecodeBField);
  AddInstTable(InstTable, "BFEXS", InstrZ++, DecodeBField);
  AddInstTable(InstTable, "BFIN" , InstrZ++, DecodeBField);

  AddGAEq("ABCD" , 0x0110);
  AddGAEq("ADC"  , 0x0004);
  AddGAEq("CBCD" , 0x0112);
  AddGAEq("CPC"  , 0x0006);
  AddGAEq("MAX"  , 0x0116);
  AddGAEq("MAXS" , 0x0017);
  AddGAEq("MIN"  , 0x0114);
  AddGAEq("MINS" , 0x0015);
  AddGAEq("SBC"  , 0x0105);
  AddGAEq("SBCD" , 0x0111);

  AddGAHalf("DIV"  , 0x26);
  AddGAHalf("DIVS" , 0x27);
  AddGAHalf("MUL"  , 0x24);
  AddGAHalf("MULS" , 0x25);

  AddGAFirst("ANDCF", 0x44);
  AddGAFirst("LDCF" , 0x47);
  AddGAFirst("ORCF" , 0x45);
  AddGAFirst("STCF" , 0x43);
  AddGAFirst("TSET" , 0x70);
  AddGAFirst("XORCF", 0x46);

  AddGASecond("BS0B" , 0x54);
  AddGASecond("BS0F" , 0x55);
  AddGASecond("BS1B" , 0x56);
  AddGASecond("BS1F" , 0x57);

  AddInstTable(InstTable, "CPSZ", 0, DecodeString);   
  AddInstTable(InstTable, "CPSN", 1, DecodeString); 
  AddInstTable(InstTable, "LDS" , 3, DecodeString);

  Conditions = (char **) malloc(sizeof(char *)*ConditionCount); InstrZ = 0;
  Conditions[InstrZ++] = "C";   Conditions[InstrZ++] = "NC";
  Conditions[InstrZ++] = "Z";   Conditions[InstrZ++] = "NZ";
  Conditions[InstrZ++] = "OV";  Conditions[InstrZ++] = "NOV";
  Conditions[InstrZ++] = "MI";  Conditions[InstrZ++] = "PL";
  Conditions[InstrZ++] = "LE";  Conditions[InstrZ++] = "GT";
  Conditions[InstrZ++] = "LT";  Conditions[InstrZ++] = "GE";
  Conditions[InstrZ++] = "ULE"; Conditions[InstrZ++] = "UGT";
  Conditions[InstrZ++] = "N";   Conditions[InstrZ++] = "A";
  Conditions[InstrZ++] = "ULT"; Conditions[InstrZ++] = "UGE";
  Conditions[InstrZ++] = "EQ";  Conditions[InstrZ++] = "NE";
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);

  free(RMWOrders);
  free(Conditions);
}

static void MakeCode_97C241(void)
{
  char *p;

  CodeLen = 0;
  DontPrint = False;
  PrefUsed[0] = False;
  PrefUsed[1] = False;
  AdrInc = 0;
  MinOneIs0 = False;
  LowLim4 = -8; LowLim8 = -128;

  /* zu ignorierendes */

  if (Memo(""))
    return;

  /* Formatangabe abspalten */

  switch (AttrSplit)
  {
    case '.':
      p = strchr(AttrPart.Str, ':');
      if (p)
      {
        Format = (p < AttrPart.Str + strlen(AttrPart.Str) - 1) ? p[1] : ' ';
        *p = '\0';
      }
      else
        Format = ' ';
      break;
    case ':':
      p = strchr(AttrPart.Str, '.');
      if (!p)
      {
        Format = (*AttrPart.Str);
        *AttrPart.Str = '\0';
      }
      else
      {
        Format = (p == AttrPart.Str) ? ' ' : *AttrPart.Str;
        strmov(AttrPart.Str, p + 1);
      }
      break;
    default:
      Format = ' ';
  }
  Format = mytoupper(Format);

  /* Attribut abarbeiten */

  if (!*AttrPart.Str)
    OpSize = -1;
  else
   switch (mytoupper(*AttrPart.Str))
   {
     case 'B':
       OpSize = 0;
       break;
     case 'W':
       OpSize = 1;
       break;
     case 'D':
       OpSize = 2;
       break;
     default:
      WrError(ErrNum_UndefAttr);
      return;
   }

  /* Pseudoanweisungen */

  if (DecodePseudo()) return;

  if (DecodeIntelPseudo(False)) return;

  if (LookupInstTable(InstTable, OpPart.Str))
  {
    AddPrefixes();
    return;
  }

  WrStrErrorPos(ErrNum_UnknownInstruction, &OpPart);
}

static Boolean IsDef_97C241(void)
{
  return False;
}

static void SwitchFrom_97C241(void)
{
  DeinitFields();
}

static void SwitchTo_97C241(void)
{
  TurnWords = False;
  ConstMode = ConstModeIntel;

  PCSymbol = "$";
  HeaderID = 0x56;
  NOPCode = 0x7fa0;
  DivideChars = ",";
  HasAttrs = True;
  AttrChars = ".:";

  ValidSegs = 1 << SegCode;
  Grans[SegCode] = 1;
  ListGrans[SegCode] = 2;
  SegInits[SegCode] = 0;
  SegLimits[SegCode] = 0xffffffl;

  MakeCode = MakeCode_97C241;
  IsDef = IsDef_97C241;
  SwitchFrom = SwitchFrom_97C241;
  InitFields();
}

void code97c241_init(void)
{
  CPU97C241 = AddCPU("97C241", SwitchTo_97C241);
}
