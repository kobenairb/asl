/* code29k.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator AM29xxx-Familie                                             */
/*                                                                           */
/* Historie: 18.11.1996 Grundsteinlegung                                     */
/*            3. 1.1999 ChkPC-Anpassung                                      */
/*            9. 3.2000 'ambiguous else'-Warnungen beseitigt                 */
/*                                                                           */
/*****************************************************************************/
/* $Id: code29k.c,v 1.10 2014/12/07 19:13:59 alfred Exp $                     */
/*****************************************************************************
 * $Log: code29k.c,v $
 * Revision 1.10  2014/12/07 19:13:59  alfred
 * - silence a couple of Borland C related warnings and errors
 *
 * Revision 1.9  2014/12/05 11:58:15  alfred
 * - collapse STDC queries into one file
 *
 * Revision 1.8  2014/11/05 15:47:13  alfred
 * - replace InitPass callchain with registry
 *
 * Revision 1.7  2014/10/05 14:02:21  alfred
 * - rework to current style
 *
 * Revision 1.6  2014/03/08 21:06:35  alfred
 * - rework ASSUME framework
 *
 * Revision 1.5  2007/11/24 22:48:03  alfred
 * - some NetBSD changes
 *
 * Revision 1.4  2005/10/02 10:00:44  alfred
 * - ConstLongInt gets default base, correct length check on KCPSM3 registers
 *
 * Revision 1.3  2005/09/08 17:31:03  alfred
 * - add missing include
 *
 * Revision 1.2  2004/05/29 11:33:00  alfred
 * - relocated DecodeIntelPseudo() into own module
 *
 *****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "nls.h"
#include "strutil.h"
#include "bpemu.h"
#include "stringlists.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmallg.h"
#include "asmitree.h"
#include "codepseudo.h" 
#include "intpseudo.h"
#include "codevars.h"
#include "intconsts.h"
#include "errmsg.h"

#include "code29k.h"

typedef struct
{
  Boolean MustSup;
  CPUVar MinCPU;
  LongWord Code;
} StdOrder;

typedef struct
{
  Boolean HasReg, HasInd;
  CPUVar MinCPU;
  LongWord Code;
} JmpOrder;

typedef struct
{
  char *Name;
  LongWord Code;
} SPReg;

#define StdOrderCount 51
#define NoImmOrderCount 22
#define VecOrderCount 10
#define JmpOrderCount 5
#define FixedOrderCount 2
#define MemOrderCount 7
#define SPRegCount 28

static StdOrder *StdOrders;
static StdOrder *NoImmOrders;
static StdOrder *VecOrders;
static JmpOrder *JmpOrders;
static StdOrder *FixedOrders;
static StdOrder *MemOrders;
static SPReg *SPRegs;


static CPUVar CPU29000, CPU29240, CPU29243, CPU29245;
static LongInt Reg_RBP;
static StringList Emulations;

/*-------------------------------------------------------------------------*/

static void ChkSup(void)
{
  if (!SupAllowed)
    WrError(50);
}

static Boolean IsSup(LongWord RegNo)
{
  return ((RegNo < 0x80) || (RegNo >= 0xa0));
}

static Boolean ChkCPU(CPUVar Min)
{
  return StringListPresent(Emulations, OpPart.Str) ? True : ChkMinCPU(Min);
}

/*-------------------------------------------------------------------------*/

static Boolean DecodeReg(char *Asc, LongWord *Erg)
{
  Boolean io, OK;

  if ((strlen(Asc) >= 2) && (mytoupper(*Asc) == 'R'))
  {
    *Erg = ConstLongInt(Asc + 1, &io, 10);
    OK = ((io) && (*Erg <= 255));
  }
  else if ((strlen(Asc) >= 3) && (mytoupper(*Asc) == 'G') && (mytoupper(Asc[1]) == 'R'))
  {
    *Erg = ConstLongInt(Asc + 2, &io, 10);
    OK = ((io) && (*Erg <= 127));
  }
  else if ((strlen(Asc) >= 3) && (mytoupper(*Asc) == 'L') && (mytoupper(Asc[1]) == 'R'))
  {
    *Erg = ConstLongInt(Asc + 2, &io, 10);
    OK = ((io) && (*Erg <= 127));
    *Erg += 128;
  }
  else
    OK = False;
  if (OK)
  {
    if ((*Erg < 127) && (Odd(Reg_RBP >> ((*Erg) >> 4))))
      ChkSup();
  }
  return OK;
}

static Boolean DecodeArgReg(int ArgIndex, LongWord *pRes)
{
  Boolean Result = DecodeReg(ArgStr[ArgIndex], pRes);

  if (!Result)
    WrXErrorPos(ErrNum_InvReg, ArgStr[ArgIndex], &ArgStrPos[ArgIndex]);
  return Result;
}

