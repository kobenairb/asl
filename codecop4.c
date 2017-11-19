/* codecop4.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegeneratormodul COP4-Familie                                           */
/*                                                                           */
/*****************************************************************************/
/* $Id: codecop4.c,v 1.9 2014/03/03 19:54:17 alfred Exp $                   *
 *****************************************************************************
 * $Log: codecop4.c,v $
 * Revision 1.9  2014/03/03 19:54:17  alfred
 * - add COP444 target
 *
 * Revision 1.8  2013-06-16 17:18:20  alfred
 * - add COP440 target
 *
 * Revision 1.7  2006/08/05 18:07:31  alfred
 * - silence some warnings
 *
 * Revision 1.6  2006/05/22 17:17:11  alfred
 * - address space is 1K for COP42x
 *
 * Revision 1.5  2006/05/07 13:42:52  alfred
 * - regard JP target on next page
 *
 * Revision 1.4  2006/05/06 10:26:38  alfred
 * - add COP42x instructions
 *
 * Revision 1.3  2006/05/06 09:21:34  alfred
 * - catch invalid values for AISC & LBI
 *
 * Revision 1.2  2006/04/09 12:40:11  alfred
 * - unify COP pseudo instructions
 *
 * Revision 1.1  2006/04/06 20:26:54  alfred
 * - add COP4
 *
 *****************************************************************************/

#include "stdinc.h"

#include "bpemu.h"
#include "asmdef.h"
#include "asmpars.h"
#include "asmsub.h"
#include "asmitree.h"
#include "headids.h"
#include "codevars.h"
#include "intpseudo.h"
#include "natpseudo.h"
#include "errmsg.h"

#include "codecop4.h"

#define FixedOrderCnt 44
#define ImmOrderCnt 3

#define M_CPUCOP410 (1 << 0)
#define M_CPUCOP420 (1 << 1)
#define M_CPUCOP440 (1 << 2)
#define M_CPUCOP444 (1 << 3)

typedef struct
{
  Word CPUMask;
  Word Code;
} FixedOrder;

static CPUVar CPUCOP410, CPUCOP420, CPUCOP440, CPUCOP444;
static IntType AdrInt;

static FixedOrder *FixedOrders, *ImmOrders;

/*---------------------------------------------------------------------------*/
/* Code Generators */

static void DecodeFixed(Word Index)
{
  FixedOrder *pOrder = FixedOrders + Index;

  if (ChkArgCnt(0, 0)
   && (ChkExactCPUMask(pOrder->CPUMask, CPUCOP410) >= 0))
  {
    if (Hi(pOrder->Code))
      BAsmCode[CodeLen++] = Hi(pOrder->Code);
    BAsmCode[CodeLen++] = Lo(pOrder->Code);
  }
}

static void DecodeSK(Word Index)
{
  if (ChkArgCnt(1, 1))
  {
    Byte Bit;
    Boolean OK;

    Bit = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (OK)
    {
      if (Index)
        BAsmCode[CodeLen++] = Index;
      BAsmCode[CodeLen++] = 0x01 | ((Bit & 1) << 4) | (Bit & 2);
    }
  }
}

static void DecodeImm(Word Index)
{
  FixedOrder *pOrder = ImmOrders + Index;

  if (ChkArgCnt(1, 1)
   && (ChkExactCPUMask(pOrder->CPUMask, CPUCOP410) >= 0))
  {
    Byte Val;
    Boolean OK;

    Val = EvalIntExpression(ArgStr[1], Int4, &OK);
    if (OK)
    {
      if (Hi(pOrder->Code))
        BAsmCode[CodeLen++] = Hi(pOrder->Code);
      BAsmCode[CodeLen++] = Lo(pOrder->Code) | (Val & 0x0f);
    }
  }
}

static void DecodeJmp(Word Index)
{
  if (ChkArgCnt(1, 1))
  {
    Word Addr;
    Boolean OK;
 
    Addr = EvalIntExpression(ArgStr[1], AdrInt, &OK);
    if (OK)
    {
      BAsmCode[CodeLen++] = Index | Hi(Addr);
      BAsmCode[CodeLen++] = Lo(Addr);
    }
  }
}
 
