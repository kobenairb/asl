/* code53c8xx.c */
/*****************************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only                     */
/*                                                                           */
/* Makroassembler AS                                                         */
/*                                                                           */
/* Codegenerator SYM53C8xx                                                   */
/*                                                                           */
/*****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmitree.h"
#include "headids.h"
#include "strutil.h"
#include "codevars.h"
#include "intconsts.h"
#include "intpseudo.h"
#include "errmsg.h"

#include "code53c8xx.h"

/*---------------------------------------------------------------------------*/

#define RegCnt 69

typedef struct
{
  char *Name;
  LongWord Code;
  Word Mask;
} TReg, *PReg;

static CPUVar CPU53C810, CPU53C860, CPU53C815, CPU53C825, CPU53C875,
              CPU53C895;

#define M_53C810 0x0001
#define M_53C860 0x0002
#define M_53C815 0x0004
#define M_53C825 0x0008
#define M_53C875 0x0010
#define M_53C895 0x0020

static PReg Regs;

/*---------------------------------------------------------------------------*/

static Boolean IsInToken(char Inp)
{
  return ((Inp == '_') || (isalnum(((usint) Inp) & 0xff)));
}

static void GetToken(tStrComp *pSrc, tStrComp *pDest)
{
  char *p;

  /* search token start, by skipping spaces */

  for (p = pSrc->Str; myisspace(*p); p++)
    if (*p == '\0') break;
  StrCompCutLeft(pSrc, p - pSrc->Str);
  if (*p == '\0')
  {
    StrCompReset(pDest);
    return;
  }
  pDest->Pos.StartCol = pSrc->Pos.StartCol;

  /* geklammerter Ausdruck ? */

  if (*pSrc->Str == '(')
  {
    p = QuotPos(pSrc->Str, ')');

    /* no closing ) -> copy all up to end */

    if (!p)
    {
      StrCompCopy(pDest, pSrc);
      StrCompCutLeft(pSrc, strlen(pSrc->Str));
    }

    /* otherwise, copy (...) */

    else
    {
      pDest->Pos.Len = strmemcpy(pDest->Str, STRINGSIZE, pSrc->Str, p + 1 - pSrc->Str);
      StrCompCutLeft(pSrc, p + 1 - pSrc->Str);
    }
  }

  /* Spezialtoken ? */

  else if (!IsInToken(*pSrc->Str))
  {
    pDest->Str[0] = *pSrc->Str;
    pDest->Str[1] = '\0';
    pDest->Pos.Len = 1;
    StrCompCutLeft(pSrc, 1);
  }

  /* Wort ? */

  else
  {
    for (; IsInToken(*p); p++)
      if (*p == '\0')
        break;
    pDest->Pos.Len = strmemcpy(pDest->Str, STRINGSIZE, pSrc->Str, p - pSrc->Str);
    StrCompCutLeft(pSrc, p - pSrc->Str);
  }
}


static Boolean DecodePhase(char *Name, LongWord *Result)
{
  *Result = 8;
  if (!strcasecmp(Name, "DATA_OUT")) *Result = 0;
  else if (!strcasecmp(Name, "DATA_IN")) *Result = 1;
  else if (!strcasecmp(Name, "CMD")) *Result = 2;
  else if (!strcasecmp(Name, "COMMAND")) *Result = 2;
  else if (!strcasecmp(Name, "STATUS")) *Result = 3;
  else if (!strcasecmp(Name, "RES4")) *Result = 4;
  else if (!strcasecmp(Name, "RES5")) *Result = 5;
  else if (!strcasecmp(Name, "MSG_OUT")) *Result = 6;
  else if (!strcasecmp(Name, "MSG_IN")) *Result = 7;
  else if (!strcasecmp(Name, "MESSAGE_OUT")) *Result = 6;
  else if (!strcasecmp(Name, "MESSAGE_IN")) *Result = 7;
  return (*Result < 8);
}

static Boolean DecodeReg(char *Name, LongWord *Result)
{
  int z;
  Integer Mask = 1 << (MomCPU - CPU53C810);
  PReg Reg;

  for (z = 0, Reg = Regs; z < RegCnt; z++, Reg++)
    if (!(strcasecmp(Reg->Name, Name)) && (Mask & Reg->Mask))
    {
      *Result = Reg->Code;
      return True;
    }

  return False;
}

static Boolean Err(int Num, char *Msg)
{
  WrXError(Num, Msg);
  return False;
}