static Boolean DecodeSpReg(char *Asc_O, LongWord *Erg)
{
  int z;
  String Asc;

  strmaxcpy(Asc, Asc_O, 255);
  NLS_UpString(Asc);
  for (z = 0; z < SPRegCount; z++)
   if (!strcmp(Asc, SPRegs[z].Name))
   {
     *Erg = SPRegs[z].Code;
     break;
   }
  return (z < SPRegCount);
}

static Boolean DecodeArgSpReg(int ArgIndex, LongWord *pRes)
{
  Boolean Result = DecodeSpReg(ArgStr[ArgIndex], pRes);

  if (!Result)
    WrXErrorPos(ErrNum_InvCtrlReg, ArgStr[ArgIndex], &ArgStrPos[ArgIndex]);
  return Result;
}

/*-------------------------------------------------------------------------*/

/* Variante 1: Register <-- Register op Register/uimm8 */

static void DecodeStd(Word Index)
{
  const StdOrder *pOrder = StdOrders + Index;
  LongWord Dest, Src1, Src2, Src3;
  Boolean OK;

  if (ChkArgCnt(2, 3) && DecodeArgReg(1, &Dest))
  {
    OK = True;
    if (ArgCnt == 2) Src1 = Dest;
    else OK = DecodeArgReg(2, &Src1);
    if (OK)
    {
      if (DecodeReg(ArgStr[ArgCnt], &Src2))
      {
        OK = True; Src3 = 0;
      }
      else
      {
        Src2 = EvalIntExpression(ArgStr[ArgCnt], UInt8, &OK);
        Src3 = 0x1000000;
      }
      if (OK)
      {
        CodeLen = 4;
        DAsmCode[0] = (pOrder->Code << 24) + Src3 + (Dest << 16) + (Src1 << 8) + Src2;
        if (pOrder->MustSup)
          ChkSup();
      }
    }
  }
}

/* Variante 2: Register <-- Register op Register */

static void DecodeNoImm(Word Index)
{
  const StdOrder *pOrder = NoImmOrders + Index;
  Boolean OK;
  LongWord Dest, Src1, Src2;

  if (ChkArgCnt(2, 3) && DecodeArgReg(1, &Dest))
  {
    OK = True;
    if (ArgCnt == 2) Src1 = Dest;
    else OK = DecodeArgReg(2, &Src1);
    if (OK && DecodeArgReg(ArgCnt, &Src2))
    {
      CodeLen = 4;
      DAsmCode[0] = (pOrder->Code << 24) + (Dest << 16) + (Src1 << 8) + Src2;
      if (pOrder->MustSup)
        ChkSup();
    }
  }
}

/* Variante 3: Vektor <-- Register op Register/uimm8 */

static void DecodeVec(Word Index)
{
  const StdOrder *pOrder = VecOrders + Index;
  Boolean OK;
  LongWord Dest, Src1, Src2, Src3;

  if (ChkArgCnt(3, 3))
  {
    FirstPassUnknown = False;
    Dest = EvalIntExpression(ArgStr[1], UInt8, &OK);
    if (FirstPassUnknown) Dest = 64;
    if (OK)
    {
      if (DecodeArgReg(2, &Src1))
      {
        if (DecodeReg(ArgStr[ArgCnt], &Src2))
        {
          OK = True; Src3 = 0;
        }
        else
        {
          Src2 = EvalIntExpression(ArgStr[ArgCnt], UInt8, &OK);
          Src3 = 0x1000000;
        }
        if (OK)
        {
          CodeLen = 4;
          DAsmCode[0] = (pOrder->Code << 24) + Src3 + (Dest << 16) + (Src1 << 8) + Src2;
          if ((pOrder->MustSup) || (Dest <= 63))
            ChkSup();
        }
      }
    }
  }
}

/* Variante 4: ohne Operanden */

static void DecodeFixed(Word Code)
{
  const StdOrder *pOrder = FixedOrders + Code;

  if (ChkArgCnt(0, 0))
  {
    CodeLen = 4;
    DAsmCode[0] = pOrder->Code << 24;
    if (pOrder->MustSup)
      ChkSup();
  }
}

/* Variante 5 : [0], Speichersteuerwort, Register, Register/uimm8 */

