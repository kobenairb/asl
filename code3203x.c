/* code3203x.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator TMS320C3x-Familie                                           */               
/*                                                                           */
/* Historie: 12.12.1996 Grundsteinlegung                                     */               
/*            7. 7.1998 Fix Zugriffe auf CharTransTable wg. signed chars     */
/*           18. 8.1998 BookKeeping-Aufruf in RES                            */
/*            3. 1.1998 ChkPC-Anpassung                                      */
/*            9. 3.2000 'ambiguous else'-Warnungen beseitigt                 */
/*                                                                           */
/*****************************************************************************/
/* $Id: code3203x.c,v 1.23 2017/06/07 19:38:43 alfred Exp $                       */
/***************************************************************************** 
 * $Log: code3203x.c,v $
 * Revision 1.23  2017/06/07 19:38:43  alfred
 * - do not double-generate CALL and LAJ
 *
 * Revision 1.22  2016/10/21 20:05:56  alfred
 * - fix some bugs detected by pedantic GCC
 *
 * Revision 1.21  2016/09/12 17:31:41  alfred
 * - corrections for 16-bit compiler
 *
 * Revision 1.20  2016/08/27 07:31:51  alfred
 * - remove debug printf
 *
 * Revision 1.19  2016/08/27 07:12:25  alfred
 * - code format 0x76 now reflects C3x and C4x code
 *
 * Revision 1.18  2016/08/26 18:57:11  alfred
 * - add parallel versions of TOIEEE/FROMIEEE
 *
 * Revision 1.17  2016/08/26 18:43:13  alfred
 * - add new non-parallel C4x instructions
 *
 * Revision 1.16  2016/08/26 09:10:19  alfred
 * - add MPYSHI and MPYUHI
 *
 * Revision 1.15  2016/08/26 08:45:19  alfred
 * - regard different register names on C4x
 *
 * Revision 1.14  2016/08/25 21:45:50  alfred
 * - implement type 2 3-op generic format for C4x
 *
 * Revision 1.13  2016/08/25 20:56:22  alfred
 * - SrcxMode/Part designators consistent to syntax in TI manual (src2,src1,dst)
 *
 * Revision 1.12  2016/08/25 20:43:03  alfred
 * - C4x will be realized as extension of C3x target
 *
 * Revision 1.11  2016/08/24 12:13:19  alfred
 * - begun with 320C4x support
 *
 * Revision 1.10  2014/11/16 13:15:06  alfred
 * - remove some superfluous semicolons
 *
 * Revision 1.9  2014/11/05 15:47:14  alfred
 * - replace InitPass callchain with registry
 *
 * Revision 1.8  2014/11/02 14:43:30  alfred
 * - rework to current style
 *
 * Revision 1.7  2014/03/08 21:06:35  alfred
 * - rework ASSUME framework
 *
 * Revision 1.6  2010/04/17 13:14:19  alfred
 * - address overlapping strcpy()
 *
 * Revision 1.5  2008/11/23 10:39:16  alfred
 * - allow strings with NUL characters
 *
 * Revision 1.4  2007/11/24 22:48:03  alfred
 * - some NetBSD changes
 *
 * Revision 1.3  2005/10/02 10:00:44  alfred
 * - ConstLongInt gets default base, correct length check on KCPSM3 registers
 *
 * Revision 1.2  2005/09/08 17:31:03  alfred
 * - add missing include
 *
 * Revision 1.1  2003/11/06 02:49:19  alfred
 * - recreated
 *
 * Revision 1.4  2003/05/02 21:23:09  alfred
 * - strlen() updates
 *
 * Revision 1.3  2002/08/14 18:17:35  alfred
 * - warn about NULL allocation
 *
 * Revision 1.2  2002/07/14 18:39:58  alfred
 * - fixed TempAll-related warnings
 *
 *****************************************************************************/

#include "stdinc.h"
#include <ctype.h>
#include <string.h>

#include "nls.h"
#include "endian.h"
#include "bpemu.h"
#include "strutil.h"
#include "chunks.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmcode.h"
#include "asmitree.h"
#include "codepseudo.h"
#include "codevars.h"
#include "tipseudo.h"
#include "headids.h"
#include "errmsg.h"

#include "code3203x.h"

#define FixedOrderCount 3
#define GenOrderCount 73
#define ParOrderCount 8
#define SingOrderCount 2

typedef struct
{
  LongWord Code;
} FixedOrder;

typedef struct
{
  int NameLen;
  CPUVar MinCPU;
  Boolean May1, May3;
  Word Code, Code3;
  Boolean OnlyMem;
  Boolean SwapOps;
  Boolean ImmFloat;
  Boolean Commutative;
  Byte ParMask, Par3Mask;
  unsigned ParIndex, ParIndex3;
  Byte PCodes[8], P3Codes[8];
} GenOrder;

typedef struct
{
  LongWord Code;
  Byte Mask;
} SingOrder;

typedef struct
{
  const GenOrder *pOrder;
  Boolean Is3;
  ShortInt Src2Mode, Src1Mode, DestMode;
  Word Src2Part, Src1Part, DestPart;
} tGenOrderInfo;

static CPUVar CPU32030, CPU32031,
              CPU32040, CPU32044;

static Boolean NextPar, ThisPar;
static Byte PrevARs, ARs;
static char PrevOp[7];
static tGenOrderInfo PrevGenInfo;

static FixedOrder *FixedOrders;
static GenOrder *GenOrders;
static char **ParOrders;
static SingOrder *SingOrders;

static LongInt DPValue;

/*-------------------------------------------------------------------------*/
/* Adressparser */

/* do not change this enum, since it is defined the way as needed in the machine
   code G field! */

#define ModNone (-1)
#define ModReg 0
#define MModReg (1 << ModReg)
#define ModDir 1
#define MModDir (1 << ModDir)
#define ModInd 2
#define MModInd (1 << ModInd)
#define ModImm 3
#define MModImm (1 << ModImm)

static ShortInt AdrMode;
static LongInt AdrPart;

/* special registers with address 0x10... vary: */

#define ARxRegStart 0x08
#define CxxRegStart 0x10
static const char *C4XRegs[] =
{ "DP", "IR0", "IR1", "BK", "SP", "ST", "DIE", "IIE", "IIF", "RS", "RE", "RC", NULL },
                  *C3XRegs[] =
{ "DP", "IR0", "IR1", "BK", "SP", "ST", "IE" , "IF" , "IOF", "RS", "RE", "RC", NULL },
                  **CxxRegs;
static const char *ExpRegs[] =
{ "IVTP", "TVTP", NULL };

static Boolean Is4x(void)
{
  return MomCPU >= CPU32040;
}

static Boolean DecodeReg(const char *Asc, Byte *Erg)
{
  if ((mytoupper(*Asc) == 'R') && (strlen(Asc) <= 3) && (strlen(Asc) >= 2))
  {
    Boolean OK;

    *Erg = ConstLongInt(Asc + 1, &OK, 10);

    /* For C3x, tolerate R8...R27 as aliases for other registers for backward compatibility.
       For C4x, four new registers R8...R11 are mapped at register file address 28..31.
       So for C4x, only allow the defined R0..11 registers: */

    if (OK)
      OK = *Erg <= Is4x() ? 11 : 27;
    if (OK)
    {
      if (Is4x() && (*Erg >= 8))
        *Erg += 20; 
      return True;
    }
  }

  if ((strlen(Asc) == 3)
   && (mytoupper(*Asc) == 'A') && (mytoupper(Asc[1]) == 'R')
   && (Asc[2] >= '0') && (Asc[2] <= '7'))
  {
    *Erg = Asc[2] - '0' + ARxRegStart;
    return True;
  }

  for (*Erg = 0; CxxRegs[*Erg]; (*Erg)++)
    if (!strcasecmp(CxxRegs[*Erg], Asc))
    {
      *Erg += CxxRegStart;
      return True;
    }

  return False;
}

static Boolean DecodeExpReg(const char *Asc, Byte *Erg)
{
  for (*Erg = 0; ExpRegs[*Erg]; (*Erg)++)
    if (!strcasecmp(ExpRegs[*Erg], Asc))
      return True;

  return False;
}