static Boolean DecodeCond(tStrComp *pSrc, LongWord *Dest)
{
  String TokStr;
  tStrComp Tok;
  Boolean PhaseATNUsed, DataUsed, CarryUsed, MaskUsed;
  LongWord Tmp;
  Boolean OK;

  /* IF/WHEN/Nix-Unterscheidung - TRUE fuer Nix setzen */

  StrCompMkTemp(&Tok, TokStr);
  GetToken(pSrc, &Tok);
  if (*Tok.Str == '\0')
  {
    *Dest |= 0x00080000;
    return True;
  }

  if (strcasecmp(Tok.Str, "WHEN") == 0)
    *Dest |= 0x00010000;
  else if (strcasecmp(Tok.Str, "IF") != 0)
    return Err(1135, Tok.Str);

  /* Negierung? */

  GetToken(pSrc, &Tok);  
  if (strcasecmp(Tok.Str, "NOT") == 0)
    GetToken(pSrc, &Tok);
  else
    *Dest |= 0x00080000;

  /* Bedingungen durchgehen */

  PhaseATNUsed = DataUsed = MaskUsed = CarryUsed = False;
  do
  {
    if (!strcasecmp(Tok.Str, "ATN"))
    {
      if (PhaseATNUsed)
        return Err(1350, "2 x ATN/Phase");
      if (CarryUsed)
        return Err(1350, "Carry + ATN/Phase");
      if ((*Dest & 0x00010000) != 0)
        return Err(1350, "WHEN + ATN");
      PhaseATNUsed = True;
      *Dest |= 0x00020000;
    }
    else if (DecodePhase(Tok.Str, &Tmp))
    {
      if (PhaseATNUsed)
        return Err(1350, "2 x ATN/Phase");
      if (CarryUsed)
        return Err(1350, "Carry + ATN/Phase");
      PhaseATNUsed = True;
      *Dest |= 0x00020000 + (Tmp << 24);
    }
    else if (!strcasecmp(Tok.Str, "CARRY"))
    {
      if (CarryUsed)
        return Err(1350, "2 x Carry");
      if ((PhaseATNUsed) || (DataUsed))
        return Err(1350, "Carry + ...");
      CarryUsed = True;
      *Dest |= 0x00200000;
    }
    else if (!strcasecmp(Tok.Str, "MASK"))
    {
      if (CarryUsed)
        return Err(1350, "Carry + Data");
      if (MaskUsed)
        return Err(1350, "2 x Mask");
      if (!DataUsed)
        return Err(1350, "Mask + !Data");
      GetToken(pSrc, &Tok);
      Tmp = EvalStrIntExpression(&Tok, UInt8, &OK);
      if (!OK)
        return False;
      MaskUsed = True;
      *Dest |= (Tmp << 8);
    }
    else
    {
      if (CarryUsed)
        return Err(1350, "Carry + Data");
      if (DataUsed)
        return Err(1350, "2 x Data");
      Tmp = EvalStrIntExpression(&Tok, UInt8, &OK);
      if (!OK)
        return False;
      DataUsed = True;
      *Dest |= 0x00040000 + Tmp;
    }
    GetToken(pSrc, &Tok);
    if (*Tok.Str != '\0') 
    {
      if (strcasecmp(Tok.Str, "AND"))
        return Err(1350, Tok.Str);
      GetToken(pSrc, &Tok);
    }
  }
  while (*Tok.Str != '\0');

  return True;
}

typedef enum
{
  NONE, SFBR, REGISTER, IMM8
} CompType;

static CompType DecodeComp(const tStrComp *pInp, LongWord *Outp)
{
  Boolean OK;

  if (!strcasecmp(pInp->Str, "SFBR"))
    return SFBR;
  else if (DecodeReg(pInp->Str, Outp))
    return REGISTER;
  else
  {
    *Outp = EvalStrIntExpression(pInp, Int8, &OK) & 0xff;
    return (OK) ?  IMM8 : NONE;
  }
}

/*---------------------------------------------------------------------------*/

static void DecodeFixed(Word Index)
{
  if (ChkArgCnt(0, 0))
  {
    DAsmCode[0] = ((LongWord) Index) << 24;
    DAsmCode[1] = 0x00000000;
    CodeLen = 8;
  }
}

static void DecodeJmps(Word Index)
{
  LongWord Buf;
  LongInt Adr;
  int l;
  Boolean OK;

  if (ArgCnt == 0)
  {
    if (Memo("INTFLY"))
    {
      ArgCnt = 1;
      strcpy(ArgStr[1].Str, "0");
    }
    else if (Memo("RETURN"))
    {
      ArgCnt = 1;
      *ArgStr[1].Str = '\0';
    }
  }
  if (ChkArgCnt(1, 2))
  {
    if (ArgCnt == 1)
    {
      if (Memo("RETURN"))
        StrCompCopy(&ArgStr[2], &ArgStr[1]);
      else
        StrCompReset(&ArgStr[2]);
    }
    Buf = 0;
    if (Memo("RETURN"))
    {
      Adr = 0;
      OK = True;
    }
    else
    {
      l = strlen(ArgStr[1].Str);
      if ((!strncasecmp(ArgStr[1].Str, "REL(", 4)) && (ArgStr[1].Str[l - 1] == ')'))
      {
        if (*OpPart.Str == 'I') 
        {
          WrError(ErrNum_InvAddrMode);
          OK = False;
        }
        Buf |= 0x00800000;
        strmov(ArgStr[1].Str, ArgStr[1].Str + 4);
        ArgStr[1].Str[l - 5] = '\0';
      }
      Adr = EvalStrIntExpression(&ArgStr[1], UInt32, &OK);
      if ((OK) && (Buf != 0))
      {
        Adr -= EProgCounter() + 8;
        if ((!SymbolQuestionable) && ((Adr > 0x7fffff) || (Adr < -0x800000)))
        {
          WrError(ErrNum_JmpDistTooBig);
          OK = False;
        }
      }
    }
    if ((OK) && (DecodeCond(&ArgStr[2], &Buf)))
    {
      DAsmCode[0] = 0x80000000 + (((LongWord) Index) << 27) + Buf;
      if (Memo("INTFLY")) DAsmCode[0] |= 0x00100000;
      DAsmCode[1] = Adr;
      CodeLen = 8;
    }
  }
}