static void DecodeReg(Word Index)
{
  if (ChkArgCnt(1, 1))
  {
    Byte Reg;
    Boolean OK;

    Reg = EvalIntExpression(ArgStr[1], UInt2, &OK);   
    if (OK)
    {
      BAsmCode[CodeLen++] = Index | ((Reg & 3) << 4);;
    }
  }
}

static void DecodeAISC(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(1, 1))
  {
    Byte Val;
    Boolean OK;

    FirstPassUnknown = False;
    Val = EvalIntExpression(ArgStr[1], Int4, &OK);
    if (FirstPassUnknown)
      Val = 1;
    if (OK)
    {
      if (!Val) WrError(1315);
      else
        BAsmCode[CodeLen++] = 0x50 | (Val & 0x0f);
    }
  }
}

static void DecodeRMB(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(1, 1))
  {
    Byte Reg;
    Boolean OK;
    static Byte Vals[4] = { 0x4c, 0x45, 0x42, 0x43 };
 
    Reg = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (OK)
    {
      BAsmCode[CodeLen++] = Vals[Reg & 3];
    }
  }
}

static void DecodeSMB(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(1, 1))
  {   
    Byte Reg;
    Boolean OK;
    static Byte Vals[4] = { 0x4d, 0x47, 0x46, 0x4b };
 
    Reg = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (OK)
    {
      BAsmCode[CodeLen++] = Vals[Reg & 3];                
    }
  }  
}    

static void DecodeXAD(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(2, 2))
  {   
    Byte Reg1, Reg2;
    Boolean OK;
 
    FirstPassUnknown = False;
    Reg1 = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if ((FirstPassUnknown) && (MomCPU < CPUCOP420))
      Reg1 = 3;
    if (OK)
    {
      FirstPassUnknown = False;
      Reg2 = EvalIntExpression(ArgStr[2], UInt4, &OK);
      if ((FirstPassUnknown) && (MomCPU < CPUCOP420))
        Reg2 = 15;
      if (OK)
      {
        if ((MomCPU < CPUCOP420) && ((Reg1 != 3) || (Reg2 != 15))) WrError(1350);
        else
        {
          BAsmCode[CodeLen++] = 0x23;
          BAsmCode[CodeLen++] = 0x80 | (Reg1 << 4) | Reg2;
        }
      }
    }
  }  
}    
     
static void DecodeLBI(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(2, 2))
  {   
    Byte Reg, Val;
    Boolean OK;
 
    Reg = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (OK)
    {
      FirstPassUnknown = False;
      Val = EvalIntExpression(ArgStr[2], UInt4, &OK);
      if (FirstPassUnknown)
        Val = 0;
      if (OK)
      {
        if ((Val > 0) && (Val < 9))
        {
          if (ChkExactCPUMask(M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP440, CPUCOP410) >= 0)
          {
            BAsmCode[CodeLen++] = 0x33;
            BAsmCode[CodeLen++] = 0x80 | (Reg << 4) | Val;
          }
        }
        else
        {
          Val = (Val - 1) & 0x0f;
          BAsmCode[CodeLen++] = (Reg << 4) | Val;
        }
      }
    }
  }
}

static void DecodeLDD(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(2, 2)
   && (ChkExactCPUMask(M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP440, CPUCOP410) >= 0))
  {   
    Byte Reg, Val;
    Boolean OK;
 
    Reg = EvalIntExpression(ArgStr[1], UInt2, &OK);
    if (OK)
    {
      Val = EvalIntExpression(ArgStr[2], UInt4, &OK);
      if (OK)
      {
        BAsmCode[CodeLen++] = 0x23;
        BAsmCode[CodeLen++] = (Reg << 4) | Val;
      }
    }
  }
}

static void DecodeJSRP(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(1, 1))
  {
    Word Addr;
    Boolean OK;
 
    FirstPassUnknown = FALSE;
    Addr = EvalIntExpression(ArgStr[1], AdrInt, &OK);
    if (FirstPassUnknown)
      Addr = 2 << 6;
    if (OK)
    {
      if (((Addr >> 6) != 2) || (Addr == 0xbf)) WrError(1905);
      else if ((EProgCounter() >> 7) == 1) WrError(1900);
      else
        BAsmCode[CodeLen++] = 0x80 | (Addr & 0x3f);
    }
  }   
}     