static void DecodeMem(Word Index)
{
  const StdOrder *pOrder = MemOrders + Index;
  Boolean OK;
  LongWord AdrLong, Dest, Src1, Src2, Src3;

  if (ChkArgCnt(3, 4))
  {
    if (ArgCnt == 3)
    {
      OK = True; AdrLong = 0;
    }
    else
    {
      AdrLong = EvalIntExpression(ArgStr[1], Int32, &OK);
      if (OK) OK = ChkRange(AdrLong, 0, 0);
    }
    if (OK)
    {
      Dest = EvalIntExpression(ArgStr[ArgCnt - 2], UInt7, &OK);
      if (OK && DecodeArgReg(ArgCnt - 1, &Src1))
      {
        if (DecodeReg(ArgStr[ArgCnt], &Src2))
        {
          OK = True; Src3 = 0;
        }
        else
        {
          Src2 = EvalIntExpression(ArgStr[ArgCnt], UInt8, &OK);
          Src3 = 0x1000000;
        }
        if (OK)
        {
          CodeLen = 4;
          DAsmCode[0] = (pOrder->Code << 24) + Src3 + (Dest << 16) + (Src1 << 8) + Src2;
          if (pOrder->MustSup)
            ChkSup();
        }
      }
    }
  }
}

/* Sprungbefehle */

static void DecodeJmp(Word Index)
{
  const JmpOrder *pOrder = JmpOrders + (Index & 0xff);
  Word Immediate = Index & 0x100;
  LongWord Dest, Src1, AdrLong;
  LongInt AdrInt;
  Boolean OK;
  unsigned NumArgs = 1 + Ord(pOrder->HasReg);

  if (ChkArgCnt(NumArgs, NumArgs)
   && DecodeReg(ArgStr[ArgCnt], &Src1))
  {
    if (!pOrder->HasReg)
    {
      Dest = 0;
      OK = True;
    }
    else OK = DecodeReg(ArgStr[1], &Dest);
    if (OK)
    {
      CodeLen = 4;
      DAsmCode[0] = ((pOrder->Code + 0x20) << 24) + (Dest << 8) + Src1;
    }
  }
  else if (Immediate) WrError(1350);
  else
  {
    if (!pOrder->HasReg)
    {
      Dest = 0;
      OK = True;
    }
    else
      OK = DecodeReg(ArgStr[1], &Dest);
    if (OK)
    {
      AdrLong = EvalIntExpression(ArgStr[ArgCnt], Int32, &OK); 
      AdrInt = AdrLong - EProgCounter();
      if (OK)
      {
        if ((AdrLong & 3) != 0) WrError(1325);
         else if ((AdrInt <= 0x1ffff) && (AdrInt >= -0x20000))
        {
          CodeLen = 4;
          AdrLong -= EProgCounter();
          DAsmCode[0] = (pOrder->Code << 24)
                      + ((AdrLong & 0x3fc00) << 6)
                      + (Dest << 8) + ((AdrLong & 0x3fc) >> 2);
        }
        else if ((!SymbolQuestionable) && (AdrLong > 0x3fffff)) WrError(1370);
        else
        {
          CodeLen = 4;
          DAsmCode[0] = ((pOrder->Code + 1) << 24)
                      + ((AdrLong & 0x3fc00) << 6)
                      + (Dest << 8) + ((AdrLong & 0x3fc) >> 2);
        }
      }
    }
  }
}

static void DecodeCLASS(Word Code)
{
  LongWord Dest, Src1, Src2;
  Boolean OK;

  UNUSED(Code);

  if (ChkArgCnt(3, 3)
   && ChkCPU(CPU29000)
   && DecodeArgReg(1, &Dest)
   && DecodeArgReg(2, &Src1))
  {
    Src2 = EvalIntExpression(ArgStr[3], UInt2, &OK);
    if (OK)
    {
      CodeLen = 4;
      DAsmCode[0] = 0xe6000000 + (Dest << 16) + (Src1 << 8) + Src2;
    }
  }
}

static void DecodeEMULATE(Word Code)
{
  LongWord Dest, Src1, Src2;
  Boolean OK;

  UNUSED(Code);

  if (ChkArgCnt(3, 3))
  {
    FirstPassUnknown = False;
    Dest = EvalIntExpression(ArgStr[1], UInt8, &OK);
    if (FirstPassUnknown) Dest = 64;
    if (OK)
    {
      if (DecodeArgReg(2, &Src1)
       && DecodeArgReg(ArgCnt, &Src2))
      {
        CodeLen = 4;
        DAsmCode[0] = 0xd7000000 + (Dest << 16) + (Src1 << 8) + Src2;
        if (Dest <= 63)
          ChkSup();
      }
    }
  }
}