static void DecodeCHMOV(Word Index)
{
  String TokenStr;
  tStrComp Token, *pAdrArg = NULL;
  LongWord Phase;
  Boolean OK;

  UNUSED(Index);
  StrCompMkTemp(&Token, TokenStr);  

  if ((ChkExactCPUList(0, CPU53C825, CPU53C875, CPU53C895, CPUNone) >= 0)
   && ChkArgCnt(2, 3))
  {
    GetToken(&ArgStr[ArgCnt], &Token);
    if (!strcasecmp(Token.Str, "WITH"))
      DAsmCode[0] = 0x08000000;
    else if (!strcasecmp(Token.Str, "WHEN"))
      DAsmCode[0] = 0x00000000;
    else
    {
      WrStrErrorPos(ErrNum_InvAddrMode, &Token);
      return;
    }
    KillPrefBlanksStrComp(&ArgStr[ArgCnt]);
    KillPostBlanksStrComp(&ArgStr[ArgCnt]);
    if (!DecodePhase(ArgStr[ArgCnt].Str, &Phase)) WrStrErrorPos(ErrNum_InvAddrMode, &ArgStr[ArgCnt]);
    else
    {
      DAsmCode[0] |= Phase << 24;
      OK = False;
      if (ArgCnt == 2)
      {
        GetToken(&ArgStr[1], &Token);
        if (strcasecmp(Token.Str, "FROM")) WrError(ErrNum_InvAddrMode);
        else
        {
          pAdrArg = &ArgStr[1];
          DAsmCode[0] |= 0x10000000;
          OK = True;
        }
      }
      else
      {
        Phase = EvalStrIntExpression(&ArgStr[1], UInt24, &OK);
        if (OK)
        {
          DAsmCode[0] |= Phase;
          if (!strncasecmp(ArgStr[2].Str,"PTR", 3))
          {
            StrCompCutLeft(&ArgStr[2], 4);
            DAsmCode[0] |= 0x20000000;
          }
          pAdrArg = &ArgStr[2];
        }
      }
      if (OK)
      {
        KillPrefBlanksStrComp(pAdrArg);
        DAsmCode[1] = EvalStrIntExpression(pAdrArg, UInt32, &OK);
        if (OK)
          CodeLen = 8;
      }
    }
  }
}

static Boolean TrueFnc(void)
{
  return True;
}

static void DecodeFlags(Word Index)
{
  Boolean OK;
  String TokenStr;
  tStrComp Token;

  StrCompMkTemp(&Token, TokenStr);
  if (ChkArgCnt(1, 1))
  {
    OK = True;
    DAsmCode[0] = ((LongWord) Index) << 24;
    DAsmCode[1] = 0;
    while ((OK) && (*ArgStr[1].Str != '\0'))
    {
      GetToken(&ArgStr[1], &Token);
      if (!strcasecmp(Token.Str, "ACK"))
        DAsmCode[0] |= 0x00000040;
      else if (!strcasecmp(Token.Str, "ATN"))
        DAsmCode[0] |= 0x00000008;
      else if (!strcasecmp(Token.Str, "TARGET"))
        DAsmCode[0] |= 0x00000200;
      else if (!strcasecmp(Token.Str, "CARRY"))
        DAsmCode[0] |= 0x00000400;
      else
      {
        OK = False;
        WrStrErrorPos(ErrNum_InvAddrMode, &Token);
      }
      if (OK && (*ArgStr[1].Str != '\0'))
      {
        GetToken(&ArgStr[1], &Token);
        if (strcasecmp(Token.Str, "AND"))
        {
          OK = False;
          WrStrErrorPos(ErrNum_InvAddrMode, &Token);
        }
      }
    }
    if (OK)
      CodeLen = 8;
  }
}