static void DecodeAdr(char *Asc, Byte Erl, Boolean ImmFloat)
{
  Byte HReg;
  Integer Disp;
  char *p;
  int l;
  Double f;
  Word fi;
  LongInt AdrLong;
  Boolean BitRev, Circ;
  String NDisp;
  Boolean OK;
  enum
  {
    ModBase, ModAdd, ModSub, ModPreInc, ModPreDec, ModPostInc, ModPostDec
  } Mode;

  KillBlanks(Asc);

  AdrMode = ModNone;

  /* I. Register? */

  if (DecodeReg(Asc, &HReg))
  {
    AdrMode = ModReg; AdrPart = HReg;
    goto chk;
  }

  /* II. indirekt ? */

  if (*Asc == '*')
  {
    /* II.1. Erkennungszeichen entfernen */

    Asc++;

    /* II.2. Extrawuerste erledigen */

    BitRev = False;
    Circ = False;
    l = strlen(Asc);
    if ((l > 0) && (mytoupper(Asc[l - 1]) == 'B'))
    {
      BitRev = True;
      Asc[--l] = '\0';
    }
    else if ((l > 0) && (Asc[l - 1] == '%'))
    {
      Circ = True;
      Asc[--l] = '\0';
    }

    /* II.3. Displacement entfernen und auswerten:
            0..255-->Displacement
            -1,-2 -->IR0,IR1
            -3    -->Default */

    p = QuotPos(Asc, '(');
    if (p)
    {
      if (Asc[l - 1] != ')')
      {
        WrError(1350);
        return;
      }
      *p = '\0'; strmaxcpy(NDisp, p + 1, 255);
      NDisp[strlen(NDisp) - 1] = '\0';
      if (!strcasecmp(NDisp, "IR0"))
        Disp = -1;
      else if (!strcasecmp(NDisp, "IR1"))
        Disp = -2;
      else
      {
        Disp = EvalIntExpression(NDisp, UInt8, &OK);
        if (!OK)
          return;
      }
    }
    else Disp = -3;

    /* II.4. Addieren/Subtrahieren mit/ohne Update? */

    l = strlen(Asc);
    if (*Asc == '-')
    {
      if (Asc[1] == '-')
      {
        Mode = ModPreDec;
        Asc += 2;
      }
      else
      {
        Mode = ModSub;
        Asc++;
      }
    }
    else if (*Asc == '+')
    {
      if (Asc[1] == '+')
      {
        Mode = ModPreInc;
        Asc += 2;
      }
      else
      {
        Mode = ModAdd;
        Asc++;
      }
    }
    else if (Asc[l - 1] == '-')
    {
      if (Asc[l - 2] == '-')
      {
        Mode = ModPostDec;
        Asc[l - 2] = '\0';
      }
      else
      {
        WrError(1350);
        return;
      }
    }
    else if (Asc[l - 1] == '+')
    {
      if (Asc[l - 2] == '+')
      {
        Mode = ModPostInc;
        Asc[l - 2] = '\0';
      }
      else
      {
        WrError(1350);
        return;
      }
    }
    else
      Mode = ModBase;

    /* II.5. Rest muss Basisregister sein */

    if ((!DecodeReg(Asc, &HReg)) || (HReg < 8) || (HReg > 15))
    {
      WrError(1350);
      return;
    }
    HReg -= 8;
    if ((ARs & (1l << HReg)) == 0)
      ARs += 1l << HReg;
    else
      WrXError(210, Asc);

    /* II.6. Default-Displacement explizit machen */

    if (Disp == -3)
     Disp = (Mode == ModBase) ? 0 : 1;

    /* II.7. Entscheidungsbaum */

    switch (Mode)
    {
      case ModBase:
      case ModAdd:
        if ((Circ) || (BitRev)) WrError(1350);
        else
        {
          switch (Disp)
          {
            case -2: AdrPart = 0x8000; break;
            case -1: AdrPart = 0x4000; break;
            case  0: AdrPart = 0xc000; break;
            default: AdrPart = Disp;
          }
          AdrPart += ((Word)HReg) << 8;
          AdrMode = ModInd;
        }
        break;
      case ModSub:
        if ((Circ) || (BitRev)) WrError(1350);
        else
        {
          switch (Disp)
          {
            case -2: AdrPart = 0x8800; break;
            case -1: AdrPart = 0x4800; break;
            case  0: AdrPart = 0xc000; break;
            default: AdrPart = 0x0800 + Disp;
          }
          AdrPart += ((Word)HReg) << 8;
          AdrMode = ModInd;
        }
        break;
      case ModPreInc:
        if ((Circ) || (BitRev)) WrError(1350);
        else
        {
          switch (Disp)
          {
            case -2: AdrPart = 0x9000; break;
            case -1: AdrPart = 0x5000; break;
            default: AdrPart = 0x1000 + Disp;
          }
          AdrPart += ((Word)HReg) << 8;
          AdrMode = ModInd;
        }
        break;
      case ModPreDec:
        if ((Circ) || (BitRev)) WrError(1350);
        else
        {
          switch (Disp)
          {
            case -2: AdrPart = 0x9800; break;
            case -1: AdrPart = 0x5800; break;
            default: AdrPart = 0x1800 + Disp;
          }
          AdrPart += ((Word)HReg) << 8;
          AdrMode = ModInd;
        }
        break;
      case ModPostInc:
        if (BitRev)
        {
          if (Disp != -1) WrError(1350);
          else
          {
            AdrPart = 0xc800 + (((Word)HReg) << 8);
            AdrMode = ModInd;
          }
        }
        else
        {
          switch (Disp)
          {
            case -2: AdrPart = 0xa000; break;
            case -1: AdrPart = 0x6000; break;
            default: AdrPart = 0x2000 + Disp;
          }
          if (Circ)
            AdrPart += 0x1000;
          AdrPart += ((Word)HReg) << 8;
          AdrMode = ModInd;
        }
        break;
      case ModPostDec:
        if (BitRev) WrError(1350);
        else
        {
          switch (Disp)
          {
            case -2: AdrPart = 0xa800; break;
            case -1: AdrPart = 0x6800; break;
            default: AdrPart = 0x2800 + Disp; break;
          }
          if (Circ)
             AdrPart += 0x1000;
          AdrPart += ((Word)HReg) << 8;
          AdrMode = ModInd;
        }
        break;
    }

    goto chk;
  }

  /* III. absolut */

  if (*Asc == '@')
  {
    AdrLong = EvalIntExpression(Asc + 1, UInt24, &OK);
    if (OK)
    {
      if ((DPValue != -1) && ((AdrLong >> 16) != DPValue))
        WrError(110);
      AdrMode = ModDir;
      AdrPart = AdrLong & 0xffff;
    }
    goto chk;
  }

  /* IV. immediate */

  if (ImmFloat)
  {
    f = EvalFloatExpression(Asc, Float64, &OK);
    if ((OK) && (ExtToTIC34xShort(f, &fi)))
    {
      AdrPart = fi;
      AdrMode = ModImm;
    }
  }
  else
  {
    AdrPart = EvalIntExpression(Asc, Int16, &OK);
    if (OK)
    {
      AdrPart &= 0xffff;
      AdrMode = ModImm;
    }
  }

chk:
  if ((AdrMode != ModNone) && (!(Erl & (1 << AdrMode))))
  {
    AdrMode = ModNone;
    WrError(1350);
  }
}

static Word EffPart(Byte Mode, Word Part)
{
  switch (Mode)
  {
    case ModReg:
    case ModImm:
      return Lo(Part);
    case ModInd:
      return Hi(Part);
    default:
      WrError(10000);
      return 0;
  }
}

/*-------------------------------------------------------------------------*/
/* Code-Erzeugung */

static void JudgePar(const GenOrder *Prim, int Sec, Byte *ErgMode, Byte *ErgCode)
{
  if (Sec > 3)
    *ErgMode = 3;
  else if (Prim->May3)
    *ErgMode = 1;
  else
    *ErgMode = 2;
  *ErgCode = (*ErgMode == 2) ? Prim->PCodes[Sec] : Prim->P3Codes[Sec];
}

static LongWord EvalAdrExpression(char *Asc, Boolean *OK)
{
  if (*Asc == '@')
    Asc++;
  return EvalIntExpression(Asc, UInt24, OK);
}

static void SwapMode(ShortInt *M1, ShortInt *M2)
{
  AdrMode = (*M1);
  *M1 = (*M2);
  *M2 = AdrMode;
}

static void SwapPart(Word *P1, Word *P2)
{
  AdrPart = (*P1);
  *P1 = (*P2);
  *P2 = AdrPart;
}

static unsigned MatchParIndex(Byte Mask, unsigned Index)
{
  return (Mask & (1 << Index)) ? Index : ParOrderCount;
}

/*-------------------------------------------------------------------------*/
/* Instruction Decoders */

 /* ohne Argument */

static void DecodeFixed(Word Index)
{
  if (!ChkArgCnt(0, 0));
  else if (ThisPar) WrError(1950);
  else
  {
    DAsmCode[0] = FixedOrders[Index].Code;
    CodeLen = 1;
  }
  NextPar = False;
}

/* Arithmetik/Logik */