static void DecodeJP(Word Index)
{
  UNUSED(Index);

  if (ChkArgCnt(1, 1))
  {
    Word Addr, CurrPC;
    Boolean OK;
 
    FirstPassUnknown = FALSE;
    Addr = EvalIntExpression(ArgStr[1], AdrInt, &OK);
    if (FirstPassUnknown)
      Addr = EProgCounter() & (~0x1f);
    if (OK)
    {
      CurrPC = EProgCounter();
      if ((Addr & 0x3f) == 0x3f)
        WrError(1905);
      if (((CurrPC >> 7) == 1) && ((Addr >> 7) == 1))
        BAsmCode[CodeLen++] = 0x80 | (Addr & 0x7f);
      else
      {
        Word PossPage;

        PossPage = CurrPC >> 6;
        if ((CurrPC & 0x3f) == 0x3f)
        {
          PossPage++;
          if (FirstPassUnknown)
            Addr += 0x40;
        }

        if (PossPage == (Addr >> 6))
          BAsmCode[CodeLen++] = 0xc0 | (Addr & 0x3f);
        else
          WrError(1905);
      }
    }
  }   
}     

/*---------------------------------------------------------------------------*/
/* Code Table Handling */

static void AddFixed(char *NName, Word NCode, Word NMask)
{
  if (InstrZ >= FixedOrderCnt)
    exit(0);
  else
  {
    FixedOrders[InstrZ].Code = NCode;
    FixedOrders[InstrZ].CPUMask = NMask;
    AddInstTable(InstTable, NName, InstrZ++, DecodeFixed);
  }
}

static void AddImm(char *NName, Word NCode, Word NMask)
{
  if (InstrZ >= ImmOrderCnt)
    exit(0);
  else
  {
    ImmOrders[InstrZ].Code = NCode;
    ImmOrders[InstrZ].CPUMask = NMask;
    AddInstTable(InstTable, NName, InstrZ++, DecodeImm);
  }
}