static void DecodeSQRT(Word Code)
{
  Boolean OK;
  LongWord Src1, Src2, Dest;

  UNUSED(Code);

  if (ChkArgCnt(2, 3)
   && ChkCPU(CPU29000)
   && DecodeArgReg(1, &Dest))
  {
    if (ArgCnt == 2)
    {
      OK = True;
      Src1 = Dest;
    }
    else
      OK = DecodeArgReg(2, &Src1);
    if (OK)
    {
      Src2 = EvalIntExpression(ArgStr[ArgCnt], UInt2, &OK);
      if (OK)
      {
        CodeLen = 4;
        DAsmCode[0] = 0xe5000000 + (Dest << 16) + (Src1 << 8) + Src2;
      }
    }
  }
}

static void DecodeCLZ(Word Code)
{
  Boolean OK;
  LongWord Src1, Src3, Dest;

  UNUSED(Code);

  if (ChkArgCnt(2, 2) && DecodeArgReg(1, &Dest))
  {
    if (DecodeReg(ArgStr[2], &Src1))
    {
      OK = True; Src3 = 0;
    }
    else
    {
      Src1 = EvalIntExpression(ArgStr[2], UInt8, &OK);
      Src3 = 0x1000000;
    }
    if (OK)
    {
      CodeLen = 4;
      DAsmCode[0] = 0x08000000 + Src3 + (Dest << 16) + Src1;
    }
  }
}

static void DecodeCONST(Word Code)
{
  Boolean OK;
  LongWord AdrLong, Dest;
  UNUSED(Code);

  if (ChkArgCnt(2, 2) && DecodeArgReg(1, &Dest))
  {
    AdrLong = EvalIntExpression(ArgStr[2], Int32, &OK);
    if (OK)
    {
      CodeLen = 4;
      DAsmCode[0] = ((AdrLong & 0xff00) << 8) + (Dest << 8) + (AdrLong & 0xff);
      AdrLong = AdrLong >> 16;
      if (AdrLong == 0xffff) DAsmCode[0] += 0x01000000;
      else
      {
        DAsmCode[0] += 0x03000000;
        if (AdrLong != 0)
        {
          CodeLen = 8;
          DAsmCode[1] = 0x02000000 + ((AdrLong & 0xff00) << 16) + (Dest << 8) + (AdrLong & 0xff);
        }
      }
    }
  }
  return;
}

static void DecodeCONSTH_CONSTN(Word IsHi)
{
  Boolean OK;
  LongWord AdrLong, Dest;

  if (ChkArgCnt(2, 2) && DecodeArgReg(1, &Dest))
  {
    FirstPassUnknown = False;
    AdrLong = EvalIntExpression(ArgStr[2], Int32, &OK);
    if (FirstPassUnknown)
      AdrLong &= 0xffff;
    if ((!IsHi) && ((AdrLong >> 16) == 0xffff))
      AdrLong &= 0xffff;
    if (ChkRange(AdrLong, 0, 0xffff))
    {
      CodeLen = 4;
      DAsmCode[0] = 0x1000000 + ((AdrLong & 0xff00) << 8) + (Dest << 8) + (AdrLong & 0xff);
      if (IsHi)
        DAsmCode[0] += 0x1000000;
    }
  }
}

static void DecodeCONVERT(Word Code)
{
  Boolean OK;
  LongWord Src1, Src2, Dest;

  UNUSED(Code);

  if (ChkArgCnt(6, 6)
   && ChkCPU(CPU29000)
   && DecodeArgReg(1, &Dest)
   && DecodeArgReg(2, &Src1))
  {
    Src2 = 0;
    Src2 += EvalIntExpression(ArgStr[3], UInt1, &OK) << 7;
    if (OK)
    {
      Src2 += EvalIntExpression(ArgStr[4], UInt3, &OK) << 4;
      if (OK)
      {
        Src2 += EvalIntExpression(ArgStr[5], UInt2, &OK) << 2;
        if (OK)
        {
          Src2 += EvalIntExpression(ArgStr[6], UInt2, &OK);
          if (OK)
          {
            CodeLen = 4;
            DAsmCode[0] = 0xe4000000 + (Dest << 16) + (Src1 << 8) + Src2;
          }
        }
      }
    }
  }
}

static void DecodeEXHWS(Word Code)
{
  LongWord Src1, Dest;

  UNUSED(Code);

  if (ChkArgCnt(2, 2)
   && DecodeArgReg(1, &Dest)
   && DecodeArgReg(2, &Src1))
  {
    CodeLen = 4;
    DAsmCode[0] = 0x7e000000 + (Dest << 16) + (Src1 << 8);
  }
}