static void DecodeRegTrans(Word Index)
{
  LongWord Reg, Cnt;
  Boolean OK;

  if (!ChkArgCnt(3, 3));
  else if (!ChkExcludeCPU(CPU53C815));
  else if (!DecodeReg(ArgStr[1].Str, &Reg)) WrStrErrorPos(ErrNum_InvReg, &ArgStr[1]);
  else
  {
    FirstPassUnknown = False;
    Cnt = EvalStrIntExpression(&ArgStr[2], UInt3, &OK);
    if (FirstPassUnknown)
      Cnt = 1;
    if ((OK) && (ChkRange(Cnt, 1, 4)))
    {
      int l = strlen(ArgStr[3].Str);
      DAsmCode[0] = 0xe0000000 + (((LongInt) Index) << 24) + (Reg << 16) + Cnt;
      if ((!strncasecmp(ArgStr[3].Str, "DSAREL(", 7))
       && (ArgStr[3].Str[l - 1] == ')'))
      {
        ArgStr[3].Str[--l] = '\0';
        DAsmCode[0] |= 0x10000000;
        DAsmCode[1] = EvalStrIntExpressionOffs(&ArgStr[3], 6, SInt24, &OK) & 0xffffff;
      }
      else
        DAsmCode[1] = EvalStrIntExpression(&ArgStr[3], UInt32, &OK);
      if (OK)
        CodeLen = 8;
    }
  }
}