static Boolean Gen3IndirectAllowed(LongWord ThisAdrPart)
{
  /* indirect addressing mode fits into 3-op, non-par instruction if it needs no
     displacement, i.e. the 'displacement' is IR0/IR1/special, or it is one: */

  return ((ThisAdrPart & 0xc000) || (Lo(ThisAdrPart) == 1));
}

static Boolean Is4xArDisp(ShortInt ThisAdrMode, LongWord ThisAdrPart)
{
  return (ThisAdrMode == ModInd)
      && ((ThisAdrPart & 0xf800) == 0x0000)
      && (Lo(ThisAdrPart) != 1)  /* prefer C3x format if displacement=1 */
      && (Lo(ThisAdrPart) <= 31);
}

static void DecodeGen(Word Index)
{
  Byte HReg, HReg2, Sum;
  String Form;
  tGenOrderInfo CurrGenInfo;
  LongWord T21_22 = 0, T28 = 0;

  NextPar = False;

  CurrGenInfo.pOrder = GenOrders + (Index & ~0x8000);
  CurrGenInfo.Is3 = (Index & 0x8000) || FALSE;

  if (!ChkMinCPU(CurrGenInfo.pOrder->MinCPU))
    return;

  /* Argumentzahl abgleichen */

  if (ArgCnt == 1)
  {
    if (CurrGenInfo.pOrder->May1)
    {
      ArgCnt = 2;
      strcpy(ArgStr[2], ArgStr[1]);
    }
    else
    {
      (void)ChkArgCnt(2, 2);
      return;
    }
  }
  if ((ArgCnt == 3) && (!CurrGenInfo.Is3))
    CurrGenInfo.Is3 = True;

  if ((CurrGenInfo.pOrder->SwapOps) && (!CurrGenInfo.Is3))
  {
    strcpy(ArgStr[3], ArgStr[1]); 
    strcpy(ArgStr[1], ArgStr[2]);
    strcpy(ArgStr[2], ArgStr[3]);
  }
  if ((CurrGenInfo.Is3) && (ArgCnt == 2))
  {
    ArgCnt = 3;
    strcpy(ArgStr[3], ArgStr[2]);
  }
  if ((ArgCnt < 2) || (ArgCnt > 3) || ((CurrGenInfo.Is3) && (!CurrGenInfo.pOrder->May3)))
  {
    (void)ChkArgCnt(3, 3);
    return;
  }

  /* Argumente parsen */

  if (CurrGenInfo.Is3)
  {
    if (Memo("TSTB3"))
    {
      CurrGenInfo.DestMode = ModReg;
      CurrGenInfo.DestPart = 0;
    }
    else
    {
      DecodeAdr(ArgStr[3], MModReg, CurrGenInfo.pOrder->ImmFloat);
      if (AdrMode == ModNone)
        return;
      CurrGenInfo.DestMode = AdrMode;
      CurrGenInfo.DestPart = AdrPart;
    }

    /* The C4x type 2 format may use an immediate operand only if it is an
       integer operation - there is no 8-bit represenataion of floats. */

    DecodeAdr(ArgStr[1],
              MModReg | MModInd | ((Is4x() && !CurrGenInfo.pOrder->ImmFloat) ? MModImm : 0),
              CurrGenInfo.pOrder->ImmFloat);
    if (AdrMode == ModNone)
      return;

    /* src2 is immediate or *+ARn(udisp5): C4x type 2 format */

    if (AdrMode == ModImm)
    {
      T28 = 1ul << 28;
    }
    else if (Is4x() && Is4xArDisp(AdrMode, AdrPart))
    {
      /* note that for type 2, bit 21 defines addressing mode of src2 and bit 22
         defines addressing mode of src1, which is the opposite of the type 1 format! */

      T21_22 |= 1ul << 21;
      T28 = 1ul << 28;
      AdrPart = ((Word)(Hi(AdrPart) & 7) | (Lo(AdrPart) << 3)) << 8;
    }

    /* type 1/C3x format: check whether indirect mode is 'short': */

    else if (AdrMode == ModInd)
    {
      if (!Gen3IndirectAllowed(AdrPart))
      {
        WrError(1350);
        return;
      }
      T21_22 |= 1ul << 22;
    }
    CurrGenInfo.Src2Mode = AdrMode;
    CurrGenInfo.Src2Part = AdrPart;

    DecodeAdr(ArgStr[2], MModReg | MModInd, CurrGenInfo.pOrder->ImmFloat);
    if (AdrMode == ModNone)
      return;

    /* if type 2, the only indirect mode allowed for src1 is *+ARn(udisp5): */

    if (T28)
    {
      if (AdrMode == ModInd)
      {
        if (!Is4xArDisp(AdrMode, AdrPart))
        {
          WrError(1350);
          return;
        }
        else
          AdrPart = ((Word)(Hi(AdrPart) & 7) | (Lo(AdrPart) << 3)) << 8;
        T21_22 |= 1ul << 22;
      }
    }

    /* type 1/C3x format: similar check for src1 for 'short' adressing: */

    else if (AdrMode == ModInd)
    {
      if (!Gen3IndirectAllowed(AdrPart))
      {
        WrError(1350);
        return;
      }
      T21_22 |= 1ul << 21;
    }
    CurrGenInfo.Src1Mode = AdrMode;
    CurrGenInfo.Src1Part = AdrPart;
  }
  else /* !CurrGenInfo.Is3 */
  {
    DecodeAdr(ArgStr[1], MModDir + MModInd + ((CurrGenInfo.pOrder->OnlyMem) ? 0 : MModReg + MModImm), CurrGenInfo.pOrder->ImmFloat);
    if (AdrMode == ModNone)
      return;
    CurrGenInfo.Src2Mode = AdrMode;
    CurrGenInfo.Src2Part = AdrPart;
    DecodeAdr(ArgStr[2], MModReg + MModInd, CurrGenInfo.pOrder->ImmFloat);
    switch (AdrMode)
    {
      case ModReg:
        CurrGenInfo.DestMode = AdrMode;
        CurrGenInfo.DestPart = AdrPart;
        CurrGenInfo.Src1Mode = CurrGenInfo.Src2Mode;
        CurrGenInfo.Src1Part = CurrGenInfo.Src2Part;
        break;
      case ModInd:
        if (((strcmp(OpPart, "TSTB")) && (strcmp(OpPart, "CMPI")) && (strcmp(OpPart, "CMPF")))
        ||  ((CurrGenInfo.Src2Mode == ModDir) || (CurrGenInfo.Src2Mode == ModImm))
        ||  ((CurrGenInfo.Src2Mode == ModInd) && ((CurrGenInfo.Src2Part & 0xe000) == 0) && (Lo(CurrGenInfo.Src2Part) != 1))
        ||  (((AdrPart & 0xe000) == 0) && (Lo(AdrPart) != 1)))
        {
          WrError(1350);
          return;
        }
        else
        {
          CurrGenInfo.Is3 = True;
          CurrGenInfo.DestMode = ModReg;
          CurrGenInfo.DestPart = 0;
          CurrGenInfo.Src1Mode = AdrMode;
          CurrGenInfo.Src1Part = AdrPart;
        }
        break;
      case ModNone: 
        return;
    }
  }

  /* auswerten: parallel... */

  if (ThisPar)
  {
    unsigned ParIndex, ARIndex;

    if (!PrevGenInfo.pOrder)
    {
      WrError(1950);
      return;
    }

    /* in Standardreihenfolge suchen */

    ParIndex = MatchParIndex(PrevGenInfo.Is3 ? PrevGenInfo.pOrder->Par3Mask : PrevGenInfo.pOrder->ParMask,
                             CurrGenInfo.Is3 ? CurrGenInfo.pOrder->ParIndex3 : CurrGenInfo.pOrder->ParIndex);
    if (ParIndex < ParOrderCount)
      JudgePar(PrevGenInfo.pOrder, ParIndex, &HReg, &HReg2);

    /* in gedrehter Reihenfolge suchen */

    else
    {
      ParIndex = MatchParIndex(CurrGenInfo.Is3 ? CurrGenInfo.pOrder->Par3Mask : CurrGenInfo.pOrder->ParMask,
                               PrevGenInfo.Is3 ? PrevGenInfo.pOrder->ParIndex3 : PrevGenInfo.pOrder->ParIndex);
      if (ParIndex < ParOrderCount)
      {
        JudgePar(CurrGenInfo.pOrder, ParIndex, &HReg, &HReg2);
        SwapMode(&CurrGenInfo.DestMode, &PrevGenInfo.DestMode);
        SwapMode(&CurrGenInfo.Src2Mode, &PrevGenInfo.Src2Mode);
        SwapMode(&CurrGenInfo.Src1Mode, &PrevGenInfo.Src1Mode);
        SwapPart(&CurrGenInfo.DestPart, &PrevGenInfo.DestPart);
        SwapPart(&CurrGenInfo.Src2Part, &PrevGenInfo.Src2Part);
        SwapPart(&CurrGenInfo.Src1Part, &PrevGenInfo.Src1Part);
      }
      else
      {
        WrError(1950);
        return;
      }
    }

    /* mehrfache Registernutzung ? */

    for (ARIndex = 0; ARIndex < 8; ARIndex++)
      if (ARs & PrevARs & (1l << ARIndex))
      {
        sprintf(Form, "AR%d", ARIndex);
        WrXError(210, Form);
      }

    /* 3 Basisfaelle */

    switch (HReg)
    {
      case 1:
        if ((!strcmp(PrevOp, "LSH3")) || (!strcmp(PrevOp, "ASH3")) || (!strcmp(PrevOp, "SUBF3")) || (!strcmp(PrevOp, "SUBI3")))
        {
          SwapMode(&PrevGenInfo.Src2Mode, &PrevGenInfo.Src1Mode);
          SwapPart(&PrevGenInfo.Src2Part, &PrevGenInfo.Src1Part);
        }
        if ((PrevGenInfo.DestPart > 7) || (CurrGenInfo.DestPart > 7))
        {
          WrError(1445);
          return;
        }

        /* Bei Addition und Multiplikation Kommutativitaet nutzen */

        if  ((PrevGenInfo.Src1Mode == ModInd) && (PrevGenInfo.Src2Mode == ModReg) && (PrevGenInfo.pOrder->Commutative))
        {
          SwapMode(&PrevGenInfo.Src2Mode, &PrevGenInfo.Src1Mode);
          SwapPart(&PrevGenInfo.Src2Part, &PrevGenInfo.Src1Part);
        }
        if ((PrevGenInfo.Src1Mode != ModReg) || (PrevGenInfo.Src1Part > 7)
         || (PrevGenInfo.Src2Mode != ModInd) || (CurrGenInfo.Src2Mode != ModInd))
        {
          WrError(1355);
          return;
        }
        RetractWords(1);
        DAsmCode[0] = 0xc0000000 + (((LongWord)HReg2) << 25)
                    + (((LongWord)PrevGenInfo.DestPart) << 22)
                    + (((LongWord)PrevGenInfo.Src1Part) << 19)
                    + (((LongWord)CurrGenInfo.DestPart) << 16)
                    + (CurrGenInfo.Src2Part & 0xff00) + Hi(PrevGenInfo.Src2Part);
        CodeLen = 1;
        NextPar = False;
        break;
      case 2:
        if ((PrevGenInfo.DestPart > 7) || (CurrGenInfo.DestPart > 7))
        {
          WrError(1445);
          return;
        }
        if ((PrevGenInfo.Src2Mode != ModInd) || (CurrGenInfo.Src2Mode != ModInd))
        {
          WrError(1355);
          return;
        }
        RetractWords(1);
        DAsmCode[0] = 0xc0000000 + (((LongWord)HReg2) << 25)
                    + (((LongWord)PrevGenInfo.DestPart) << 22)
                    + (CurrGenInfo.Src2Part & 0xff00) + Hi(PrevGenInfo.Src2Part);
        if ((!strcmp(PrevOp, OpPart)) && (*OpPart == 'L'))
        {
          DAsmCode[0] += ((LongWord)CurrGenInfo.DestPart) << 19;
          if (PrevGenInfo.DestPart == CurrGenInfo.DestPart) WrError(140);
        }
        else
          DAsmCode[0] += ((LongWord)CurrGenInfo.DestPart) << 16;
        CodeLen = 1;
        NextPar = False;
        break;
      case 3:
        if ((PrevGenInfo.DestPart > 1) || (CurrGenInfo.DestPart<2) || (CurrGenInfo.DestPart > 3))
        {
          WrError(1445);
          return;
        }
        Sum = 0;
        if (PrevGenInfo.Src2Mode == ModInd) Sum++;
        if (PrevGenInfo.Src1Mode == ModInd) Sum++;
        if (CurrGenInfo.Src2Mode == ModInd) Sum++;
        if (CurrGenInfo.Src1Mode == ModInd) Sum++;
        if (Sum != 2)
        {
          WrError(1355);
          return;
        }
        RetractWords(1);
        DAsmCode[0] = 0x80000000 + (((LongWord)HReg2) << 26)
                    + (((LongWord)PrevGenInfo.DestPart & 1) << 23)
                    + (((LongWord)CurrGenInfo.DestPart & 1) << 22);
        CodeLen = 1;
        if (CurrGenInfo.Src1Mode == ModReg)
        {
          if (CurrGenInfo.Src2Mode == ModReg)
          {
            DAsmCode[0] += ((LongWord)0x00000000)
                         + (((LongWord)CurrGenInfo.Src1Part) << 19)
                         + (((LongWord)CurrGenInfo.Src2Part) << 16)
                         + (PrevGenInfo.Src1Part & 0xff00) + Hi(PrevGenInfo.Src2Part);
          }
          else
          {
            DAsmCode[0] += ((LongWord)0x03000000)
                         + (((LongWord)CurrGenInfo.Src1Part) << 16)
                         + Hi(CurrGenInfo.Src2Part);
            if (PrevGenInfo.Src2Mode == ModReg)
              DAsmCode[0] += (((LongWord)PrevGenInfo.Src2Part) << 19) + (PrevGenInfo.Src1Part & 0xff00);
            else
              DAsmCode[0] += (((LongWord)PrevGenInfo.Src1Part) << 19) + (PrevGenInfo.Src2Part & 0xff00);
          }
        }
        else
        {
          if (CurrGenInfo.Src2Mode == ModReg)
          {
            DAsmCode[0] += ((LongWord)0x01000000)
                         + (((LongWord)CurrGenInfo.Src2Part) << 16)
                         + Hi(CurrGenInfo.Src1Part);
            if (PrevGenInfo.Src2Mode == ModReg)
              DAsmCode[0] += (((LongWord)PrevGenInfo.Src2Part) << 19) + (PrevGenInfo.Src1Part & 0xff00);
            else
              DAsmCode[0] += (((LongWord)PrevGenInfo.Src1Part) << 19) + (PrevGenInfo.Src2Part & 0xff00);
          }
          else
          {
            DAsmCode[0] += ((LongWord)0x02000000)
                         + (((LongWord)PrevGenInfo.Src1Part) << 19)
                         + (((LongWord)PrevGenInfo.Src2Part) << 16)
                         + (CurrGenInfo.Src1Part & 0xff00) + Hi(CurrGenInfo.Src2Part);
          }
        }
        break;
    }
  }
  /* ...sequentiell */
  else
  {
    strcpy(PrevOp, OpPart);
    PrevARs = ARs;
    PrevGenInfo = CurrGenInfo;
    if (CurrGenInfo.Is3)
      DAsmCode[0] = 0x20000000 | T28 | (((LongWord)CurrGenInfo.pOrder->Code3) << 23) | T21_22
                  | (((LongWord)CurrGenInfo.DestPart) << 16)
                  | (EffPart(CurrGenInfo.Src1Mode, CurrGenInfo.Src1Part) << 8)
                  | EffPart(CurrGenInfo.Src2Mode, CurrGenInfo.Src2Part);
    else
      DAsmCode[0] = 0x00000000 | (((LongWord)CurrGenInfo.pOrder->Code) << 23)
                  | (((LongWord)CurrGenInfo.Src2Mode) << 21)
                  | CurrGenInfo.Src2Part
                  | (((LongWord)CurrGenInfo.DestPart) << 16);
    CodeLen = 1;
    NextPar = True;
  }
}