static void DecodeINV_IRETINV(Word Code)
{
  Boolean OK;
  LongWord Src1;

  if (ChkArgCnt(0, 1))
  {
    if (ArgCnt == 0)
    {
      Src1 = 0; OK = True;
    }
    else Src1 = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (OK)
    {
      CodeLen = 4;
      DAsmCode[0] = (((LongWord)Code) << 16) | Src1 << 16;
      ChkSup();
    }
  }
}

static void DecodeMFSR(Word Code)
{
  LongWord Src1, Dest;

  UNUSED(Code);

  if (ChkArgCnt(2, 2)
   && DecodeArgReg(1, &Dest)
   && DecodeArgSpReg(2, &Src1))
  {
    DAsmCode[0] = 0xc6000000 + (Dest << 16) + (Src1 << 8);
    CodeLen = 4;
    if (IsSup(Src1))
      ChkSup();
  }
}

static void DecodeMTSR(Word Code)
{
  LongWord Src1, Dest;

  UNUSED(Code);

  if (ChkArgCnt(2, 2)
   && DecodeArgSpReg(1, &Dest)
   && DecodeArgReg(2, &Src1))
  {
    DAsmCode[0] = 0xce000000 + (Dest << 8) + Src1;
    CodeLen = 4;
    if (IsSup(Dest))
      ChkSup();
  }
}

static void DecodeMTSRIM(Word Code)
{
  LongWord Src1, Dest;
  Boolean OK;

  UNUSED(Code);

  if (ChkArgCnt(2, 2) && DecodeArgSpReg(1, &Dest))
  {
    Src1 = EvalIntExpression(ArgStr[2], UInt16, &OK);
    if (OK)
    {
      DAsmCode[0] = 0x04000000 + ((Src1 & 0xff00) << 8) + (Dest << 8) + Lo(Src1);
      CodeLen = 4;
      if (IsSup(Dest))
        ChkSup();
    }
  }
}

static void DecodeMFTLB(Word Code)
{
  LongWord Src1, Dest;

  UNUSED(Code);

  if (ChkArgCnt(2, 2)
   && DecodeArgReg(1, &Dest)
   && DecodeArgReg(2, &Src1))
  {
    DAsmCode[0] = 0xb6000000 + (Dest << 16) + (Src1 << 8);
    CodeLen = 4;
    ChkSup();
  }
}

static void DecodeMTTLB(Word Code)
{
  LongWord Src1, Dest;

  UNUSED(Code);

  if (ChkArgCnt(2, 2)
   && DecodeArgReg(1, &Dest)
   && DecodeArgReg(2, &Src1))
  {
    DAsmCode[0] = 0xbe000000 + (Dest << 8) + Src1;
    CodeLen = 4;
    ChkSup();
  }
}

static void DecodeEMULATED(Word Code)
{
  int z;

  UNUSED(Code);

  if (ChkArgCnt(1, ArgCntMax))
    for (z = 1; z <= ArgCnt; z++)
    {
      NLS_UpString(ArgStr[z]);
      if (!StringListPresent(Emulations, ArgStr[z]))
        AddStringListLast(&Emulations, ArgStr[z]);
    }
}

/*-------------------------------------------------------------------------*/

static void AddStd(char *NName, CPUVar NMin, Boolean NSup, LongWord NCode)
{
  if (InstrZ >= StdOrderCount) exit(255);
  StdOrders[InstrZ].Code = NCode;
  StdOrders[InstrZ].MustSup = NSup;
  StdOrders[InstrZ].MinCPU = NMin;
  AddInstTable(InstTable, NName, InstrZ++, DecodeStd);
}

static void AddNoImm(char *NName, CPUVar NMin, Boolean NSup, LongWord NCode)
{
  if (InstrZ >= NoImmOrderCount) exit(255);
  NoImmOrders[InstrZ].Code = NCode;
  NoImmOrders[InstrZ].MustSup = NSup;
  NoImmOrders[InstrZ].MinCPU = NMin;
  AddInstTable(InstTable, NName, InstrZ++, DecodeNoImm);
}

static void AddVec(char *NName, CPUVar NMin, Boolean NSup, LongWord NCode)
{
  if (InstrZ >= VecOrderCount) exit(255);
  VecOrders[InstrZ].Code = NCode;
  VecOrders[InstrZ].MustSup = NSup;
  VecOrders[InstrZ].MinCPU = NMin;
  AddInstTable(InstTable, NName, InstrZ++, DecodeVec);
}

static void AddJmp(char *NName, CPUVar NMin, Boolean NHas, Boolean NInd, LongWord NCode)
{
  char IName[30];

  if (InstrZ >= JmpOrderCount) exit(255);
  JmpOrders[InstrZ].HasReg = NHas;
  JmpOrders[InstrZ].HasInd = NInd;
  JmpOrders[InstrZ].Code = NCode;
  JmpOrders[InstrZ].MinCPU = NMin;
  AddInstTable(InstTable, NName, InstrZ, DecodeJmp);
  sprintf(IName, "%sI", NName);
  AddInstTable(InstTable, IName, 0x100 | InstrZ, DecodeJmp);
  InstrZ++;
}