static void DecodeMOVE(Word Index)
{
#define MAXPARTS 8
  Boolean WithCarry;
  String TokenStr;
  tStrComp Token;
  LongWord Tmp, DReg , AriOp = 0xff, ImmVal = 0x100;
  Boolean OK;
  int z;
  Word BigCPUMask = (1 << (CPU53C825 - CPU53C810))
                  | (1 << (CPU53C875 - CPU53C810))
                  | (1 << (CPU53C895 - CPU53C810));

  UNUSED(Index);
  StrCompMkTemp(&Token, TokenStr);
  
  if (!ChkArgCnt(1, 3));
  else if (ArgCnt == 1) /* MOVE Register */
  {
    String PartStr[MAXPARTS];
    tStrComp Parts[MAXPARTS];
    int PartCnt = 0;

    for (z = 0; z < MAXPARTS; z++)
      StrCompMkTemp(&Parts[z], PartStr[z]);
    do
    {
      GetToken(&ArgStr[1], &Parts[PartCnt++]);
    }
    while ((*ArgStr[1].Str != '\0') && (PartCnt < MAXPARTS));
    if ((PartCnt > 1) && (!strcasecmp(Parts[PartCnt - 1].Str, "CARRY")) && (!strcasecmp(Parts[PartCnt - 1].Str, "TO")))
    {
      WithCarry = True;
      PartCnt -= 2;
    }
    else
      WithCarry = False;
    DAsmCode[0] = 0x40000000;
    DAsmCode[1] = 0;
    if (PartCnt == 3)
    {
      if (WithCarry) WrError(ErrNum_InvAddrMode);
      else if (!strcasecmp(Parts[1].Str, "TO")) /* MOVE */
      {
        switch (DecodeComp(&Parts[0], &ImmVal))
        {
          case SFBR:
            switch (DecodeComp(&Parts[2], &ImmVal))
            {
              case SFBR:
                ImmVal = 8;
              case REGISTER: /* --> 0x00 OR SFBR to reg */
                DAsmCode[0] += 0x2a000000 + (ImmVal << 16);
                CodeLen = 8;
                break;
              default:
                WrError(ErrNum_InvAddrMode);
            }
            break;
          case REGISTER:
            DReg = ImmVal;
            switch (DecodeComp(&Parts[2], &ImmVal))
            {
              case SFBR: /* --> 0x00 OR reg to SFBR */
                DAsmCode[0] += 0x32000000 + (DReg << 16);
                CodeLen = 8;
                break;
              case REGISTER:
                if (ImmVal != DReg) WrError(ErrNum_InvAddrMode);
                else
                {
                  DAsmCode[0] += 0x3a000000 + (DReg << 16);
                  CodeLen = 8;
                }
                break;
              default:
                WrError(ErrNum_InvAddrMode);
            }
            break;
          case IMM8:
            switch (DecodeComp(&Parts[2], &DReg))
            {
              case SFBR:
                DReg = 8;
              case REGISTER: /* --> imm to reg */
                DAsmCode[0] += 0x38000000 + (DReg << 16) + (ImmVal << 8);
                CodeLen = 8;
                break;
              default:
                WrError(ErrNum_InvAddrMode);
            }
            break;
          default:
            WrError(ErrNum_InvAddrMode);
            break;
        }
      } /* ... TO ... */
      else if ((!strcasecmp(Parts[1].Str, "SHL")) || (!strcasecmp(Parts[1].Str, "SHR")))
      {
        AriOp = 1 + (Ord(mytoupper(Parts[1].Str[2]) == 'R') << 2);
        switch (DecodeComp(&Parts[0], &DReg))
        {
           case SFBR:
             switch (DecodeComp(&Parts[2], &DReg))
             {
               case SFBR:
                 DReg = 8;
               case REGISTER:
                 DAsmCode[0] += 0x28000000 + (AriOp << 24) + (DReg << 16);
                 CodeLen = 8;
                 break;
               default:
                 WrError(ErrNum_InvAddrMode);
             }
             break;
           case REGISTER:
             ImmVal = DReg;
             switch (DecodeComp(&Parts[2], &DReg))
             {
               case SFBR:
                 DAsmCode[0] += 0x30000000 + (AriOp << 24) + (ImmVal << 16);
                 CodeLen = 8;
                 break;
               case REGISTER:
                 if (DReg != ImmVal) WrError(ErrNum_InvAddrMode);
                 else
                 {
                   DAsmCode[0] += 0x38000000 + (AriOp << 24) + (ImmVal << 16);
                   CodeLen = 8;
                 }
                 break;
               default:
                 WrError(ErrNum_InvAddrMode);
             }
             break;
          default:
            WrError(ErrNum_InvAddrMode);
        }
      } /* ... SHx ... */
    } /* PartCnt == 3 */
    else if (PartCnt == 5)
    {
      if (strcasecmp(Parts[3].Str, "TO")) WrError(ErrNum_InvAddrMode);
      else
      {
        if ((!strcasecmp(Parts[1].Str, "XOR"))
         || (!strcasecmp(Parts[1].Str, "^")))
          AriOp = 3;
        else if ((!strcasecmp(Parts[1].Str, "OR"))
              || (!strcasecmp(Parts[1].Str, "|")))
          AriOp = 2;
        else if ((!strcasecmp(Parts[1].Str, "AND"))
              || (!strcasecmp(Parts[1].Str, "&")))
          AriOp = 4;
        else if (!strcmp(Parts[1].Str, "+"))
          AriOp = 6;
        if (WithCarry)
          AriOp = (AriOp == 6) ? 7 : 0xff;
        if (AriOp == 0xff) WrError(ErrNum_InvAddrMode);
        else
        {
          DAsmCode[0] |= (AriOp << 24);
          switch (DecodeComp(&Parts[0], &ImmVal))
          {
            case SFBR:
              switch (DecodeComp(&Parts[2], &ImmVal))
              {
                case SFBR:
                  switch (DecodeComp(&Parts[4], &ImmVal))
                  {
                    case SFBR:
                      ImmVal = 8;
                    case REGISTER:
                      if (ChkExactCPUMask(BigCPUMask, CPU53C810) >= 0)
                      {
                        DAsmCode[0] |= 0x28800000 + (ImmVal << 16);
                        CodeLen = 8;
                      }
                      break;
                    default:
                      WrError(ErrNum_InvAddrMode);
                  }
                  break;
                case IMM8:
                  switch (DecodeComp(&Parts[4], &DReg))
                  {
                    case SFBR:
                      DReg = 8;
                    case REGISTER:
                      DAsmCode[0] |= 0x28000000 + (DReg << 16) + (ImmVal << 8);
                      CodeLen = 8;
                      break;
                    default:
                      WrError(ErrNum_InvAddrMode);
                  }
                  break;
                default:
                  WrError(ErrNum_InvAddrMode);
              }
              break;
            case REGISTER:
              DAsmCode[0] |= ImmVal << 16;
              DReg = ImmVal;
              switch (DecodeComp(&Parts[2], &ImmVal))
              {
                case SFBR:
                  switch (DecodeComp(&Parts[4], &ImmVal))
                  {
                    case SFBR:
                      if (ChkExactCPUMask(BigCPUMask, CPU53C810) >= 0)
                      {
                        DAsmCode[0] |= 0x30800000;
                        CodeLen = 8;
                      }
                      break;
                    case REGISTER:
                      if (DReg != ImmVal) WrError(ErrNum_InvAddrMode);
                      else if (ChkExactCPUMask(BigCPUMask, CPU53C810) >= 0)
                      {
                        DAsmCode[0] |= 0x38800000;
                        CodeLen = 8;
                      }
                      break;
                    default:
                      WrError(ErrNum_InvAddrMode);
                  }
                  break;
                case IMM8:
                  DAsmCode[0] |= (ImmVal << 8);
                  switch (DecodeComp(&Parts[4], &Tmp))
                  {
                    case SFBR:
                      DAsmCode[0] |= 0x30000000;
                      CodeLen = 8;
                      break;
                    case REGISTER: 
                      if (DReg != Tmp) WrError(ErrNum_InvAddrMode);
                      else
                      {
                        DAsmCode[0] |= 0x38000000;
                        CodeLen = 8;
                      }
                      break;
                    default:
                      WrError(ErrNum_InvAddrMode);
                  }
                  break;
                default:
                  WrError(ErrNum_InvAddrMode);
              }
              break;
            default:
              WrError(ErrNum_InvAddrMode);
          }
        }
      }
    }  /* PartCnt == 5 */
    else
      WrError(ErrNum_InvAddrMode);
  }
  else if (ArgCnt == 2)
  {
    GetToken(&ArgStr[1], &Token);
    if (strcasecmp(Token.Str, "FROM")) WrError(ErrNum_InvAddrMode);
    else
    {
      DAsmCode[0] = 0x00000000;
      DAsmCode[1] = EvalStrIntExpression(&ArgStr[1], Int32, &OK);
      if (OK)
      {
        GetToken(&ArgStr[2], &Token);
        OK = True;
        if (!strcasecmp(Token.Str, "WHEN"))
          DAsmCode[0] |= 0x08000000;
        else if (strcasecmp(Token.Str, "WITH"))
          OK = False;
        if (!OK) WrError(ErrNum_InvAddrMode);
        else
        {
          KillPrefBlanksStrComp(&ArgStr[2]);
          KillPostBlanksStrComp(&ArgStr[2]);
          if (!DecodePhase(ArgStr[2].Str, &ImmVal)) WrStrErrorPos(ErrNum_InvAddrMode, &ArgStr[2]);
          else
          {
            DAsmCode[0] |= ImmVal << 24;
            CodeLen = 8;
          }
        }
      }
    }
  }
  else if (ArgCnt == 3)
  {
    if (!strncasecmp(ArgStr[1].Str, "MEMORY", 6))
    {
      StrCompCutLeft(&ArgStr[1], 7);
      if (!strncasecmp(ArgStr[1].Str, "NO FLUSH", 8))
      {
        DAsmCode[0] = 0xc1000000;
        StrCompCutLeft(&ArgStr[1], 9);
      }
      else DAsmCode[0] = 0xc0000000;
      DAsmCode[0] |= EvalStrIntExpression(&ArgStr[1], UInt24, &OK);
      if (OK)
      {
        DAsmCode[1] = EvalStrIntExpression(&ArgStr[2], Int32, &OK);
        if (OK)
        {
          DAsmCode[2] = EvalStrIntExpression(&ArgStr[3], Int32, &OK);
          if (OK) CodeLen = 12;
        }
      }
    }
    else
    {
      DAsmCode[0] = EvalStrIntExpression(&ArgStr[1], UInt24, &OK);
      if (OK)
      {
        GetToken(&ArgStr[3], &Token);
        OK = True;
        if (!strcasecmp(Token.Str, "WHEN")) DAsmCode[0] |= 0x08000000;
        else if (strcasecmp(Token.Str, "WITH")) OK = False;
        if (!OK) WrError(ErrNum_InvAddrMode);
        else
        {
          KillPrefBlanksStrComp(&ArgStr[3]);
          KillPostBlanksStrComp(&ArgStr[3]);
          if (!DecodePhase(ArgStr[3].Str, &ImmVal)) WrStrErrorPos(ErrNum_InvAddrMode, &ArgStr[3]);
          else
          { 
            DAsmCode[0] |= ImmVal << 24;
            if (!strncasecmp(ArgStr[2].Str, "PTR", 3))
            {
              StrCompCutLeft(&ArgStr[2], 4);
              DAsmCode[0] |= 0x20000000;
            }
            DAsmCode[1] = EvalStrIntExpression(&ArgStr[2], UInt32, &OK);
            if (OK) CodeLen = 8;
          }
        }
      }
    }
  }
}