static void InitFields(void)
{
  InstTable = CreateInstTable(173);

  FixedOrders = (FixedOrder*)malloc(sizeof(FixedOrder) * FixedOrderCnt);
  InstrZ = 0;
  AddFixed("ASC"  , 0x30,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("ADD"  , 0x31,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CLRA" , 0x00,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("COMP" , 0x40,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("NOP"  , 0x44,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("RC"   , 0x32,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("SC"   , 0x22,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("XOR"  , 0x02,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("JID"  , 0xff,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("RET"  , 0x48,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("RETSK", 0x49,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CAMQ" , 0x333c, M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CAME" , 0x331f,                             M_CPUCOP440              );
  AddFixed("CAMT" , 0x333f,                             M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CAMR" , 0x333d,                             M_CPUCOP440              );
  AddFixed("CEMA" , 0x330f,                             M_CPUCOP440              );
  AddFixed("CTMA" , 0x332f,                             M_CPUCOP440 | M_CPUCOP444);
  AddFixed("LQID" , 0xbf,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CAB"  , 0x50,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CBA"  , 0x4e,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("SKC"  , 0x20,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("SKE"  , 0x21,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("SKGZ" , 0x3321, M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("SKSZ" , 0x331c,                             M_CPUCOP440 | M_CPUCOP444);
  AddFixed("ING"  , 0x332a, M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("INH"  , 0x332b,                             M_CPUCOP440              );
  AddFixed("INL"  , 0x332e, M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("INR"  , 0x332d,                             M_CPUCOP440              );
  AddFixed("OBD"  , 0x333e, M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("OMG"  , 0x333a, M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("OMH"  , 0x333b,                             M_CPUCOP440              );
  AddFixed("XAS"  , 0x4f,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("ADT"  , 0x4a,                 M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CASC" , 0x10,                 M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("CQMA" , 0x332c,               M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("SKT"  , 0x41,                 M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);  
  AddFixed("XABR" , 0x12,                 M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);  
  AddFixed("ININ" , 0x3328,               M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("INIL" , 0x3329,               M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddFixed("OR"   , 0x331a,                             M_CPUCOP440              );
  AddFixed("LID"  , 0x3319,                             M_CPUCOP440              );
  AddFixed("XAN"  , 0x330b,                             M_CPUCOP440              );
  AddFixed("HALT" , 0x3338,                                           M_CPUCOP444);
  AddFixed("IT"   , 0x3339,                                           M_CPUCOP444);

  AddInstTable(InstTable, "SKGBZ", 0x33, DecodeSK);
  AddInstTable(InstTable, "SKMBZ", 0x00, DecodeSK);

  ImmOrders = (FixedOrder*)malloc(sizeof(FixedOrder) * ImmOrderCnt);
  InstrZ = 0;
  AddImm("STII" , 0x70,   M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddImm("LEI"  , 0x3360, M_CPUCOP410 | M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);
  AddImm("OGI"  , 0x3350,               M_CPUCOP420 | M_CPUCOP440 | M_CPUCOP444);                 

  AddInstTable(InstTable, "JMP"  , 0x60, DecodeJmp);
  AddInstTable(InstTable, "JSR"  , 0x68, DecodeJmp);

  AddInstTable(InstTable, "LD"   , 0x05, DecodeReg);
  AddInstTable(InstTable, "X"    , 0x06, DecodeReg);
  AddInstTable(InstTable, "XDS"  , 0x07, DecodeReg);
  AddInstTable(InstTable, "XIS"  , 0x04, DecodeReg);

  AddInstTable(InstTable, "AISC" , 0x00, DecodeAISC);
  AddInstTable(InstTable, "RMB"  , 0x00, DecodeRMB);
  AddInstTable(InstTable, "SMB"  , 0x00, DecodeSMB);

  AddInstTable(InstTable, "XAD"  , 0x00, DecodeXAD);

  AddInstTable(InstTable, "LBI"  , 0x00, DecodeLBI);  

  AddInstTable(InstTable, "LDD"  , 0x00, DecodeLDD);

  AddInstTable(InstTable, "JSRP"  , 0x00, DecodeJSRP);

  AddInstTable(InstTable, "JP"    , 0x00, DecodeJP);
}

static void DeinitFields(void)
{
  free(FixedOrders);
  free(ImmOrders);
  DestroyInstTable(InstTable);
}

/*---------------------------------------------------------------------------*/

static void MakeCode_COP4(void)
{
  Boolean BigFlag;

  CodeLen = 0; DontPrint = False;

  /* zu ignorierendes */

  if (*OpPart == '\0') return;

  /* pseudo instructions */

  if (DecodeNatPseudo(&BigFlag)) return;

  if (DecodeIntelPseudo(BigFlag)) return;

  /* machine instructions */

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200, OpPart);
}

static void SwitchFrom_COP4(void)
{
  DeinitFields();
}

static Boolean IsDef_COP4(void)
{
  return False;
}

static void SwitchTo_COP4(void)
{
  PFamilyDescr pDescr;

  pDescr = FindFamilyByName("COP4");

  TurnWords = False; ConstMode = ConstModeC; SetIsOccupied = False;

  PCSymbol = "."; HeaderID = pDescr->Id;
  NOPCode = 0x44;
  DivideChars = ","; HasAttrs = False;

  ValidSegs = (1 << SegCode);
  Grans[SegCode] = 1; ListGrans[SegCode] = 1; SegInits[SegCode] = 0;
  if (MomCPU >= CPUCOP440)
  {
    SegLimits[SegCode] = 0x7ff;
    AdrInt = UInt11;
  }
  else if (MomCPU >= CPUCOP420)
  {
    SegLimits[SegCode] = 0x3ff;
    AdrInt = UInt10;
  }
  else
  {
    SegLimits[SegCode] = 0x1ff;
    AdrInt = UInt9;
  }

  MakeCode = MakeCode_COP4; IsDef = IsDef_COP4;
  SwitchFrom = SwitchFrom_COP4; InitFields();
}

void codecop4_init(void)
{
  CPUCOP410 = AddCPU("COP410", SwitchTo_COP4);
  CPUCOP420 = AddCPU("COP420", SwitchTo_COP4);
  CPUCOP440 = AddCPU("COP440", SwitchTo_COP4);
  CPUCOP444 = AddCPU("COP444", SwitchTo_COP4);
}