/* Due to the way it is executed in the instruction pipeline, LDA has some restrictions
   on its operands: only ARx, DP-SP allowed as destination, and source+dest reg cannot be same register */

static void DecodeLDA(Word Code)
{
  Byte HReg;

  if (!ChkArgCnt(2, 2)); 
  else if (!ChkMinCPU(CPU32040));
  else if (ThisPar) WrError(1950);
  else if (!DecodeReg(ArgStr[2], &HReg)) WrXError(1445, ArgStr[2]);
  else if ((HReg < 8) || (HReg > 20)) WrXError(1445, ArgStr[2]);
  else
  {
    Boolean RegClash;

    DecodeAdr(ArgStr[1], MModReg | MModInd | MModDir | MModImm, False);
    switch (AdrMode)
    {
      case ModDir:
      case ModImm:
        RegClash = False;
        break;
      case ModReg:
        RegClash = (AdrPart == HReg);
        break;
      case ModInd:
        if ((Hi(AdrPart) & 7) + ARxRegStart == HReg)
          RegClash = True;
        else if (((AdrPart & 0xc000) == 0x4000) && (HReg == 0x11))
          RegClash = True;
        else if (((AdrPart & 0xc000) == 0x8000) && (HReg == 0x12))
          RegClash = True;
        else if (((AdrPart & 0xf800) == 0xc800) && (HReg == 0x11))
          RegClash = True;
        else
          RegClash = False;
        break;
      default:
        return;
    }
    if (RegClash) WrError(1760);
    else
    {
      DAsmCode[0] = (((LongWord)Code) << 23)
                  | (((LongWord)AdrMode) << 21)
                  | (((LongWord)HReg) << 16)
                  | AdrPart;
      CodeLen = 1;
    }
  }
  NextPar = False;
}