static void DecodeSELECT(Word MayATN)
{
  Boolean OK;
  LongInt Dist;
  int l;

  if (ChkArgCnt(2, 2))
  {
    DAsmCode[0] = 0x40000000;
    OK = True;
    if (!strncasecmp(ArgStr[1].Str, "ATN ", 4))
    {
      strmov(ArgStr[1].Str, ArgStr[1].Str + 4);
      ArgStr[1].Pos.StartCol += 4;
      ArgStr[1].Pos.Len -= 4;
      KillPrefBlanksStrComp(&ArgStr[1]);
      if (!MayATN)
        OK = False;
      else
        DAsmCode[0] |= 0x01000000;
    }
    if (!OK) WrError(ErrNum_InvAddrMode);
    else
    {
      if (!strncasecmp(ArgStr[1].Str, "FROM ", 5))
      {
        strmov(ArgStr[1].Str, ArgStr[1].Str + 5);
        ArgStr[1].Pos.StartCol += 5;
        ArgStr[1].Pos.Len -= 5;
        KillPrefBlanksStrComp(&ArgStr[1]);
        DAsmCode[0] |= 0x02000000 + EvalStrIntExpression(&ArgStr[1], UInt24, &OK);
      }
      else
        DAsmCode[0] |= EvalStrIntExpression(&ArgStr[1], UInt4, &OK) << 16;
      if (OK)
      {
        l = strlen(ArgStr[2].Str);
        if ((!strncasecmp(ArgStr[2].Str, "REL(", 4)) && (ArgStr[2].Str[l - 1] == ')'))
        {
          DAsmCode[0] |= 0x04000000;
          ArgStr[2].Str[l - 1] = '\0';
          Dist = EvalStrIntExpressionOffs(&ArgStr[2], 4, UInt32, &OK) - (EProgCounter() + 8);
          if (OK)
          {
            if ((!SymbolQuestionable) && ((Dist > 0x7fffff) || (Dist < -0x800000))) WrError(ErrNum_JmpDistTooBig);
            else DAsmCode[1] = Dist & 0xffffff;
          }
        }
        else DAsmCode[1] = EvalStrIntExpression(&ArgStr[2], UInt32, &OK);
        if (OK) CodeLen = 8;
      }
    }
  }
}