static void AddFixed(char *NName, CPUVar NMin, Boolean NSup, LongWord NCode)
{
  if (InstrZ >= FixedOrderCount) exit(255);
  FixedOrders[InstrZ].Code = NCode;
  FixedOrders[InstrZ].MustSup = NSup;
  FixedOrders[InstrZ].MinCPU = NMin;
  AddInstTable(InstTable, NName, InstrZ++, DecodeFixed);
}

static void AddMem(char *NName, CPUVar NMin, Boolean NSup, LongWord NCode)
{
  if (InstrZ >= MemOrderCount) exit(255);
  MemOrders[InstrZ].Code = NCode;
  MemOrders[InstrZ].MustSup = NSup;
  MemOrders[InstrZ].MinCPU = NMin;
  AddInstTable(InstTable, NName, InstrZ++, DecodeMem);
}

static void AddSP(char *NName, LongWord NCode)
{
  if (InstrZ >= SPRegCount) exit(255);
  SPRegs[InstrZ].Name = NName;
  SPRegs[InstrZ++].Code = NCode;
}

static void InitFields(void)
{
  InstTable = CreateInstTable(307);
  SetDynamicInstTable(InstTable);
  AddInstTable(InstTable, "CLASS", 0, DecodeCLASS);
  AddInstTable(InstTable, "EMULATE", 0, DecodeEMULATE);
  AddInstTable(InstTable, "SQRT", 0, DecodeSQRT);
  AddInstTable(InstTable, "CLZ", 0, DecodeCLZ);
  AddInstTable(InstTable, "CONST", 0, DecodeCONST);
  AddInstTable(InstTable, "CONSTH", True, DecodeCONSTH_CONSTN);
  AddInstTable(InstTable, "CONSTN", False, DecodeCONSTH_CONSTN);
  AddInstTable(InstTable, "CONVERT", 0, DecodeCONVERT);
  AddInstTable(InstTable, "EXHWS", 0, DecodeEXHWS);
  AddInstTable(InstTable, "INV", 0x9f00, DecodeINV_IRETINV);   
  AddInstTable(InstTable, "IRETINV", 0x8c00, DecodeINV_IRETINV);
  AddInstTable(InstTable, "MFSR", 0, DecodeMFSR);
  AddInstTable(InstTable, "MTSR", 0, DecodeMTSR);
  AddInstTable(InstTable, "MTSRIM", 0, DecodeMTSRIM);
  AddInstTable(InstTable, "MFTLB", 0, DecodeMFTLB);
  AddInstTable(InstTable, "MTTLB", 0, DecodeMTTLB);
  AddInstTable(InstTable, "EMULATED", 0, DecodeEMULATED);

  StdOrders = (StdOrder *) malloc(sizeof(StdOrder)*StdOrderCount); InstrZ = 0;
  AddStd("ADD"    , CPU29245, False, 0x14); AddStd("ADDC"   , CPU29245, False, 0x1c);
  AddStd("ADDCS"  , CPU29245, False, 0x18); AddStd("ADDCU"  , CPU29245, False, 0x1a);
  AddStd("ADDS"   , CPU29245, False, 0x10); AddStd("ADDU"   , CPU29245, False, 0x12);
  AddStd("AND"    , CPU29245, False, 0x90); AddStd("ANDN"   , CPU29245, False, 0x9c);
  AddStd("CPBYTE" , CPU29245, False, 0x2e); AddStd("CPEQ"   , CPU29245, False, 0x60);
  AddStd("CPGE"   , CPU29245, False, 0x4c); AddStd("CPGEU"  , CPU29245, False, 0x4e);
  AddStd("CPGT"   , CPU29245, False, 0x48); AddStd("CPGTU"  , CPU29245, False, 0x4a);
  AddStd("CPLE"   , CPU29245, False, 0x44); AddStd("CPLEU"  , CPU29245, False, 0x46);
  AddStd("CPLT"   , CPU29245, False, 0x40); AddStd("CPLTU"  , CPU29245, False, 0x42);
  AddStd("CPNEQ"  , CPU29245, False, 0x62); AddStd("DIV"    , CPU29245, False, 0x6a);
  AddStd("DIV0"   , CPU29245, False, 0x68); AddStd("DIVL"   , CPU29245, False, 0x6c);
  AddStd("DIVREM" , CPU29245, False, 0x6e); AddStd("EXBYTE" , CPU29245, False, 0x0a);
  AddStd("EXHW"   , CPU29245, False, 0x7c); AddStd("EXTRACT", CPU29245, False, 0x7a);
  AddStd("INBYTE" , CPU29245, False, 0x0c); AddStd("INHW"   , CPU29245, False, 0x78);
  AddStd("MUL"    , CPU29245, False, 0x64); AddStd("MULL"   , CPU29245, False, 0x66);
  AddStd("MULU"   , CPU29245, False, 0x74); AddStd("NAND"   , CPU29245, False, 0x9a);
  AddStd("NOR"    , CPU29245, False, 0x98); AddStd("OR"     , CPU29245, False, 0x92);
  AddStd("SLL"    , CPU29245, False, 0x80); AddStd("SRA"    , CPU29245, False, 0x86);
  AddStd("SRL"    , CPU29245, False, 0x82); AddStd("SUB"    , CPU29245, False, 0x24);
  AddStd("SUBC"   , CPU29245, False, 0x2c); AddStd("SUBCS"  , CPU29245, False, 0x28);
  AddStd("SUBCU"  , CPU29245, False, 0x2a); AddStd("SUBR"   , CPU29245, False, 0x34);
  AddStd("SUBRC"  , CPU29245, False, 0x3c); AddStd("SUBRCS" , CPU29245, False, 0x38);
  AddStd("SUBRCU" , CPU29245, False, 0x3a); AddStd("SUBRS"  , CPU29245, False, 0x30);
  AddStd("SUBRU"  , CPU29245, False, 0x32); AddStd("SUBS"   , CPU29245, False, 0x20);
  AddStd("SUBU"   , CPU29245, False, 0x22); AddStd("XNOR"   , CPU29245, False, 0x96);
  AddStd("XOR"    , CPU29245, False, 0x94);

  NoImmOrders = (StdOrder *) malloc(sizeof(StdOrder)*NoImmOrderCount); InstrZ = 0;
  AddNoImm("DADD"    , CPU29000, False, 0xf1); AddNoImm("DDIV"    , CPU29000, False, 0xf7);
  AddNoImm("DEQ"     , CPU29000, False, 0xeb); AddNoImm("DGE"     , CPU29000, False, 0xef);
  AddNoImm("DGT"     , CPU29000, False, 0xed); AddNoImm("DIVIDE"  , CPU29000, False, 0xe1);
  AddNoImm("DIVIDU"  , CPU29000, False, 0xe3); AddNoImm("DMUL"    , CPU29000, False, 0xf5);
  AddNoImm("DSUB"    , CPU29000, False, 0xf3); AddNoImm("FADD"    , CPU29000, False, 0xf0);
  AddNoImm("FDIV"    , CPU29000, False, 0xf6); AddNoImm("FDMUL"   , CPU29000, False, 0xf9);
  AddNoImm("FEQ"     , CPU29000, False, 0xea); AddNoImm("FGE"     , CPU29000, False, 0xee);
  AddNoImm("FGT"     , CPU29000, False, 0xec); AddNoImm("FMUL"    , CPU29000, False, 0xf4);
  AddNoImm("FSUB"    , CPU29000, False, 0xf2); AddNoImm("MULTIPLU", CPU29243, False, 0xe2);
  AddNoImm("MULTIPLY", CPU29243, False, 0xe0); AddNoImm("MULTM"   , CPU29243, False, 0xde);
  AddNoImm("MULTMU"  , CPU29243, False, 0xdf); AddNoImm("SETIP"   , CPU29245, False, 0x9e);

  VecOrders = (StdOrder *) malloc(sizeof(StdOrder)*VecOrderCount); InstrZ = 0;
  AddVec("ASEQ"   , CPU29245, False, 0x70); AddVec("ASGE"   , CPU29245, False, 0x5c);
  AddVec("ASGEU"  , CPU29245, False, 0x5e); AddVec("ASGT"   , CPU29245, False, 0x58);
  AddVec("ASGTU"  , CPU29245, False, 0x5a); AddVec("ASLE"   , CPU29245, False, 0x54);
  AddVec("ASLEU"  , CPU29245, False, 0x56); AddVec("ASLT"   , CPU29245, False, 0x50);
  AddVec("ASLTU"  , CPU29245, False, 0x52); AddVec("ASNEQ"  , CPU29245, False, 0x72);

  JmpOrders = (JmpOrder *) malloc(sizeof(JmpOrder)*JmpOrderCount); InstrZ = 0;
  AddJmp("CALL"   , CPU29245, True , True , 0xa8); AddJmp("JMP"    , CPU29245, False, True , 0xa0);
  AddJmp("JMPF"   , CPU29245, True , True , 0xa4); AddJmp("JMPFDEC", CPU29245, True , False, 0xb4);
  AddJmp("JMPT"   , CPU29245, True , True , 0xac);

  FixedOrders = (StdOrder *) malloc(sizeof(StdOrder)*FixedOrderCount); InstrZ = 0;
  AddFixed("HALT"   , CPU29245, True, 0x89); AddFixed("IRET"   , CPU29245, True, 0x88);

  MemOrders = (StdOrder *) malloc(sizeof(StdOrder)*MemOrderCount); InstrZ = 0;
  AddMem("LOAD"   , CPU29245, False, 0x16); AddMem("LOADL"  , CPU29245, False, 0x06);
  AddMem("LOADM"  , CPU29245, False, 0x36); AddMem("LOADSET", CPU29245, False, 0x26);
  AddMem("STORE"  , CPU29245, False, 0x1e); AddMem("STOREL" , CPU29245, False, 0x0e);
  AddMem("STOREM" , CPU29245, False, 0x3e);

  SPRegs = (SPReg *) malloc(sizeof(SPReg)*SPRegCount); InstrZ = 0;
  AddSP("VAB",   0);
  AddSP("OPS",   1);
  AddSP("CPS",   2);
  AddSP("CFG",   3);
  AddSP("CHA",   4);
  AddSP("CHD",   5);
  AddSP("CHC",   6);
  AddSP("RBP",   7);
  AddSP("TMC",   8);
  AddSP("TMR",   9);
  AddSP("PC0",  10);
  AddSP("PC1",  11);
  AddSP("PC2",  12);
  AddSP("MMU",  13);
  AddSP("LRU",  14);
  AddSP("CIR",  29);
  AddSP("CDR",  30);
  AddSP("IPC", 128);
  AddSP("IPA", 129);
  AddSP("IPB", 130);
  AddSP("Q",   131);
  AddSP("ALU", 132);
  AddSP("BP",  133);
  AddSP("FC",  134);
  AddSP("CR",  135);
  AddSP("FPE", 160);
  AddSP("INTE",161);
  AddSP("FPS", 162);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
  free(StdOrders);
  free(NoImmOrders);
  free(VecOrders);
  free(JmpOrders);
  free(FixedOrders);
  free(MemOrders);
  free(SPRegs);
}