static void DecodeRot(Word Code)
{
  Byte HReg;

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else if (!DecodeReg(ArgStr[1], &HReg)) WrError(1350);
  else
  {
    DAsmCode[0] = 0x11e00000 + (((LongWord)Code) << 23) + (((LongWord)HReg) << 16);
    CodeLen = 1;
  }
  NextPar = False;
}

static void DecodeStk(Word Code)
{
  Byte HReg;

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else if (!DecodeReg(ArgStr[1], &HReg)) WrError(1350);
  else
  {
    DAsmCode[0] = 0x0e200000 + (((LongWord)Code) << 23) + (((LongWord)HReg) << 16);
    CodeLen = 1;
  }
  NextPar = False;
}

/* Datentransfer */

static void DecodeLDIcc_LDFcc(Word Code)
{
  LongWord CondCode = Lo(Code), InstrCode = ((LongWord)Hi(Code)) << 24;
  Byte HReg;

  if (!ChkArgCnt(2, 2));
  else if (ThisPar) WrError(1950);
  else
  {
    DecodeAdr(ArgStr[2], MModReg, False);
    if (AdrMode != ModNone)
    {
      HReg = AdrPart;
      DecodeAdr(ArgStr[1], MModReg + MModDir + MModInd + MModImm, InstrCode == 0x40000000);
      if (AdrMode != ModNone)
      {
        DAsmCode[0] = InstrCode + (((LongWord)HReg) << 16)
                    + (CondCode << 23)
                    + (((LongWord)AdrMode) << 21) + AdrPart;
        CodeLen = 1;
      }
    }
    NextPar = False;
  }
}

/* Sonderfall NOP auch ohne Argumente */

static void DecodeNOP(Word Code)
{
  UNUSED(Code);

  if (ArgCnt == 0)
  {
    CodeLen = 1;
    DAsmCode[0] = NOPCode;
  }
  else if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else
  {
    DecodeAdr(ArgStr[1], 5, False);
    if (AdrMode != ModNone)
    {
      DAsmCode[0] = 0x0c800000 + (((LongWord)AdrMode) << 21) + AdrPart;
      CodeLen = 1;
    }
  }
  NextPar = False;
}

/* Sonderfaelle */

static void DecodeSing(Word Index)
{
  const SingOrder *pOrder = SingOrders + Index;

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else
  {
    DecodeAdr(ArgStr[1], pOrder->Mask, False);
    if (AdrMode != ModNone)
    {
      DAsmCode[0] = pOrder->Code + (((LongWord)AdrMode) << 21) + AdrPart;
      CodeLen = 1;
    }
  }
  NextPar = False;
}

static void DecodeLDP(Word Code)
{
  UNUSED(Code);

  if (!ChkArgCnt(1, 2));
  else if (ThisPar) WrError(1950);
  else if ((ArgCnt == 2) && (strcasecmp(ArgStr[2], "DP"))) WrError(1350);
  else
  {
    Boolean OK;
    LongInt AdrLong = EvalAdrExpression(ArgStr[1], &OK);

    if (OK)
    {
      DAsmCode[0] = 0x08700000 + (AdrLong >> 16);
      CodeLen = 1;
    }
  }
  NextPar = False;
}

static void DecodeLdExp(Word Code)
{
  Boolean Swapped = (Code & 1) || False;
  Byte Src, Dest;

  if (!ChkArgCnt(2, 2));
  else if (!ChkMinCPU(CPU32040));
  else if (ThisPar) WrError(1950);
  else if (!(Swapped ? DecodeReg(ArgStr[1], &Src) : DecodeExpReg(ArgStr[1], &Src))) WrXError(1445, ArgStr[1]);
  else if (!(Swapped ? DecodeExpReg(ArgStr[2], &Dest) : DecodeReg(ArgStr[2], &Dest))) WrXError(1445, ArgStr[2]);
  else
  {
    DAsmCode[0] = (((LongWord)Code) << 23)
                | (((LongWord)Dest) << 16)
                | (((LongWord)Src) << 0);
    CodeLen = 1;
  }
  NextPar = False;
}

static void DecodeRegImm(Word Code)
{
  Byte Dest;

  if (!ChkArgCnt(2, 2));
  else if (!ChkMinCPU(CPU32040));
  else if (ThisPar) WrError(1950);
  else if (!DecodeReg(ArgStr[2], &Dest)) WrXError(1445, ArgStr[2]);
  else
  {
    DecodeAdr(ArgStr[1], MModImm, False);
    if (AdrMode == ModImm)
    {
      DAsmCode[0] = (((LongWord)Code) << 23)
                  | (((LongWord)AdrMode) << 21)
                  | (((LongWord)Dest) << 16)
                  | AdrPart;
      CodeLen = 1;
    }
  }
  NextPar = False;
}

static void DecodeLDPK(Word Code)
{
  Byte Dest;

  if (!ChkArgCnt(1, 1));
  else if (!ChkMinCPU(CPU32040));
  else if (ThisPar) WrError(1950);
  else if (!DecodeReg("DP", &Dest)) WrXError(1445, "DP");
  else
  {
    DecodeAdr(ArgStr[1], MModImm, False);
    if (AdrMode == ModImm)
    {
      DAsmCode[0] = (((LongWord)Code) << 23)
                  | (((LongWord)AdrMode) << 21)
                  | (((LongWord)Dest) << 16)   
                  | AdrPart;
      CodeLen = 1;
    }
  }
  NextPar = False;
}

static void DecodeSTIK(Word Code)
{
  if (!ChkArgCnt(2, 2));
  else if (!ChkMinCPU(CPU32040));
  else if (ThisPar) WrError(1950);
  else
  {
    Boolean OK;
    LongInt Src = EvalIntExpression(ArgStr[1], SInt5, &OK);

    if (OK)
    {
      DecodeAdr(ArgStr[2], MModInd | MModDir, False);

      if (AdrMode != ModNone)
      {
        AdrMode = (AdrMode == ModInd) ? 3 : 0;
        Src = (Src << 16) & 0x001f0000ul;
        DAsmCode[0] = (((LongWord)Code) << 23)
                    | (((LongWord)AdrMode) << 21)
                    | Src
                    | AdrPart;
        CodeLen = 1;
      }
    }
  }
  NextPar = False;
}

/* Schleifen */

static void DecodeRPTB_C3x(Word Code)
{
  UNUSED(Code);

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else
  {
    Boolean OK;
    LongInt AdrLong = EvalAdrExpression(ArgStr[1], &OK);

    if (OK)
    {
      DAsmCode[0] = 0x64000000 + AdrLong;
      CodeLen = 1;
    }
  }
  NextPar = False;
}

static void DecodeRPTB_C4x(Word Code)
{
  Byte Reg;

  UNUSED(Code);

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else if (DecodeReg(ArgStr[1], &Reg))
  {
    DAsmCode[0] = 0x79000000 | Reg;
    CodeLen = 1;
  }
  else
  {
    Boolean OK;
    LongInt AdrLong = EvalAdrExpression(ArgStr[1], &OK) - (EProgCounter() + 1);

    if (OK)
    {
      if ((!SymbolQuestionable) && ((AdrLong > 0x7fffffl) || (AdrLong < -0x800000l))) WrError(1370);
      else
      {
        DAsmCode[0] = 0x64000000 + AdrLong;
        CodeLen = 1;
      }
    }
  }
  NextPar = False;
}

/* Spruenge */

/* note that BR/BRD/CALL/(LAJ) take an absolute 24-bit address on C3x,
   but a 24-bit displacement on C4x. So we use different decoders for
   C3x and C4x: */

static void DecodeBR_BRD_CALL_C3x(Word Code)
{
  Byte InstrCode = Lo(Code);

  if (!ChkArgCnt(1, 1));
  else if (InstrCode == 0x63) (void)ChkMinCPU(CPU32040); /* no LAJ on C3x */
  else if (ThisPar) WrError(1950);
  else
  {
    Boolean OK;   
    LongInt AdrLong = EvalAdrExpression(ArgStr[1], &OK);

    if (OK)
    {
      DAsmCode[0] = (((LongWord)Code) << 24) + AdrLong;
      CodeLen = 1;
    }
  }
  NextPar = False;
}