static void DecodeWAIT(Word Index)
{
  String TokenStr;
  tStrComp Token;
  Boolean OK;
  LongInt Dist;

  UNUSED(Index);
  StrCompMkTemp(&Token, TokenStr);

  if (ChkArgCnt(1, 1))
  {
    GetToken(&ArgStr[1], &Token);
    KillPrefBlanksStrComp(&ArgStr[1]);
    if (!strcasecmp(Token.Str, "DISCONNECT"))
    {
      if (*ArgStr[1].Str != '\0') WrError(ErrNum_InvAddrMode);
      else
      {
        DAsmCode[0] = 0x48000000;
        DAsmCode[1] = 0;
        CodeLen = 8;
      }
    }
    else if ((!strcasecmp(Token.Str, "RESELECT")) || (!strcasecmp(Token.Str, "SELECT")))
    {
      if ((!strncasecmp(ArgStr[1].Str, "REL(", 4)) && (ArgStr[1].Str[strlen(ArgStr[1].Str) - 1] == ')'))
      {
        StrCompShorten(&ArgStr[1], 1);
        DAsmCode[0] = 0x54000000;
        Dist = EvalStrIntExpressionOffs(&ArgStr[1], 4, UInt32, &OK) - (EProgCounter() + 8);
        if (OK)
        {
          if ((!SymbolQuestionable) && ((Dist > 0x7fffff) || (Dist < -0x800000))) WrError(ErrNum_JmpDistTooBig);
          else
            DAsmCode[1] = Dist & 0xffffff;
        }
      }
      else
      {
        DAsmCode[0] = 0x50000000;
        DAsmCode[1] = EvalStrIntExpression(&ArgStr[1], UInt32, &OK);
      }
      if (OK)
      {
        if (mytoupper(*Token.Str) == 'S')
          DAsmCode[0] |= 0x00000200;
        CodeLen = 8;
      }
    }
  }
}

/*---------------------------------------------------------------------------*/

static void AddReg(char *NName, LargeWord Adr, Word Mask)
{
  if (InstrZ >= RegCnt) exit(255);
  Regs[InstrZ].Name = NName;
  Regs[InstrZ].Code = Adr;
  Regs[InstrZ++].Mask = Mask;
}