/*-------------------------------------------------------------------------*/

static void MakeCode_29K(void)
{
  CodeLen = 0;
  DontPrint = False;

  /* Nullanweisung */

  if (Memo("") && (*AttrPart == '\0') && (ArgCnt == 0))
    return;

  /* Pseudoanweisungen */

  if (DecodeIntelPseudo(True))
    return;

  if (!LookupInstTable(InstTable, OpPart.Str))
    WrStrErrorPos(ErrNum_UnknownOpcode, &OpPart);
}

static void InitCode_29K(void)
{
  Reg_RBP = 0;
  ClearStringList(&Emulations);
}

static Boolean IsDef_29K(void)
{
  return False;
}

static void SwitchFrom_29K(void)
{
  DeinitFields(); ClearONOFF();
}

static void SwitchTo_29K(void)
{
  static ASSUMERec ASSUME29Ks[] = 
  {
    {"RBP", &Reg_RBP, 0, 0xff, 0x00000000}
  };
  static const int ASSUME29KCount = sizeof(ASSUME29Ks) / sizeof(*ASSUME29Ks);
  TurnWords = True;
  ConstMode = ConstModeC;
  SetIsOccupied = False;

  PCSymbol = "$";
  HeaderID = 0x29;
  NOPCode = 0x000000000;
  DivideChars = ",";
  HasAttrs = False;

  ValidSegs = 1 << SegCode;
  Grans[SegCode] = 1; ListGrans[SegCode] = 4; SegInits[SegCode] = 0;
  SegLimits[SegCode] = INTCONST_ffffffff;

  MakeCode = MakeCode_29K;
  IsDef = IsDef_29K;
  AddONOFF("SUPMODE", &SupAllowed, SupAllowedName, False);

  pASSUMERecs = ASSUME29Ks;
  ASSUMERecCnt = ASSUME29KCount;

  SwitchFrom = SwitchFrom_29K; InitFields();
}

void code29k_init(void)
{
  CPU29245 = AddCPU("AM29245", SwitchTo_29K);
  CPU29243 = AddCPU("AM29243", SwitchTo_29K);
  CPU29240 = AddCPU("AM29240", SwitchTo_29K);
  CPU29000 = AddCPU("AM29000", SwitchTo_29K);

  Emulations = NULL;

  AddInitPassProc(InitCode_29K);
}