static void DecodeBR_BRD_CALL_LAJ_C4x(Word Code)
{
  Byte InstrCode = Lo(Code), Dist = Hi(Code);

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else
  {
    Boolean OK;   
    LongInt AdrLong = EvalAdrExpression(ArgStr[1], &OK) - (EProgCounter() + Dist);

    if (OK)
    {
      if ((!SymbolQuestionable) && ((AdrLong > 0x7fffffl) || (AdrLong < -0x800000l))) WrError(1370);
      else
      {
        DAsmCode[0] = (((LongWord)InstrCode) << 24) + (AdrLong & 0xffffff);
        CodeLen = 1;
      }
    }
  }
  NextPar = False;
}

static void DecodeBcc(Word Code)
{
  LongWord CondCode = Lo(Code) & 0x7f,
           DFlag = ((LongWord)Hi(Code)) << 21;
  LongInt Disp = DFlag ? 3 : 1;
  Byte HReg;

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else if ((Code & 0x80) && !ChkMinCPU(CPU32040));
  else if (DecodeReg(ArgStr[1], &HReg))
  {
    DAsmCode[0] = 0x68000000 + (CondCode << 16) + DFlag + HReg;
    CodeLen = 1;
  }
  else
  {
    Boolean OK;
    LongInt AdrLong = EvalAdrExpression(ArgStr[1], &OK) - (EProgCounter() + Disp);

    if (OK)
    {
      if ((!SymbolQuestionable) && ((AdrLong > 0x7fffl) || (AdrLong < -0x8000l))) WrError(1370);
      else
      {
        DAsmCode[0] = 0x6a000000 + (CondCode << 16) + DFlag + (AdrLong & 0xffff);
        CodeLen = 1;
      }
    }
  }
  NextPar = False;
}

static void DecodeCALLcc(Word Code)
{
  Byte HReg;

  if (!ChkArgCnt(1, 1));
  else if (ThisPar) WrError(1950);
  else if (DecodeReg(ArgStr[1], &HReg))
  {
    DAsmCode[0] = 0x70000000 + (((LongWord)Code) << 16) + HReg;
    CodeLen = 1;
  }
  else
  {
    Boolean OK;
    LongInt AdrLong = EvalAdrExpression(ArgStr[1], &OK)-(EProgCounter() + 1);
    if (OK)
    {
      if ((!SymbolQuestionable) && ((AdrLong > 0x7fffl) || (AdrLong < -0x8000l))) WrError(1370);
      else
      {
        DAsmCode[0] = 0x72000000 + (((LongWord)Code) << 16) + (AdrLong & 0xffff);
        CodeLen = 1;
      }
    }
  }
  NextPar = False;
}

static void DecodeDBcc(Word Code)
{
  LongWord CondCode = Lo(Code),
           DFlag = ((LongWord)Hi(Code)) << 21;
  LongInt Disp = DFlag ? 3 : 1;  
  Byte HReg, HReg2;

  if (!ChkArgCnt(2, 2));
  else if (ThisPar) WrError(1950);
  else if (!DecodeReg(ArgStr[1], &HReg2)) WrError(1350);
  else if ((HReg2 < 8) || (HReg2 > 15)) WrError(1350);
  else
  {
    HReg2 -= 8;
    if (DecodeReg(ArgStr[2], &HReg))
    {
      DAsmCode[0] = 0x6c000000
                  + (CondCode << 16)
                  + DFlag
                  + (((LongWord)HReg2) << 22)
                  + HReg;
      CodeLen = 1;
    }
    else
    {
    Boolean OK;   
    LongInt AdrLong = EvalAdrExpression(ArgStr[2], &OK) - (EProgCounter() + Disp);
      if (OK)
      {
        if ((!SymbolQuestionable) && ((AdrLong > 0x7fffl) || (AdrLong < -0x8000l))) WrError(1370);
        else
        {
          DAsmCode[0] = 0x6e000000
                      + (CondCode << 16)
                      + DFlag
                      + (((LongWord)HReg2) << 22)
                      + (AdrLong & 0xffff);
          CodeLen = 1;
        }
      }
    }
  }
  NextPar = False;
}

static void DecodeRETIcc_RETScc(Word Code)
{
  LongWord CondCode = Lo(Code),
           DFlag = ((LongWord)Hi(Code)) << 21;

  if (!ChkArgCnt(0, 0));
  else if ((DFlag & (1ul << 21)) && !ChkMinCPU(CPU32040));
  else if (ThisPar) WrError(1950);
  else
  {
    DAsmCode[0] = 0x78000000 + DFlag + (CondCode << 16);
    CodeLen = 1;
  }
  NextPar = False;
}

static void DecodeTRAPcc(Word Code)
{
  if (!ChkArgCnt(1, 1));
  else if ((Code & 0x80) && !ChkMinCPU(CPU32040));
  else if (ThisPar) WrError(1950);
  else
  {
    Boolean OK;
    LongWord HReg = EvalIntExpression(ArgStr[1], Is4x() ? UInt9 : UInt4, &OK);

    if (OK)
    {
      DAsmCode[0] = 0x74000000 + HReg + (((LongWord)Code) << 16);
      CodeLen = 1;
    }
  }
  NextPar = False;
}

/*-------------------------------------------------------------------------*/
/* Befehlstabellenverwaltung */

static void AddCondition(char *NName, Byte NCode)
{
  char InstName[30];

  if (*NName)
  {
    sprintf(InstName, "LDI%s", NName);
    AddInstTable(InstTable, InstName, 0x5000 | NCode, DecodeLDIcc_LDFcc);
    sprintf(InstName, "LDF%s", NName);
    AddInstTable(InstTable, InstName, 0x4000 | NCode, DecodeLDIcc_LDFcc);
  }
  sprintf(InstName, "B%s", NName);
  AddInstTable(InstTable, InstName, 0x0000 | NCode, DecodeBcc);
  sprintf(InstName, "B%sD", NName);
  AddInstTable(InstTable, InstName, 0x0100 | NCode, DecodeBcc);
  sprintf(InstName, "B%sAF", NName);
  AddInstTable(InstTable, InstName, 0x0580 | NCode, DecodeBcc);
  sprintf(InstName, "B%sAT", NName);
  AddInstTable(InstTable, InstName, 0x0380 | NCode, DecodeBcc);
  if (*NName)
  {
    sprintf(InstName, "LAJ%s", NName);
    AddInstTable(InstTable, InstName, 0x4180 | NCode, DecodeBcc);
    sprintf(InstName, "CALL%s", NName);
    AddInstTable(InstTable, InstName, NCode, DecodeCALLcc);
  }
  sprintf(InstName, "DB%s", NName);
  AddInstTable(InstTable, InstName, NCode, DecodeDBcc);
  sprintf(InstName, "DB%sD", NName);
  AddInstTable(InstTable, InstName, 0x100 | NCode, DecodeDBcc);
  sprintf(InstName, "RETI%s", NName);
  AddInstTable(InstTable, InstName, NCode, DecodeRETIcc_RETScc);
  sprintf(InstName, "RETI%sD", NName);
  AddInstTable(InstTable, InstName, 0x100 | NCode, DecodeRETIcc_RETScc);
  sprintf(InstName, "RETS%s", NName);
  AddInstTable(InstTable, InstName, 0x400 | NCode, DecodeRETIcc_RETScc);
  sprintf(InstName, "TRAP%s", NName);
  AddInstTable(InstTable, InstName, NCode, DecodeTRAPcc);
  sprintf(InstName, "LAT%s", NName);
  AddInstTable(InstTable, InstName, 0x80 | NCode, DecodeTRAPcc);
}

static void AddFixed(char *NName, LongWord NCode)
{
  if (InstrZ >= FixedOrderCount) exit(255);
  FixedOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ++, DecodeFixed);
}

static void AddSing(char *NName, LongWord NCode, Byte NMask)
{
  if (InstrZ >= SingOrderCount) exit(255);
  SingOrders[InstrZ].Code = NCode;
  SingOrders[InstrZ].Mask = NMask;
  AddInstTable(InstTable, NName, InstrZ++, DecodeSing);
}