static void InitFields(void)
{
  InstTable = CreateInstTable(51);
  AddInstTable(InstTable, "NOP"     , 0x80, DecodeFixed);
  AddInstTable(InstTable, "DISCONNECT" , 0x48, DecodeFixed);
  AddInstTable(InstTable, "JUMP"    , 0,    DecodeJmps);
  AddInstTable(InstTable, "CALL"    , 1,    DecodeJmps);
  AddInstTable(InstTable, "RETURN"  , 2,    DecodeJmps);
  AddInstTable(InstTable, "INT"     , 3,    DecodeJmps);
  AddInstTable(InstTable, "INTFLY"  , 3,    DecodeJmps);
  AddInstTable(InstTable, "CHMOV"   , 0,    DecodeCHMOV);
  AddInstTable(InstTable, "CLEAR"   , 0x60, DecodeFlags);
  AddInstTable(InstTable, "SET"     , 0x58, DecodeFlags);
  AddInstTable(InstTable, "LOAD"    , 1,    DecodeRegTrans);
  AddInstTable(InstTable, "STORE"   , 0,    DecodeRegTrans);
  AddInstTable(InstTable, "MOVE"    , 0,    DecodeMOVE);
  AddInstTable(InstTable, "RESELECT", 0,    DecodeSELECT);
  AddInstTable(InstTable, "SELECT"  , 1,    DecodeSELECT);
  AddInstTable(InstTable, "WAIT"    , 0,    DecodeWAIT);

  Regs = (PReg) malloc(sizeof(TReg) * RegCnt); InstrZ = 0;
  AddReg("SCNTL0"   , 0x00, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SCNTL1"   , 0x01, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SCNTL2"   , 0x02, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SCNTL3"   , 0x03, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SCID"     , 0x04, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SXFER"    , 0x05, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SDID"     , 0x06, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("GPREG"    , 0x07, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SFBR"     , 0x08, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SOCL"     , 0x09, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SSID"     , 0x0a, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SBCL"     , 0x0b, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DSTAT"    , 0x0c, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SSTAT0"   , 0x0d, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SSTAT1"   , 0x0e, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SSTAT2"   , 0x0f, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DSA"      , 0x10, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("ISTAT"    , 0x14, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("CTEST0"   , 0x18, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("CTEST1"   , 0x19, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("CTEST2"   , 0x1a, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("CTEST3"   , 0x1b, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("TEMP"     , 0x1c, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DFIFO"    , 0x20, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("CTEST4"   , 0x21, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("CTEST5"   , 0x22, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);  
  AddReg("CTEST6"   , 0x23, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DBC"      , 0x24, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DCMD"     , 0x27, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DNAD"     , 0x28, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DSP"      , 0x2c, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DSPS"     , 0x30, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SCRATCHA" , 0x34, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DMODE"    , 0x38, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("DIEN"     , 0x39, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SBR"      , 0x3a, M_53C810                       + M_53C860 + M_53C875 + M_53C895);
  AddReg("DWT"      , 0x3a,            M_53C815                                            );
  AddReg("DCNTL"    , 0x3b, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("ADDER"    , 0x3c, M_53C810            + M_53C825 + M_53C860            + M_53C895);
  AddReg("SIEN0"    , 0x40, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SIEN1"    , 0x41, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SIST0"    , 0x42, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SIST1"    , 0x43, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SLPAR"    , 0x44, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SWIDE"    , 0x45,                       M_53C825            + M_53C875 + M_53C895);
  AddReg("MACNTL"   , 0x46, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("GPCNTL"   , 0x47, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("STIME0"   , 0x48, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("STIME1"   , 0x49, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("RESPID"   , 0x4a, M_53C810 + M_53C815 + M_53C825 + M_53C860                      );
  AddReg("RESPID0"  , 0x4a,                                             M_53C875 + M_53C895);
  AddReg("RESPID1"  , 0x4b,                                             M_53C875 + M_53C895);
  AddReg("STEST0"   , 0x4c, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("STEST1"   , 0x4d, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("STEST2"   , 0x4e, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("STEST3"   , 0x4f, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SIDL"     , 0x50, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("STEST4"   , 0x52,                                                      + M_53C895);
  AddReg("SODL"     , 0x54, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SBDL"     , 0x58, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SCRATCHB" , 0x5c, M_53C810 + M_53C815 + M_53C825 + M_53C860 + M_53C875 + M_53C895);
  AddReg("SCRATCHC" , 0x60,                                             M_53C875 + M_53C895);
  AddReg("SCRATCHD" , 0x64,                                             M_53C875 + M_53C895);
  AddReg("SCRATCHE" , 0x68,                                             M_53C875 + M_53C895);
  AddReg("SCRATCHF" , 0x6c,                                             M_53C875 + M_53C895);
  AddReg("SCRATCHG" , 0x70,                                             M_53C875 + M_53C895);
  AddReg("SCRATCHH" , 0x74,                                             M_53C875 + M_53C895);
  AddReg("SCRATCHI" , 0x78,                                             M_53C875 + M_53C895);
  AddReg("SCRATCHJ" , 0x7c,                                             M_53C875 + M_53C895);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
  free(Regs);
}

/*---------------------------------------------------------------------------*/

static void MakeCode_53c8xx(void)
{
  CodeLen = 0;
  DontPrint = False;

  /* zu ignorierendes */   

  if (Memo(""))
    return; 

  if (DecodeIntelPseudo(False))
    return;

  if (!LookupInstTable(InstTable, OpPart.Str))
    WrStrErrorPos(ErrNum_UnknownInstruction, &OpPart);
}

static Boolean IsDef_53c8xx(void)
{
  return False;
}

static void SwitchFrom_53c8xx(void)
{
  DeinitFields();
}

static void SwitchTo_53c8xx(void)
{
  PFamilyDescr FoundDescr;

  FoundDescr = FindFamilyByName("SYM53C8xx");

  TurnWords = False;
  ConstMode = ConstModeC;
  SetIsOccupiedFnc = TrueFnc;
  PCSymbol="$";
  HeaderID = FoundDescr->Id;
  NOPCode = 0;
  DivideChars = ",";
  HasAttrs = False;

  ValidSegs = (1 << SegCode);
  Grans[SegCode ] = 1; ListGrans[SegCode ] = 4; SegInits[SegCode ] = 0;
  SegLimits[SegCode] = INTCONST_ffffffff;

  MakeCode = MakeCode_53c8xx;
  IsDef = IsDef_53c8xx;
  SwitchFrom = SwitchFrom_53c8xx;

  InitFields();
}

/*---------------------------------------------------------------------------*/

void code53c8xx_init(void)
{
  CPU53C810 = AddCPU("SYM53C810", SwitchTo_53c8xx);
  CPU53C860 = AddCPU("SYM53C860", SwitchTo_53c8xx);
  CPU53C815 = AddCPU("SYM53C815", SwitchTo_53c8xx);
  CPU53C825 = AddCPU("SYM53C825", SwitchTo_53c8xx);
  CPU53C875 = AddCPU("SYM53C875", SwitchTo_53c8xx);
  CPU53C895 = AddCPU("SYM53C895", SwitchTo_53c8xx);
}