static void AddGen(char *NName, CPUVar NMin, Boolean NMay1, Boolean NMay3,
                   Word NCode, Word NCode3,
                   Boolean NOnly, Boolean NSwap, Boolean NImm, Boolean NComm,
                   Byte NMask1, Byte NMask3,
                   Byte C20, Byte C21, Byte C22, Byte C23, Byte C24,
                   Byte C25, Byte C26, Byte C27, Byte C30, Byte C31,
                   Byte C32, Byte C33, Byte C34, Byte C35, Byte C36,
                   Byte C37)
{
  char NName3[30];
  unsigned z;

  if (InstrZ >= GenOrderCount) exit(255);

  sprintf(NName3, "%s3", NName);

  GenOrders[InstrZ].ParIndex =
  GenOrders[InstrZ].ParIndex3 = ParOrderCount;
  for (z = 0; z < ParOrderCount; z++)
  {
    if (!strcmp(ParOrders[z], NName))
      GenOrders[InstrZ].ParIndex = z;
    if (!strcmp(ParOrders[z], NName3))
      GenOrders[InstrZ].ParIndex3 = z;
  }

  GenOrders[InstrZ].NameLen = strlen(NName);
  GenOrders[InstrZ].MinCPU = NMin;
  GenOrders[InstrZ].May1 = NMay1; GenOrders[InstrZ].May3 = NMay3;
  GenOrders[InstrZ].Code = NCode; GenOrders[InstrZ].Code3 = NCode3;
  GenOrders[InstrZ].OnlyMem = NOnly; GenOrders[InstrZ].SwapOps = NSwap;
  GenOrders[InstrZ].ImmFloat = NImm;
  GenOrders[InstrZ].Commutative = NComm;
  GenOrders[InstrZ].ParMask = NMask1; GenOrders[InstrZ].Par3Mask = NMask3;
  GenOrders[InstrZ].PCodes[0] = C20;  GenOrders[InstrZ].PCodes[1] = C21;
  GenOrders[InstrZ].PCodes[2] = C22;  GenOrders[InstrZ].PCodes[3] = C23;
  GenOrders[InstrZ].PCodes[4] = C24;  GenOrders[InstrZ].PCodes[5] = C25;
  GenOrders[InstrZ].PCodes[6] = C26;  GenOrders[InstrZ].PCodes[7] = C27;
  GenOrders[InstrZ].P3Codes[0] = C30; GenOrders[InstrZ].P3Codes[1] = C31;
  GenOrders[InstrZ].P3Codes[2] = C32; GenOrders[InstrZ].P3Codes[3] = C33;
  GenOrders[InstrZ].P3Codes[4] = C34; GenOrders[InstrZ].P3Codes[5] = C35;
  GenOrders[InstrZ].P3Codes[6] = C36; GenOrders[InstrZ].P3Codes[7] = C37;

  AddInstTable(InstTable, NName, InstrZ, DecodeGen);
  AddInstTable(InstTable, NName3, InstrZ | 0x8000, DecodeGen);
  InstrZ++;
}

static void InitFields(void)
{
  InstTable = CreateInstTable(607);
  SetDynamicInstTable(InstTable);

  AddInstTable(InstTable, "NOP", 0, DecodeNOP);
  AddInstTable(InstTable, "LDP", 0, DecodeLDP);
  AddInstTable(InstTable, "RPTB", 0, Is4x() ? DecodeRPTB_C4x : DecodeRPTB_C3x);
  AddInstTable(InstTable, "BR"  , 0x0160, Is4x() ? DecodeBR_BRD_CALL_LAJ_C4x : DecodeBR_BRD_CALL_C3x);
  AddInstTable(InstTable, "BRD" , 0x0361, Is4x() ? DecodeBR_BRD_CALL_LAJ_C4x : DecodeBR_BRD_CALL_C3x);
  AddInstTable(InstTable, "CALL", 0x0162, Is4x() ? DecodeBR_BRD_CALL_LAJ_C4x : DecodeBR_BRD_CALL_C3x);
  AddInstTable(InstTable, "LAJ" , 0x0363, Is4x() ? DecodeBR_BRD_CALL_LAJ_C4x : DecodeBR_BRD_CALL_C3x);
  AddTI34xPseudo(InstTable);

  AddCondition("U"  , 0x00); AddCondition("LO" , 0x01);
  AddCondition("LS" , 0x02); AddCondition("HI" , 0x03);
  AddCondition("HS" , 0x04); AddCondition("EQ" , 0x05);
  AddCondition("NE" , 0x06); AddCondition("LT" , 0x07);
  AddCondition("LE" , 0x08); AddCondition("GT" , 0x09);
  AddCondition("GE" , 0x0a); AddCondition("Z"  , 0x05);
  AddCondition("NZ" , 0x06); AddCondition("P"  , 0x09);
  AddCondition("N"  , 0x07); AddCondition("NN" , 0x0a);
  AddCondition("NV" , 0x0c); AddCondition("V"  , 0x0d);
  AddCondition("NUF", 0x0e); AddCondition("UF" , 0x0f);
  AddCondition("NC" , 0x04); AddCondition("C"  , 0x01);
  AddCondition("NLV", 0x10); AddCondition("LV" , 0x11);
  AddCondition("NLUF", 0x12);AddCondition("LUF", 0x13);
  AddCondition("ZUF", 0x14); AddCondition(""   , 0x00);

  FixedOrders = (FixedOrder *) malloc(sizeof(FixedOrder) * FixedOrderCount); InstrZ = 0;
  AddFixed("IDLE", 0x06000000); AddFixed("SIGI", 0x16000000);
  AddFixed("SWI" , 0x66000000);

  InstrZ = 0;
  AddInstTable(InstTable, "ROL" , InstrZ++, DecodeRot);
  AddInstTable(InstTable, "ROLC", InstrZ++, DecodeRot);
  AddInstTable(InstTable, "ROR" , InstrZ++, DecodeRot);
  AddInstTable(InstTable, "RORC", InstrZ++, DecodeRot);

  InstrZ = 0;
  AddInstTable(InstTable, "POP"  , InstrZ++, DecodeStk);
  AddInstTable(InstTable, "POPF" , InstrZ++, DecodeStk);
  AddInstTable(InstTable, "PUSH" , InstrZ++, DecodeStk);
  AddInstTable(InstTable, "PUSHF", InstrZ++, DecodeStk);

  AddInstTable(InstTable, "LDEP", 0x00ec, DecodeLdExp);
  AddInstTable(InstTable, "LDPE", 0x00ed, DecodeLdExp);
  AddInstTable(InstTable, "LDHI", 0x007f, DecodeRegImm);
  AddInstTable(InstTable, "LDPK", 0x003e, DecodeLDPK);
  AddInstTable(InstTable, "STIK", 0x002a, DecodeSTIK);

  ParOrders = (char **) malloc(sizeof(char *) * ParOrderCount); InstrZ = 0;
  ParOrders[InstrZ++] = "LDF";   ParOrders[InstrZ++] = "LDI";
  ParOrders[InstrZ++] = "STF";   ParOrders[InstrZ++] = "STI";
  ParOrders[InstrZ++] = "ADDF3"; ParOrders[InstrZ++] = "SUBF3";
  ParOrders[InstrZ++] = "ADDI3"; ParOrders[InstrZ++] = "SUBI3";

  GenOrders = (GenOrder *) malloc(sizeof(GenOrder) * GenOrderCount); InstrZ = 0;
/*        Name      MinCPU    May1   May3   Cd    Cd3   OnlyM  Swap   ImmF   Comm   PM1 PM3     */
  AddGen("ABSF"   , CPU32030, True , False, 0x00, 0xff, False, False, True , False, 4, 0,
         0xff, 0xff, 0x04, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("ABSI"   , CPU32030, True , False, 0x01, 0xff, False, False, False, False, 8, 0,
         0xff, 0xff, 0xff, 0x05, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("ADDC"   , CPU32030, False, True , 0x02, 0x00, False, False, False, True,  0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("ADDF"   , CPU32030, False, True , 0x03, 0x01, False, False, True , True,  0, 4,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0x06, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("ADDI"   , CPU32030, False, True , 0x04, 0x02, False, False, False, True,  0, 8,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x07, 0xff, 0xff, 0xff, 0xff);
  AddGen("AND"    , CPU32030, False, True , 0x05, 0x03, False, False, False, True,  0, 8,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x08, 0xff, 0xff, 0xff, 0xff);
  AddGen("ANDN"   , CPU32030, False, True , 0x06, 0x04, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("ASH"    , CPU32030, False, True , 0x07, 0x05, False, False, False, False, 0, 8,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x09, 0xff, 0xff, 0xff, 0xff);
  AddGen("CMPF"   , CPU32030, False, True , 0x08, 0x06, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("CMPI"   , CPU32030, False, True , 0x09, 0x07, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("FIX"    , CPU32030, True , False, 0x0a, 0xff, False, False, True , False, 8, 0,
         0xff, 0xff, 0xff, 0x0a, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("FLOAT"  , CPU32030, True , False, 0x0b, 0xff, False, False, False, False, 4, 0,
         0xff, 0xff, 0x0b, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("FRIEEE" , CPU32040, False, False, 0x38, 0xff, True , False, True , False, 4, 0,
         0xff, 0xff, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LB0"    , CPU32040, False, False,0x160, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LB1"    , CPU32040, False, False,0x161, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LB2"    , CPU32040, False, False,0x162, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LB3"    , CPU32040, False, False,0x163, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LBU0"   , CPU32040, False, False,0x164, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LBU1"   , CPU32040, False, False,0x165, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LBU2"   , CPU32040, False, False,0x166, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LBU3"   , CPU32040, False, False,0x167, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LH0"    , CPU32040, False, False,0x174, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LH1"    , CPU32040, False, False,0x175, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LHU0"   , CPU32040, False, False,0x176, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LHU1"   , CPU32040, False, False,0x177, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LDE"    , CPU32030, False, False, 0x0d, 0xff, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LDF"    , CPU32030, False, False, 0x0e, 0xff, False, False, True , False, 5, 0,
         0x02, 0xff, 0x0c, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LDFI"   , CPU32030, False, False, 0x0f, 0xff, True , False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LDI"    , CPU32030, False, False, 0x10, 0xff, False, False, False, False, 10, 0,
         0xff, 0x03, 0xff, 0x0d, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LDII"   , CPU32030, False, False, 0x11, 0xff, True , False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LDM"    , CPU32030, False, False, 0x12, 0xff, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("LSH"    , CPU32030, False, True , 0x13, 0x08, False, False, False, False, 0, 8,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWL0"   , CPU32040, False, False,0x168, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWL1"   , CPU32040, False, False,0x169, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWL2"   , CPU32040, False, False,0x16a, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWL3"   , CPU32040, False, False,0x16b, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWR0"   , CPU32040, False, False,0x16c, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWR1"   , CPU32040, False, False,0x16d, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWR2"   , CPU32040, False, False,0x16e, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("LWR3"   , CPU32040, False, False,0x16f, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("MB0"    , CPU32040, False, False,0x170, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("MB1"    , CPU32040, False, False,0x171, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("MB2"    , CPU32040, False, False,0x172, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("MB3"    , CPU32040, False, False,0x173, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("MH0"    , CPU32040, False, False,0x178, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("MH1"    , CPU32040, False, False,0x179, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff);
  AddGen("MPYF"   , CPU32030, False, True , 0x14, 0x09, False, False, True , True,  0,52,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0x0f, 0xff, 0x00, 0x01, 0xff, 0xff);
  AddGen("MPYI"   , CPU32030, False, True , 0x15, 0x0a, False, False, False, True,  0,200,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x10, 0xff, 0xff, 0x02, 0x03);
  AddGen("MPYSHI" , CPU32040, False, True , 0x3b, 0x11, False, False, False, True,  0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("MPYUHI" , CPU32040, False, True , 0x3c, 0x12, False, False, False, True,  0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("NEGB"   , CPU32030, True , False, 0x16, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("NEGF"   , CPU32030, True , False, 0x17, 0xff, False, False, True , False, 4, 0,
         0xff, 0xff, 0x11, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("NEGI"   , CPU32030, True , False, 0x18, 0xff, False, False, False, False, 8, 0,
         0xff, 0xff, 0xff, 0x12, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("NORM"   , CPU32030, True , False, 0x1a, 0xff, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("NOT"    , CPU32030, True , False, 0x1b, 0xff, False, False, False, False, 8, 0,
         0xff, 0xff, 0xff, 0x13, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("OR"     , CPU32030, False, True , 0x20, 0x0b, False, False, False, True,  0, 8,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x14, 0xff, 0xff, 0xff, 0xff);
  AddGen("RCPF"   , CPU32040, False, False, 0x3a, 0xff, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x14, 0xff, 0xff, 0xff, 0xff);
  AddGen("RND"    , CPU32030, True , False, 0x22, 0xff, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("RSQRF"  , CPU32040, False, False, 0x39, 0xff, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x14, 0xff, 0xff, 0xff, 0xff);
  AddGen("STF"    , CPU32030, False, False, 0x28, 0xff, True , True , True , False, 4, 0,
         0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("STFI"   , CPU32030, False, False, 0x29, 0xff, True , True , True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("STI"    , CPU32030, False, False, 0x2a, 0xff, True , True , False, False, 8, 0,
         0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("STII"   , CPU32030, False, False, 0x2b, 0xff, True , True , False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("SUBB"   , CPU32030, False, True , 0x2d, 0x0c, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("SUBC"   , CPU32030, False, False, 0x2e, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("SUBF"   , CPU32030, False, True , 0x2f, 0x0d, False, False, True , False, 0, 4,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0x15, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("SUBI"   , CPU32030, False, True , 0x30, 0x0e, False, False, False, False, 0, 8,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x16, 0xff, 0xff, 0xff, 0xff);
  AddGen("SUBRB"  , CPU32030, False, False, 0x31, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("SUBRF"  , CPU32030, False, False, 0x32, 0xff, False, False, True , False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("SUBRI"  , CPU32030, False, False, 0x33, 0xff, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("TOIEEE" , CPU32040, False, False, 0x37, 0xff, False, False, True , False, 4, 0,
         0xff, 0xff, 0x18, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("TSTB"   , CPU32030, False, True , 0x34, 0x0f, False, False, False, False, 0, 0,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  AddGen("XOR"    , CPU32030, False, True , 0x35, 0x10, False, False, False, True,  0, 8,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0x17, 0xff, 0xff, 0xff, 0xff);

  SingOrders = (SingOrder *) malloc(sizeof(SingOrder) * SingOrderCount); InstrZ = 0;
  AddSing("IACK", 0x1b000000, 6);
  AddSing("RPTS", 0x139b0000, 15);

  AddInstTable(InstTable, "LDA", 0x03d, DecodeLDA);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);

  free(FixedOrders);
  free(GenOrders);
  free(ParOrders);
  free(SingOrders);
}

static void MakeCode_3203X(void)
{
  CodeLen = 0;
  DontPrint = False;

  ThisPar = (!strcmp(LabPart, "||"));
  if ((strlen(OpPart) > 2) && (!strncmp(OpPart, "||", 2)))
  {
    ThisPar = True;
    strmov(OpPart, OpPart + 2);
  }
  if ((!NextPar) && (ThisPar))
  {
    WrError(1950);
    return;
  }
  ARs = 0;

  /* zu ignorierendes */

  if (Memo(""))
    return;

  if (!LookupInstTable(InstTable, OpPart))
  {
    WrXError(1200, OpPart);
    NextPar = False;
  }
}

static void InitCode_3203x(void)
{
  DPValue = 0;
}

static Boolean IsDef_3203X(void)
{
  return (!strcmp(LabPart, "||"));
}

static void SwitchFrom_3203X(void)
{
  DeinitFields();
}

static void SwitchTo_3203X(void)
{
#define ASSUME3203Count sizeof(ASSUME3203s) / sizeof(*ASSUME3203s)
  static ASSUMERec ASSUME3203s[] =
  {
    { "DP", &DPValue, -1, 0xff, 0x100 }
  };
  const TFamilyDescr *pDescr;

  pDescr = FindFamilyByName("TMS320C3x/C4x");

  TurnWords = False;
  ConstMode = ConstModeIntel;
  SetIsOccupied = False;

  PCSymbol = "$";
  HeaderID = pDescr->Id;
  NOPCode = 0x0c800000;
  DivideChars = ",";
  HasAttrs = False;

  ValidSegs = 1 << SegCode;
  Grans[SegCode] = 4; ListGrans[SegCode] = 4; SegInits[SegCode] = 0;
  if (MomCPU == CPU32040)
  {
    SegLimits[SegCode] = 0xfffffffful;
    CxxRegs = C4XRegs;
  }
  else if (MomCPU == CPU32044)
  {
    SegLimits[SegCode] = 0x1ffffful;
    CxxRegs = C4XRegs;
  }
  else /* C3x */
  {
    SegLimits[SegCode] = 0xfffffful;
    CxxRegs = C3XRegs;
  }

  pASSUMERecs = ASSUME3203s;
  ASSUMERecCnt = ASSUME3203Count;

  MakeCode = MakeCode_3203X;
  IsDef = IsDef_3203X;
  SwitchFrom = SwitchFrom_3203X;
  InitFields();
  NextPar = False;
}

void code3203x_init(void)
{
  CPU32030 = AddCPU("320C30", SwitchTo_3203X);
  CPU32031 = AddCPU("320C31", SwitchTo_3203X);
  CPU32040 = AddCPU("320C40", SwitchTo_3203X);
  CPU32044 = AddCPU("320C44", SwitchTo_3203X);

  AddInitPassProc(InitCode_3203x);
}
