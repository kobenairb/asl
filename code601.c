/* code601.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator PowerPC-Familie                                             */
/*                                                                           */
/* Historie: 17.10.1996 Grundsteinlegung                                     */
/*                                                                           */
/*****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "endian.h"
#include "stringutil.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "codepseudo.h"

typedef struct 
         {
          char *Name;
          LongWord Code;
          Byte CPUMask;
         } BaseOrder;

#define FixedOrderCount      6
#define Reg1OrderCount       4
#define FReg1OrderCount      2
#define CReg1OrderCount      1
#define CBit1OrderCount      4
#define Reg2OrderCount       29
#define CReg2OrderCount      2
#define FReg2OrderCount      14
#define Reg2BOrderCount      2
#define Reg2SwapOrderCount   6
#define NoDestOrderCount     10
#define Reg3OrderCount       89
#define CReg3OrderCount      8
#define FReg3OrderCount      10
#define Reg3SwapOrderCount   49
#define MixedOrderCount      8
#define FReg4OrderCount      16
#define RegDispOrderCount    16
#define FRegDispOrderCount   8
#define Reg2ImmOrderCount    12
#define Imm16OrderCount      7
#define Imm16SwapOrderCount  6

static BaseOrder *FixedOrders;
static BaseOrder *Reg1Orders;
static BaseOrder *CReg1Orders;
static BaseOrder *CBit1Orders;
static BaseOrder *FReg1Orders;
static BaseOrder *Reg2Orders;
static BaseOrder *CReg2Orders;
static BaseOrder *FReg2Orders;
static BaseOrder *Reg2BOrders;
static BaseOrder *Reg2SwapOrders;
static BaseOrder *NoDestOrders;
static BaseOrder *Reg3Orders;
static BaseOrder *CReg3Orders;
static BaseOrder *FReg3Orders;
static BaseOrder *Reg3SwapOrders;
static BaseOrder *MixedOrders;
static BaseOrder *FReg4Orders;
static BaseOrder *RegDispOrders;
static BaseOrder *FRegDispOrders;
static BaseOrder *Reg2ImmOrders;
static BaseOrder *Imm16Orders;
static BaseOrder *Imm16SwapOrders;

static void (*SaveInitProc)(void);
static Boolean BigEnd;

static CPUVar CPU403,CPU505,CPU601,CPU6000;

/*-------------------------------------------------------------------------*/
/**
{       PROCEDURE EnterByte(b:Byte);
BEGIN
   if Odd(CodeLen)
    BEGIN
     BAsmCode[CodeLen]:=BAsmCode[CodeLen-1];
     BAsmCode[CodeLen-1]:=b;
    END
   else
    BEGIN
     BAsmCode[CodeLen]:=b;
    END;
   Inc(CodeLen);
END;}
**/
/*-------------------------------------------------------------------------*/

static int InstrZ;

	static void AddFixed(char *NName1, char *NName2, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=FixedOrderCount) exit(255);
   FixedOrders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   FixedOrders[InstrZ].Code=NCode;
   FixedOrders[InstrZ++].CPUMask=NMask;
END

        static void AddReg1(char *NName1, char *NName2, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=Reg1OrderCount) exit(255);
   Reg1Orders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   Reg1Orders[InstrZ].Code=NCode;
   Reg1Orders[InstrZ++].CPUMask=NMask;
END

        static void AddCReg1(char *NName1, char *NName2, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=CReg1OrderCount) exit(255);
   CReg1Orders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   CReg1Orders[InstrZ].Code=NCode;
   CReg1Orders[InstrZ++].CPUMask=NMask;
END

        static void AddCBit1(char *NName1, char *NName2, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=CBit1OrderCount) exit(255);
   CBit1Orders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   CBit1Orders[InstrZ].Code=NCode;
   CBit1Orders[InstrZ++].CPUMask=NMask;
END

        static void AddFReg1(char *NName1, char *NName2, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=FReg1OrderCount) exit(255);
   FReg1Orders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   FReg1Orders[InstrZ].Code=NCode;
   FReg1Orders[InstrZ++].CPUMask=NMask;
END

	static void AddSReg2(char *NName, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=Reg2OrderCount) exit(255);
   if (NName==Nil) exit(255);
   Reg2Orders[InstrZ].Name=NName;
   Reg2Orders[InstrZ].Code=NCode;
   Reg2Orders[InstrZ++].CPUMask=NMask;
END

	static void AddReg2(char *NName1, char *NName2, LongInt NCode, Byte NMask, Boolean WithOE, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSReg2(strdup(NName),NCode,NMask);
   if (WithOE)
    BEGIN
     strcat(NName,"O");
     AddSReg2(strdup(NName),NCode | 0x400,NMask);
     NName[strlen(NName)-1]='\0';
    END
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSReg2(strdup(NName),NCode | 0x001,NMask);
     NName[strlen(NName)-1]='\0';
     if (WithOE)
      BEGIN
       strcat(NName,"O."); 
       AddSReg2(strdup(NName),NCode | 0x401,NMask);
      END
    END
END

	static void AddCReg2(char *NName1, char *NName2, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=CReg2OrderCount) exit(255);
   CReg2Orders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   CReg2Orders[InstrZ].Code=NCode;
   CReg2Orders[InstrZ++].CPUMask=NMask;
END

	static void AddSFReg2(char *NName, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=FReg2OrderCount) exit(255);
   if (NName==Nil) exit(255);
   FReg2Orders[InstrZ].Name=NName;
   FReg2Orders[InstrZ].Code=NCode;
   FReg2Orders[InstrZ++].CPUMask=NMask;
END

	static void AddFReg2(char *NName1, char *NName2, LongInt NCode, Byte NMask, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSFReg2(strdup(NName),NCode,NMask);
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSFReg2(strdup(NName),NCode | 0x001,NMask);
    END
END

        static void AddReg2B(char *NName1, char *NName2, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=Reg2BOrderCount) exit(255);
   Reg2BOrders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   Reg2BOrders[InstrZ].Code=NCode;
   Reg2BOrders[InstrZ++].CPUMask=NMask;
END

	static void AddSReg2Swap(char *NName, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=Reg2SwapOrderCount) exit(255);
   if (NName==Nil) exit(255);
   Reg2SwapOrders[InstrZ].Name=NName;
   Reg2SwapOrders[InstrZ].Code=NCode;
   Reg2SwapOrders[InstrZ++].CPUMask=NMask;
END

	static void AddReg2Swap(char *NName1, char *NName2, LongInt NCode, Byte NMask, Boolean WithOE, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSReg2Swap(strdup(NName),NCode,NMask);
   if (WithOE)
    BEGIN
     strcat(NName,"O");
     AddSReg2Swap(strdup(NName),NCode | 0x400,NMask);
     NName[strlen(NName)-1]='\0';
    END
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSReg2Swap(strdup(NName),NCode | 0x001,NMask);
     NName[strlen(NName)-1]='\0';
     if (WithOE)
      BEGIN
       strcat(NName,"O."); 
       AddSReg2Swap(strdup(NName),NCode | 0x401,NMask);
      END
    END
END

        static void AddNoDest(char *NName1, char *NName2, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=NoDestOrderCount) exit(255);
   NoDestOrders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   NoDestOrders[InstrZ].Code=NCode;
   NoDestOrders[InstrZ++].CPUMask=NMask;
END

	static void AddSReg3(char *NName, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=Reg3OrderCount) exit(255);
   if (NName==Nil) exit(255);
   Reg3Orders[InstrZ].Name=NName;
   Reg3Orders[InstrZ].Code=NCode;
   Reg3Orders[InstrZ++].CPUMask=NMask;
END

	static void AddReg3(char *NName1, char *NName2, LongInt NCode, Byte NMask, Boolean WithOE, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSReg3(strdup(NName),NCode,NMask);
   if (WithOE)
    BEGIN
     strcat(NName,"O");
     AddSReg3(strdup(NName),NCode | 0x400,NMask);
     NName[strlen(NName)-1]='\0';
    END
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSReg3(strdup(NName),NCode | 0x001,NMask);
     NName[strlen(NName)-1]='\0';
     if (WithOE)
      BEGIN
       strcat(NName,"O."); 
       AddSReg3(strdup(NName),NCode | 0x401,NMask);
      END
    END
END

	static void AddCReg3(char *NName, LongWord NCode, CPUVar NMask)
BEGIN
   if (InstrZ>=CReg3OrderCount) exit(255);
   CReg3Orders[InstrZ].Name=NName;
   CReg3Orders[InstrZ].Code=NCode;
   CReg3Orders[InstrZ++].CPUMask=NMask;
END

	static void AddSFReg3(char *NName, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=FReg3OrderCount) exit(255);
   if (NName==Nil) exit(255);
   FReg3Orders[InstrZ].Name=NName;
   FReg3Orders[InstrZ].Code=NCode;
   FReg3Orders[InstrZ++].CPUMask=NMask;
END

	static void AddFReg3(char *NName1, char *NName2, LongInt NCode, Byte NMask, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSFReg3(strdup(NName),NCode,NMask);
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSFReg3(strdup(NName),NCode | 0x001,NMask);
    END
END

	static void AddSReg3Swap(char *NName, LongInt NCode, Byte NMask)
BEGIN
   if (InstrZ>=Reg3SwapOrderCount) exit(255);
   if (NName==Nil) exit(255);
   Reg3SwapOrders[InstrZ].Name=NName;
   Reg3SwapOrders[InstrZ].Code=NCode;
   Reg3SwapOrders[InstrZ++].CPUMask=NMask;
END

	static void AddReg3Swap(char *NName1, char *NName2, LongInt NCode, Byte NMask, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSReg3Swap(strdup(NName),NCode,NMask);
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSReg3Swap(strdup(NName),NCode | 0x001,NMask);
    END
END

	static void AddMixed(char *NName1, char *NName2, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=MixedOrderCount) exit(255);
   MixedOrders[InstrZ].Name=(MomCPU==CPU6000)?NName1:NName2;
   MixedOrders[InstrZ].Code=NCode;
   MixedOrders[InstrZ++].CPUMask=NMask;
END

	static void AddSFReg4(char *NName, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=FReg4OrderCount) exit(255);
   if (NName==Nil) exit(255);
   FReg4Orders[InstrZ].Name=NName;
   FReg4Orders[InstrZ].Code=NCode;
   FReg4Orders[InstrZ++].CPUMask=NMask;
END

	static void AddFReg4(char *NName1, char *NName2, LongWord NCode, Byte NMask, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSFReg4(strdup(NName),NCode,NMask);
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSFReg4(strdup(NName),NCode | 0x001,NMask);
    END
END

	static void AddRegDisp(char *NName1, char *NName2, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=RegDispOrderCount) exit(255);
   RegDispOrders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   RegDispOrders[InstrZ].Code=NCode;
   RegDispOrders[InstrZ++].CPUMask=NMask;
END

	static void AddFRegDisp(char *NName1, char *NName2, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=FRegDispOrderCount) exit(255);
   FRegDispOrders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   FRegDispOrders[InstrZ].Code=NCode;
   FRegDispOrders[InstrZ++].CPUMask=NMask;
END

	static void AddSReg2Imm(char *NName, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=Reg2ImmOrderCount) exit(255);
   if (NName==Nil) exit(255);
   Reg2ImmOrders[InstrZ].Name=NName;
   Reg2ImmOrders[InstrZ].Code=NCode;
   Reg2ImmOrders[InstrZ++].CPUMask=NMask;
END

	static void AddReg2Imm(char *NName1, char *NName2, LongWord NCode, Byte NMask, Boolean WithFL)
BEGIN
   String NName;

   strcpy(NName,(MomCPU==CPU6000)?NName1:NName2);
   AddSReg2Imm(strdup(NName),NCode,NMask);
   if (WithFL)
    BEGIN
     strcat(NName,"."); 
     AddSReg2Imm(strdup(NName),NCode | 0x001,NMask);
    END
END

	static void AddImm16(char *NName1, char *NName2, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=Imm16OrderCount) exit(255);
   Imm16Orders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   Imm16Orders[InstrZ].Code=NCode;
   Imm16Orders[InstrZ++].CPUMask=NMask;
END

	static void AddImm16Swap(char *NName1, char *NName2, LongWord NCode, Byte NMask)
BEGIN
   if (InstrZ>=Imm16SwapOrderCount) exit(255);
   Imm16SwapOrders[InstrZ].Name=(MomCPU==CPU6000)?NName2:NName1;
   Imm16SwapOrders[InstrZ].Code=NCode;
   Imm16SwapOrders[InstrZ++].CPUMask=NMask;
END

	static void InitFields(void)
BEGIN
   /* --> 0 0 0 */

   FixedOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*FixedOrderCount); InstrZ=0;
   AddFixed("EIEIO"  ,"EIEIO"  ,(31u << 26)+(854 << 1),0x0f);
   AddFixed("ISYNC"  ,"ICS"    ,(19u << 26)+(150 << 1),0x0f);
   AddFixed("RFI"    ,"RFI"    ,(19u << 26)+( 50 << 1),0x0f);
   AddFixed("SC"     ,"SVCA"   ,(17u << 26)+(  1 << 1),0x0f);
   AddFixed("SYNC"   ,"DCS"    ,(31u << 26)+(598 << 1),0x0f);
   AddFixed("RFCI"   ,"RFCI"   ,(19u << 26)+( 51 << 1),0x01);

   /* D --> D 0 0 */

   Reg1Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*Reg1OrderCount); InstrZ=0;
   AddReg1("MFCR"   ,"MFCR"    ,(31u << 26)+( 19 << 1),0x0f);
   AddReg1("MFMSR"  ,"MFMSR"   ,(31u << 26)+( 83 << 1),0x0f);
   AddReg1("MTMSR"  ,"MTMSR"   ,(31u << 26)+(146 << 1),0x0f);
   AddReg1("WRTEE"  ,"WRTEE"   ,(31u << 26)+(131 << 1),0x0f);

   /* crD --> D 0 0 */

   CReg1Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*CReg1OrderCount); InstrZ=0;
   AddCReg1("MCRXR"  ,"MCRXR"  ,(31u << 26)+(512 << 1),0x0f);

   /* crbD --> D 0 0 */

   CBit1Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*CBit1OrderCount); InstrZ=0;
   AddCBit1("MTFSB0" ,"MTFSB0" ,(63u << 26)+( 70 << 1)  ,0x0c);
   AddCBit1("MTFSB0.","MTFSB0.",(63u << 26)+( 70 << 1)+1,0x0c);
   AddCBit1("MTFSB1" ,"MTFSB1" ,(63u << 26)+( 38 << 1)  ,0x0c);
   AddCBit1("MTFSB1.","MTFSB1.",(63u << 26)+( 38 << 1)+1,0x0c);

   /* frD --> D 0 0 */

   FReg1Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*FReg1OrderCount); InstrZ=0;
   AddFReg1("MFFS"   ,"MFFS"  ,(63u << 26)+(583 << 1)  ,0x0c);
   AddFReg1("MFFS."  ,"MFFS." ,(63u << 26)+(583 << 1)+1,0x0c);

   /* D,A --> D A 0 */

   Reg2Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*Reg2OrderCount); InstrZ=0;
   AddReg2("ABS"   ,"ABS"  ,(31u << 26)+(360 << 1),0x08,True ,True );
   AddReg2("ADDME" ,"AME"  ,(31u << 26)+(234 << 1),0x0f,True ,True );
   AddReg2("ADDZE" ,"AZE"  ,(31u << 26)+(202 << 1),0x0f,True ,True );
   AddReg2("CLCS"  ,"CLCS" ,(31u << 26)+(531 << 1),0x08,False,False);
   AddReg2("NABS"  ,"NABS" ,(31u << 26)+(488 << 1),0x08,True ,True );
   AddReg2("NEG"   ,"NEG"  ,(31u << 26)+(104 << 1),0x0f,True ,True );
   AddReg2("SUBFME","SFME" ,(31u << 26)+(232 << 1),0x0f,True ,True );
   AddReg2("SUBFZE","SFZE" ,(31u << 26)+(200 << 1),0x0f,True ,True );

   /* cD,cS --> D S 0 */

   CReg2Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*CReg2OrderCount); InstrZ=0;
   AddCReg2("MCRF"  ,"MCRF"  ,(19u << 26)+(  0 << 1),0x0f);
   AddCReg2("MCRFS" ,"MCRFS" ,(63u << 26)+( 64 << 1),0x0c);

   /* fD,fB --> D 0 B */

   FReg2Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*FReg2OrderCount); InstrZ=0;
   AddFReg2("FABS"  ,"FABS"  ,(63u << 26)+(264 << 1),0x0c,True );
   AddFReg2("FCTIW" ,"FCTIW" ,(63u << 26)+( 14 << 1),0x0c,True );
   AddFReg2("FCTIWZ","FCTIWZ",(63u << 26)+( 15 << 1),0x0c,True );
   AddFReg2("FMR"   ,"FMR"   ,(63u << 26)+( 72 << 1),0x0c,True );
   AddFReg2("FNABS" ,"FNABS" ,(63u << 26)+(136 << 1),0x0c,True );
   AddFReg2("FNEG"  ,"FNEG"  ,(63u << 26)+( 40 << 1),0x0c,True );
   AddFReg2("FRSP"  ,"FRSP"  ,(63u << 26)+( 12 << 1),0x0c,True );

   /* D,B --> D 0 B */

   Reg2BOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*Reg2BOrderCount); InstrZ=0;
   AddReg2B("MFSRIN","MFSRIN",(31u << 26)+(659 << 1),0x0c);
   AddReg2B("MTSRIN","MTSRI" ,(31u << 26)+(242 << 1),0x0c);

   /* A,S --> S A 0 */

   Reg2SwapOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*Reg2SwapOrderCount); InstrZ=0;
   AddReg2Swap("CNTLZW","CNTLZ" ,(31u << 26)+( 26 << 1),0x0f,False,True );
   AddReg2Swap("EXTSB ","EXTSB" ,(31u << 26)+(954 << 1),0x0f,False,True );
   AddReg2Swap("EXTSH ","EXTS"  ,(31u << 26)+(922 << 1),0x0f,False,True );

   /* A,B --> 0 A B */

   NoDestOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*NoDestOrderCount); InstrZ=0;
   AddNoDest("DCBF"  ,"DCBF"  ,(31u << 26)+(  86 << 1),0x0f);
   AddNoDest("DCBI"  ,"DCBI"  ,(31u << 26)+( 470 << 1),0x0f);
   AddNoDest("DCBST" ,"DCBST" ,(31u << 26)+(  54 << 1),0x0f);
   AddNoDest("DCBT"  ,"DCBT"  ,(31u << 26)+( 278 << 1),0x0f);
   AddNoDest("DCBTST","DCBTST",(31u << 26)+( 246 << 1),0x0f);
   AddNoDest("DCBZ"  ,"DCLZ"  ,(31u << 26)+(1014 << 1),0x0f);
   AddNoDest("DCCCI" ,"DCCCI" ,(31u << 26)+( 454 << 1),0x01);
   AddNoDest("ICBI"  ,"ICBI"  ,(31u << 26)+( 982 << 1),0x0f);
   AddNoDest("ICBT"  ,"ICBT"  ,(31u << 26)+( 262 << 1),0x01);
   AddNoDest("ICCCI" ,"ICCCI" ,(31u << 26)+( 966 << 1),0x01);

   /* D,A,B --> D A B */

   Reg3Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*Reg3OrderCount); InstrZ=0;
   AddReg3("ADD"   ,"CAX"   ,(31u << 26)+(266 << 1),0x0f,True, True );
   AddReg3("ADDC"  ,"A"     ,(31u << 26)+( 10 << 1),0x0f,True ,True );
   AddReg3("ADDE"  ,"AE"    ,(31u << 26)+(138 << 1),0x0f,True ,True );
   AddReg3("DIV"   ,"DIV"   ,(31u << 26)+(331 << 1),0x08,True ,True );
   AddReg3("DIVS"  ,"DIVS"  ,(31u << 26)+(363 << 1),0x08,True ,True );
   AddReg3("DIVW"  ,"DIVW"  ,(31u << 26)+(491 << 1),0x0f,True ,True );
   AddReg3("DIVWU" ,"DIVWU" ,(31u << 26)+(459 << 1),0x0f,True ,True );
   AddReg3("DOZ"   ,"DOZ"   ,(31u << 26)+(264 << 1),0x08,True ,True );
   AddReg3("ECIWX" ,"ECIWX" ,(31u << 26)+(310 << 1),0x08,False,False);
   AddReg3("LBZUX" ,"LBZUX" ,(31u << 26)+(119 << 1),0x0f,False,False);
   AddReg3("LBZX"  ,"LBZX"  ,(31u << 26)+( 87 << 1),0x0f,False,False);
   AddReg3("LHAUX" ,"LHAUX" ,(31u << 26)+(375 << 1),0x0f,False,False);
   AddReg3("LHAX"  ,"LHAX"  ,(31u << 26)+(343 << 1),0x0f,False,False);
   AddReg3("LHBRX" ,"LHBRX" ,(31u << 26)+(790 << 1),0x0f,False,False);
   AddReg3("LHZUX" ,"LHZUX" ,(31u << 26)+(311 << 1),0x0f,False,False);
   AddReg3("LHZX"  ,"LHZX"  ,(31u << 26)+(279 << 1),0x0f,False,False);
   AddReg3("LSCBX" ,"LSCBX" ,(31u << 26)+(277 << 1),0x08,False,True );
   AddReg3("LSWX"  ,"LSX"   ,(31u << 26)+(533 << 1),0x0f,False,False);
   AddReg3("LWARX" ,"LWARX" ,(31u << 26)+( 20 << 1),0x0f,False,False);
   AddReg3("LWBRX" ,"LBRX"  ,(31u << 26)+(534 << 1),0x0f,False,False);
   AddReg3("LWZUX" ,"LUX"   ,(31u << 26)+( 55 << 1),0x0f,False,False);
   AddReg3("LWZX"  ,"LX"    ,(31u << 26)+( 23 << 1),0x0f,False,False);
   AddReg3("MUL"   ,"MUL"   ,(31u << 26)+(107 << 1),0x08,True ,True );
   AddReg3("MULHW" ,"MULHW" ,(31u << 26)+( 75 << 1),0x0f,False,True );
   AddReg3("MULHWU","MULHWU",(31u << 26)+( 11 << 1),0x0f,False,True );
   AddReg3("MULLW" ,"MULS"  ,(31u << 26)+(235 << 1),0x0f,True ,True );
   AddReg3("STBUX" ,"STBUX" ,(31u << 26)+(247 << 1),0x0f,False,False);
   AddReg3("STBX"  ,"STBX"  ,(31u << 26)+(215 << 1),0x0f,False,False);
   AddReg3("STHBRX","STHBRX",(31u << 26)+(918 << 1),0x0f,False,False);
   AddReg3("STHUX" ,"STHUX" ,(31u << 26)+(439 << 1),0x0f,False,False);
   AddReg3("STHX"  ,"STHX"  ,(31u << 26)+(407 << 1),0x0f,False,False);
   AddReg3("STSWX" ,"STSX"  ,(31u << 26)+(661 << 1),0x0f,False,False);
   AddReg3("STWBRX","STBRX" ,(31u << 26)+(662 << 1),0x0f,False,False);
   AddReg3("STWCX.","STWCX.",(31u << 26)+(150 << 1),0x0f,False,False);
   AddReg3("STWUX" ,"STUX"  ,(31u << 26)+(183 << 1),0x0f,False,False);
   AddReg3("STWX"  ,"STX"   ,(31u << 26)+(151 << 1),0x0f,False,False);
   AddReg3("SUBF"  ,"SUBF"  ,(31u << 26)+( 40 << 1),0x0f,True ,True );
   AddReg3("SUB"   ,"SUB"   ,(31u << 26)+( 40 << 1),0x0f,True ,True );
   AddReg3("SUBFC" ,"SF"    ,(31u << 26)+(  8 << 1),0x0f,True ,True );
   AddReg3("SUBC"  ,"SUBC"  ,(31u << 26)+(  8 << 1),0x0f,True ,True );
   AddReg3("SUBFE" ,"SFE"   ,(31u << 26)+(136 << 1),0x0f,True ,True );

   /* cD,cA,cB --> D A B */

   CReg3Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*CReg3OrderCount); InstrZ=0;
   AddCReg3("CRAND"  ,(19u << 26)+(257 << 1),0x0f);
   AddCReg3("CRANDC" ,(19u << 26)+(129 << 1),0x0f);
   AddCReg3("CREQV"  ,(19u << 26)+(289 << 1),0x0f);
   AddCReg3("CRNAND" ,(19u << 26)+(225 << 1),0x0f);
   AddCReg3("CRNOR"  ,(19u << 26)+( 33 << 1),0x0f);
   AddCReg3("CROR"   ,(19u << 26)+(449 << 1),0x0f);
   AddCReg3("CRORC"  ,(19u << 26)+(417 << 1),0x0f);
   AddCReg3("CRXOR"  ,(19u << 26)+(193 << 1),0x0f);

   /* fD,fA,fB --> D A B */

   FReg3Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*FReg3OrderCount); InstrZ=0;
   AddFReg3("FADD"  ,"FA"    ,(63u << 26)+(21 << 1),0x0c,True );
   AddFReg3("FADDS" ,"FADDS" ,(59u << 26)+(21 << 1),0x0c,True );
   AddFReg3("FDIV"  ,"FD"    ,(63u << 26)+(18 << 1),0x0c,True );
   AddFReg3("FDIVS" ,"FDIVS" ,(59u << 26)+(18 << 1),0x0c,True );
   AddFReg3("FSUB"  ,"FS"    ,(63u << 26)+(20 << 1),0x0c,True );

   /* A,S,B --> S A B */

   Reg3SwapOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*Reg3SwapOrderCount); InstrZ=0;
   AddReg3Swap("AND"   ,"AND"   ,(31u << 26)+(  28 << 1),0x0f,True );
   AddReg3Swap("ANDC"  ,"ANDC"  ,(31u << 26)+(  60 << 1),0x0f,True );
   AddReg3Swap("ECOWX" ,"ECOWX" ,(31u << 26)+( 438 << 1),0x0c,False);
   AddReg3Swap("EQV"   ,"EQV"   ,(31u << 26)+( 284 << 1),0x0f,True );
   AddReg3Swap("MASKG" ,"MASKG" ,(31u << 26)+(  29 << 1),0x08,True );
   AddReg3Swap("MASKIR","MASKIR",(31u << 26)+( 541 << 1),0x08,True );
   AddReg3Swap("NAND"  ,"NAND"  ,(31u << 26)+( 476 << 1),0x0f,True );
   AddReg3Swap("NOR"   ,"NOR"   ,(31u << 26)+( 124 << 1),0x0f,True );
   AddReg3Swap("OR"    ,"OR"    ,(31u << 26)+( 444 << 1),0x0f,True );
   AddReg3Swap("ORC"   ,"ORC"   ,(31u << 26)+( 412 << 1),0x0f,True );
   AddReg3Swap("RRIB"  ,"RRIB"  ,(31u << 26)+( 537 << 1),0x08,True );
   AddReg3Swap("SLE"   ,"SLE"   ,(31u << 26)+( 153 << 1),0x08,True );
   AddReg3Swap("SLEQ"  ,"SLEQ"  ,(31u << 26)+( 217 << 1),0x08,True );
   AddReg3Swap("SLLQ"  ,"SLLQ"  ,(31u << 26)+( 216 << 1),0x08,True );
   AddReg3Swap("SLQ"   ,"SLQ"   ,(31u << 26)+( 152 << 1),0x08,True );
   AddReg3Swap("SLW"   ,"SL"    ,(31u << 26)+(  24 << 1),0x0f,True );
   AddReg3Swap("SRAQ"  ,"SRAQ"  ,(31u << 26)+( 920 << 1),0x08,True );
   AddReg3Swap("SRAW"  ,"SRA"   ,(31u << 26)+( 792 << 1),0x0f,True );
   AddReg3Swap("SRE"   ,"SRE"   ,(31u << 26)+( 665 << 1),0x08,True );
   AddReg3Swap("SREA"  ,"SREA"  ,(31u << 26)+( 921 << 1),0x08,True );
   AddReg3Swap("SREQ"  ,"SREQ"  ,(31u << 26)+( 729 << 1),0x08,True );
   AddReg3Swap("SRLQ"  ,"SRLQ"  ,(31u << 26)+( 728 << 1),0x08,True );
   AddReg3Swap("SRQ"   ,"SRQ"   ,(31u << 26)+( 664 << 1),0x08,True );
   AddReg3Swap("SRW"   ,"SR"    ,(31u << 26)+( 536 << 1),0x0f,True );
   AddReg3Swap("XOR"   ,"XOR"   ,(31u << 26)+( 316 << 1),0x0f,True );

   /* fD,A,B --> D A B */

   MixedOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*MixedOrderCount); InstrZ=0;
   AddMixed("LFDUX" ,"LFDUX" ,(31u << 26)+(631 << 1),0x0c);
   AddMixed("LFDX"  ,"LFDX"  ,(31u << 26)+(599 << 1),0x0c);
   AddMixed("LFSUX" ,"LFSUX" ,(31u << 26)+(567 << 1),0x0c);
   AddMixed("LFSX"  ,"LFSX"  ,(31u << 26)+(535 << 1),0x0c);
   AddMixed("STFDUX","STFDUX",(31u << 26)+(759 << 1),0x0c);
   AddMixed("STFDX" ,"STFDX" ,(31u << 26)+(727 << 1),0x0c);
   AddMixed("STFSUX","STFSUX",(31u << 26)+(695 << 1),0x0c);
   AddMixed("STFSX" ,"STFSX" ,(31u << 26)+(663 << 1),0x0c);

   /* fD,fA,fC,fB --> D A B C */

   FReg4Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*FReg4OrderCount); InstrZ=0;
   AddFReg4("FMADD"  ,"FMA"    ,(63u << 26)+(29 << 1),0x0c,True );
   AddFReg4("FMADDS" ,"FMADDS" ,(59u << 26)+(29 << 1),0x0c,True );
   AddFReg4("FMSUB"  ,"FMS"    ,(63u << 26)+(28 << 1),0x0c,True );
   AddFReg4("FMSUBS" ,"FMSUBS" ,(59u << 26)+(28 << 1),0x0c,True );
   AddFReg4("FNMADD" ,"FNMA"   ,(63u << 26)+(31 << 1),0x0c,True );
   AddFReg4("FNMADDS","FNMADDS",(59u << 26)+(31 << 1),0x0c,True );
   AddFReg4("FNMSUB" ,"FNMS"   ,(63u << 26)+(30 << 1),0x0c,True );
   AddFReg4("FNMSUBS","FNMSUBS",(59u << 26)+(30 << 1),0x0c,True );

   /* D,d(A) --> D A d */

   RegDispOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*RegDispOrderCount); InstrZ=0;
   AddRegDisp("LBZ"   ,"LBZ"   ,(34u << 26),0x0f);
   AddRegDisp("LBZU"  ,"LBZU"  ,(35u << 26),0x0f);
   AddRegDisp("LHA"   ,"LHA"   ,(42u << 26),0x0f);
   AddRegDisp("LHAU"  ,"LHAU"  ,(43u << 26),0x0f);
   AddRegDisp("LHZ"   ,"LHZ"   ,(40u << 26),0x0f);
   AddRegDisp("LHZU"  ,"LHZU"  ,(41u << 26),0x0f);
   AddRegDisp("LMW"   ,"LM"    ,(46u << 26),0x0f);
   AddRegDisp("LWZ"   ,"L"     ,(32u << 26),0x0f);
   AddRegDisp("LWZU"  ,"LU"    ,(33u << 26),0x0f);
   AddRegDisp("STB"   ,"STB"   ,(38u << 26),0x0f);
   AddRegDisp("STBU"  ,"STBU"  ,(39u << 26),0x0f);
   AddRegDisp("STH"   ,"STH"   ,(44u << 26),0x0f);
   AddRegDisp("STHU"  ,"STHU"  ,(45u << 26),0x0f);
   AddRegDisp("STMW"  ,"STM"   ,(47u << 26),0x0f);
   AddRegDisp("STW"   ,"ST"    ,(36u << 26),0x0f);
   AddRegDisp("STWU"  ,"STU"   ,(37u << 26),0x0f);

   /* fD,d(A) --> D A d */

   FRegDispOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*FRegDispOrderCount); InstrZ=0;
   AddFRegDisp("LFD"   ,"LFD"   ,(50u << 26),0x0c);
   AddFRegDisp("LFDU"  ,"LFDU"  ,(51u << 26),0x0c);
   AddFRegDisp("LFS"   ,"LFS"   ,(48u << 26),0x0c);
   AddFRegDisp("LFSU"  ,"LFSU"  ,(49u << 26),0x0c);
   AddFRegDisp("STFD"  ,"STFD"  ,(54u << 26),0x0c);
   AddFRegDisp("STFDU" ,"STFDU" ,(55u << 26),0x0c);
   AddFRegDisp("STFS"  ,"STFS"  ,(52u << 26),0x0c);
   AddFRegDisp("STFSU" ,"STFSU" ,(53u << 26),0x0c);

   /* A,S,Imm5 --> S A Imm */

   Reg2ImmOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*Reg2ImmOrderCount); InstrZ=0;
   AddReg2Imm("SLIQ"  ,"SLIQ"  ,(31u << 26)+(184 << 1),0x08,True);
   AddReg2Imm("SLLIQ" ,"SLLIQ" ,(31u << 26)+(248 << 1),0x08,True);
   AddReg2Imm("SRAIQ" ,"SRAIQ" ,(31u << 26)+(952 << 1),0x08,True);
   AddReg2Imm("SRAWI" ,"SRAI"  ,(31u << 26)+(824 << 1),0x0f,True);
   AddReg2Imm("SRIQ"  ,"SRIQ"  ,(31u << 26)+(696 << 1),0x08,True);
   AddReg2Imm("SRLIQ" ,"SRLIQ" ,(31u << 26)+(760 << 1),0x08,True);

   /* D,A,Imm --> D A Imm */

   Imm16Orders=(BaseOrder *) malloc(sizeof(BaseOrder)*Imm16OrderCount); InstrZ=0;
   AddImm16("ADDI"   ,"CAL"    ,14u << 26,0x0f);
   AddImm16("ADDIC"  ,"AI"     ,12u << 26,0x0f);
   AddImm16("ADDIC." ,"AI."    ,13u << 26,0x0f);
   AddImm16("ADDIS"  ,"CAU"    ,15u << 26,0x0f);
   AddImm16("DOZI"   ,"DOZI"   , 9u << 26,0x08);
   AddImm16("MULLI"  ,"MULI"   , 7u << 26,0x0f);
   AddImm16("SUBFIC" ,"SFI"    , 8u << 26,0x0c);

   /* A,S,Imm --> S A Imm */

   Imm16SwapOrders=(BaseOrder *) malloc(sizeof(BaseOrder)*Imm16SwapOrderCount); InstrZ=0;
   AddImm16Swap("ANDI."  ,"ANDIL." ,28u << 26,0x0f);
   AddImm16Swap("ANDIS." ,"ANDIU." ,29u << 26,0x0f);
   AddImm16Swap("ORI"    ,"ORIL"   ,24u << 26,0x0f);
   AddImm16Swap("ORIS"   ,"ORIU"   ,25u << 26,0x0f);
   AddImm16Swap("XORI"   ,"XORIL"  ,26u << 26,0x0f);
   AddImm16Swap("XORIS"  ,"XORIU"  ,27u << 26,0x0f);
END

	static void DeinitNames(BaseOrder *Orders, int OrderCount)
BEGIN
   int z;

   for (z=0; z<OrderCount; free(Orders[z++].Name));
END

	static void DeinitFields(void)
BEGIN
   free(FixedOrders);
   free(Reg1Orders);
   free(FReg1Orders);
   free(CReg1Orders);
   free(CBit1Orders);
   DeinitNames(Reg2Orders,Reg2OrderCount); free(Reg2Orders);
   free(CReg2Orders);
   DeinitNames(FReg2Orders,FReg2OrderCount); free(FReg2Orders);
   free(Reg2BOrders);
   DeinitNames(Reg2SwapOrders,Reg2SwapOrderCount); free(Reg2SwapOrders);
   free(NoDestOrders);
   DeinitNames(Reg3Orders,Reg3OrderCount); free(Reg3Orders);
   free(CReg3Orders);
   DeinitNames(FReg3Orders,FReg3OrderCount); free(FReg3Orders);
   DeinitNames(Reg3SwapOrders,Reg3SwapOrderCount); free(Reg3SwapOrders);
   free(MixedOrders);
   DeinitNames(FReg4Orders,FReg4OrderCount); free(FReg4Orders);
   free(RegDispOrders);
   free(FRegDispOrders);
   DeinitNames(Reg2ImmOrders,Reg2ImmOrderCount); free(Reg2ImmOrders);
   free(Imm16Orders);
   free(Imm16SwapOrders);
END

/*-------------------------------------------------------------------------*/

        static void PutCode(LongWord Code)
BEGIN
   memcpy(BAsmCode,&Code,4);
   if (NOT BigEndian) DSwap((void *)BAsmCode,4);
END

        static void IncCode(LongWord Code)
BEGIN
   BAsmCode[0]+=(Code >> 24) & 0xff;
   BAsmCode[1]+=(Code >> 16) & 0xff;
   BAsmCode[2]+=(Code >>  8) & 0xff;
   BAsmCode[3]+=(Code       ) & 0xff;
END

/*-------------------------------------------------------------------------*/

        static Boolean DecodeGenReg(char *Asc, LongWord *Erg)
BEGIN
   Boolean io;

   if ((strlen(Asc)<2) OR (toupper(*Asc)!='R')) return False;
   else
    BEGIN
     *Erg=ConstLongInt(Asc+1,&io);
     return ((io) AND (*Erg<=31));
    END
END

	static Boolean DecodeFPReg(char *Asc, LongWord *Erg)
BEGIN
   Boolean io;

   if ((strlen(Asc)<3) OR (toupper(*Asc)!='F') OR (toupper(Asc[1])!='R')) return False;
   else
    BEGIN
     *Erg=ConstLongInt(Asc+2,&io);
     return ((io) AND (*Erg<=31));
    END
END

	static Boolean DecodeCondReg(char *Asc, LongWord *Erg)
BEGIN
   Boolean OK;

   *Erg=EvalIntExpression(Asc,UInt3,&OK) << 2;
   return ((OK) AND (*Erg<=31));
END

	static Boolean  DecodeCondBit(char *Asc, LongWord *Erg)
BEGIN
   Boolean OK;

   *Erg=EvalIntExpression(Asc,UInt5,&OK);
   return ((OK) AND (*Erg<=31));
END

	static Boolean DecodeRegDisp(char *Asc, LongWord *Erg)
BEGIN
   char *p;
   int l=strlen(Asc);
   Integer Disp;
   Boolean OK;

   if (Asc[l-1]!=')') return False; Asc[l-1]='\0';  l--;
   p=Asc+l-1;  while ((p>=Asc) AND (*p!='(')) p--;
   if (p<Asc) return False;
   if (NOT DecodeGenReg(p+1,Erg)) return False;
   *p='\0';
   Disp=EvalIntExpression(Asc,Int16,&OK);  if (NOT OK) return False;
   *Erg=(*Erg << 16)+(Disp & 0xffff);  return True;
END

/*-------------------------------------------------------------------------*/

	static Boolean Convert6000(char *Name1, char *Name2)
BEGIN
   if (NOT Memo(Name1)) return True;
   if (MomCPU==CPU6000)
    BEGIN
     strmaxcpy(OpPart,Name2,255);
     return True;
    END
   else
    BEGIN
     WrError(1200); return False;
    END
END

	static Boolean PMemo(char *Name)
BEGIN
   String tmp;

   if (Memo(Name)) return True;   

   strmaxcpy(tmp,Name,255); strmaxcat(tmp,".",255);
   return (Memo(tmp));
END

	static void IncPoint(void)
BEGIN
   if (OpPart[strlen(OpPart)-1]=='.') IncCode(1);
END

	static void ChkSup(void)
BEGIN
   if (NOT SupAllowed) WrError(50);
END

	static Boolean ChkCPU(Byte Mask)
BEGIN
   return (((Mask >> (MomCPU-CPU403))&1)==1);
END

/*-------------------------------------------------------------------------*/

	static Boolean DecodePseudo(void)
BEGIN
#define ONOFF601Count 2
   static ONOFFRec ONOFF601s[ONOFF601Count]=
             {{"SUPMODE", &SupAllowed, SupAllowedName},
              {"BIGENDIAN", &BigEnd, BigEndianName}};

   if (CodeONOFF(ONOFF601s,ONOFF601Count)) return True;

   return False;
END

	static void SwapCode(LongWord *Code)
BEGIN
   *Code=((*Code & 0x1f) << 5) | ((*Code >> 5) & 0x1f);
END

	static void MakeCode_601(void)
BEGIN
   Integer z,Imm;
   LongWord Dest,Src1,Src2,Src3;
   LongInt Dist;
   Boolean OK;

   CodeLen=0; DontPrint=False;

   /* Nullanweisung */

   if ((Memo("")) AND (*AttrPart=='\0') AND (ArgCnt==0)) return;

   /* Pseudoanweisungen */

   if (DecodePseudo()) return;

   if (DecodeIntelPseudo(BigEnd)) return;

   /* ohne Argument */

   for (z=0; z<FixedOrderCount; z++)
    if (Memo(FixedOrders[z].Name))
     BEGIN
      if (ArgCnt!=0) WrError(1110);
      else if (NOT ChkCPU(FixedOrders[z].CPUMask)) WrXError(1500,OpPart);
      else
       BEGIN
        CodeLen=4; PutCode(FixedOrders[z].Code);
        if (Memo("RFI")) ChkSup();
       END
      return;
     END

   /* ein Register */

   for (z=0; z<Reg1OrderCount; z++)
    if Memo(Reg1Orders[z].Name)
     BEGIN
      if (ArgCnt!=1) WrError(1110);
      else if (NOT ChkCPU(Reg1Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(Reg1Orders[z].Code+(Dest << 21));
        if (Memo("MTMSR")) ChkSup();
       END
      return;
     END

   /* ein Steuerregister */

   for (z=1; z<CReg1OrderCount; z++)
    if (Memo(CReg1Orders[z].Name))
     BEGIN
      if (ArgCnt!=1) WrError(1110);
      else if (NOT ChkCPU(CReg1Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeCondReg(ArgStr[1],&Dest)) WrError(1350);
      else if ((Dest & 3)!=0) WrError(1351);
      else
       BEGIN
        CodeLen=4; PutCode(CReg1Orders[z].Code+(Dest << 21));
       END
      return;
     END

   /* ein Steuerregisterbit */

   for (z=0; z<CBit1OrderCount; z++)
    if (Memo(CBit1Orders[z].Name))
     BEGIN
      if (ArgCnt!=1) WrError(1110);
      else if (NOT ChkCPU(CBit1Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeCondBit(ArgStr[1],&Dest)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(CBit1Orders[z].Code+(Dest << 21));
       END
      return;
     END

   /* ein Gleitkommaregister */

   for (z=0; z<FReg1OrderCount; z++)
    if (Memo(FReg1Orders[z].Name))
     BEGIN
      if (ArgCnt!=1) WrError(1110);
      else if (NOT ChkCPU(FReg1Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeFPReg(ArgStr[1],&Dest)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(FReg1Orders[z].Code+(Dest << 21));
       END
      return;
     END

   /* 1/2 Integer-Register */

   for (z=0; z<Reg2OrderCount; z++)
    if (Memo(Reg2Orders[z].Name))
     BEGIN
      if (ArgCnt==1)
       BEGIN
        ArgCnt=2; strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=2) WrError(1110);
      else if (NOT ChkCPU(Reg2Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(Reg2Orders[z].Code+(Dest << 21)+(Src1 << 16));
       END
      return;
     END

   /* 2 Bedingungs-Bits */

   for (z=0; z<CReg2OrderCount; z++)
    if Memo(CReg2Orders[z].Name)
     BEGIN
      if (ArgCnt!=2) WrError(1110);
      else if (NOT ChkCPU(CReg2Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeCondReg(ArgStr[1],&Dest)) WrError(1350);
      else if ((Dest & 3)!=0) WrError(1351);
      else if (NOT DecodeCondReg(ArgStr[2],&Src1)) WrError(1350);
      else if ((Src1 & 3)!=0) WrError(1351);
      else
       BEGIN
        CodeLen=4; PutCode(CReg2Orders[z].Code+(Dest << 21)+(Src1 << 16));
       END
      return;
     END

   /* 1/2 Float-Register */

   for (z=0; z<FReg2OrderCount; z++)
    if Memo(FReg2Orders[z].Name)
     BEGIN
      if (ArgCnt==1)
       BEGIN
        ArgCnt=2; strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=2) WrError(1110);
      else if (NOT ChkCPU(FReg2Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeFPReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeFPReg(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(FReg2Orders[z].Code+(Dest << 21)+(Src1 << 11));
       END
      return;
     END

   /* 1/2 Integer-Register, Quelle in B */

   for (z=0; z<Reg2BOrderCount; z++)
    if Memo(Reg2BOrders[z].Name)
     BEGIN
      if (ArgCnt==1)
       BEGIN
        ArgCnt=2; strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=2) WrError(1110);
      else if (NOT ChkCPU(Reg2BOrders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(Reg2BOrders[z].Code+(Dest << 21)+(Src1 << 11));
        ChkSup();
       END
      return;
     END

   /* 1/2 Integer-Register, getauscht */

   for (z=0; z<Reg2SwapOrderCount; z++)
    if (Memo(Reg2SwapOrders[z].Name))
     BEGIN
      if (ArgCnt==1)
       BEGIN
        ArgCnt=2; strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=2) WrError(1110);
      else if (NOT ChkCPU(Reg2SwapOrders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(Reg2SwapOrders[z].Code+(Dest << 16)+(Src1 << 21));
       END
      return;
     END

   /* 2 Integer-Register, kein Ziel */

   for (z=0; z<NoDestOrderCount; z++)
    if (Memo(NoDestOrders[z].Name))
      BEGIN
       if (ArgCnt!=2) WrError(1110);
       else if (NOT ChkCPU(NoDestOrders[z].CPUMask)) WrXError(1500,OpPart);
       else if (NOT DecodeGenReg(ArgStr[1],&Src1)) WrError(1350);
       else if (NOT DecodeGenReg(ArgStr[2],&Src2)) WrError(1350);
       else
	BEGIN
         CodeLen=4; PutCode(NoDestOrders[z].Code+(Src1 << 16)+(Src2 << 11));
	END
       return;
      END

   /* 2/3 Integer-Register */

   for (z=0; z<Reg3OrderCount; z++)
    if (Memo(Reg3Orders[z].Name))
     BEGIN
      if (ArgCnt==2)
       BEGIN
        ArgCnt=3; strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(Reg3Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[3],&Src2)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(Reg3Orders[z].Code+(Dest << 21)+(Src1 << 16)+(Src2 << 11));
       END
      return;
     END

   /* 2/3 Bedingungs-Bits */

   for (z=0; z<CReg3OrderCount; z++)
    if (Memo(CReg3Orders[z].Name))
     BEGIN
      if (ArgCnt==2)
       BEGIN
        ArgCnt=3; strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(CReg3Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeCondBit(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeCondBit(ArgStr[2],&Src1)) WrError(1350);
      else if (NOT DecodeCondBit(ArgStr[3],&Src2)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(CReg3Orders[z].Code+(Dest << 21)+(Src1 << 16)+(Src2 << 11));
       END
      return;
     END

   /* 2/3 Float-Register */

   for (z=0; z<FReg3OrderCount; z++)
    if (Memo(FReg3Orders[z].Name))
     BEGIN
      if (ArgCnt==2)
       BEGIN
        ArgCnt=3; strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(FReg3Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeFPReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeFPReg(ArgStr[2],&Src1)) WrError(1350);
      else if (NOT DecodeFPReg(ArgStr[3],&Src2)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(FReg3Orders[z].Code+(Dest << 21)+(Src1 << 16)+(Src2 << 11));
       END
      return;
     END

   /* 2/3 Integer-Register, Ziel & Quelle 1 getauscht */

   for (z=0; z<Reg3SwapOrderCount; z++)
    if (Memo(Reg3SwapOrders[z].Name))
     BEGIN
      if (ArgCnt==2)
       BEGIN
        ArgCnt=3; strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(Reg3SwapOrders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[3],&Src2)) WrError(1350);
      else
       BEGIN
        CodeLen=4; PutCode(Reg3SwapOrders[z].Code+(Dest << 16)+(Src1 << 21)+(Src2 << 11));
       END
      return;
     END

   /* 1 Float und 2 Integer-Register */

   for (z=0; z<MixedOrderCount; z++)
    if (Memo(MixedOrders[z].Name))
     BEGIN
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(MixedOrders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeFPReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[3],&Src2)) WrError(1350);
      else
	BEGIN
         CodeLen=4; PutCode(MixedOrders[z].Code+(Dest << 21)+(Src1 << 16)+(Src2 << 11));
	END
       return;
      END

   /* 3/4 Float-Register */

   for (z=0; z<FReg4OrderCount; z++)
    if (Memo(FReg4Orders[z].Name))
     BEGIN
      if (ArgCnt==3)
       BEGIN
        ArgCnt=4; strcpy(ArgStr[4],ArgStr[3]);
        strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=4) WrError(1110);
      else if (NOT ChkCPU(FReg4Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeFPReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeFPReg(ArgStr[2],&Src1)) WrError(1350);
      else if (NOT DecodeFPReg(ArgStr[3],&Src3)) WrError(1350);
      else if (NOT DecodeFPReg(ArgStr[4],&Src2)) WrError(1350);
      else
       BEGIN
        CodeLen=4;
        PutCode(FReg4Orders[z].Code+(Dest << 21)+(Src1 << 16)+(Src2 << 11)+(Src3 << 6));
       END
      return;
     END

   /* Register mit indiziertem Speicheroperandem */

   for (z=0; z<RegDispOrderCount; z++)
    if (Memo(RegDispOrders[z].Name))
     BEGIN
      if (ArgCnt!=2) WrError(1110);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeRegDisp(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
        PutCode(RegDispOrders[z].Code+(Dest << 21)+Src1); CodeLen=4;
       END
      return;
     END

   /* Gleitkommaregister mit indiziertem Speicheroperandem */

   for (z=0; z<FRegDispOrderCount; z++)
    if (Memo(FRegDispOrders[z].Name))
     BEGIN
      if (ArgCnt!=2) WrError(1110);
      else if (NOT DecodeFPReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeRegDisp(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
        PutCode(FRegDispOrders[z].Code+(Dest << 21)+Src1); CodeLen=4;
       END
      return;
     END

   /* 2 verdrehte Register mit immediate */

   for (z=0; z<Reg2ImmOrderCount; z++)
    if (Memo(Reg2ImmOrders[z].Name))
     BEGIN
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(Reg2ImmOrders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
	Src2=EvalIntExpression(ArgStr[3],UInt5,&OK);
	if (OK)
	 BEGIN
          PutCode(Reg2ImmOrders[z].Code+(Src1 << 21)+(Dest << 16)+(Src2 << 11));
	  CodeLen=4;
	 END
       END
      return;
     END

   /* 2 Register+immediate */

   for (z=0; z<Imm16OrderCount; z++)
    if (Memo(Imm16Orders[z].Name))
     BEGIN
      if (ArgCnt==2)
       BEGIN
	ArgCnt=3; strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(Imm16Orders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
	Imm=EvalIntExpression(ArgStr[3],Int16,&OK);
	if (OK)
	 BEGIN
          CodeLen=4; PutCode(Imm16Orders[z].Code+(Dest << 21)+(Src1 << 16)+(Imm AND 0xffff));
	 END
       END
      return;
     END

   /* 2 Register+immediate, Ziel & Quelle 1 getauscht */

   for (z=0; z<Imm16SwapOrderCount; z++)
    if (Memo(Imm16SwapOrders[z].Name))
     BEGIN
      if (ArgCnt==2)
       BEGIN
	ArgCnt=3; strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]);
       END
      if (ArgCnt!=3) WrError(1110);
      else if (NOT ChkCPU(Imm16SwapOrders[z].CPUMask)) WrXError(1500,OpPart);
      else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
      else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
      else
       BEGIN
	Imm=EvalIntExpression(ArgStr[3],Int16,&OK);
	if (OK)
	 BEGIN
          CodeLen=4; PutCode(Imm16SwapOrders[z].Code+(Dest << 16)+(Src1 << 21)+(Imm AND 0xffff));
	 END
       END
      return;
     END

   /* Ausreisser... */

   if (NOT Convert6000("FM","FMUL")) return;
   if (NOT Convert6000("FM.","FMUL.")) return;

   if ((PMemo("FMUL")) OR (PMemo("FMULS")))
    BEGIN
     if (ArgCnt==2)
      BEGIN
       strcpy(ArgStr[3],ArgStr[2]); strcpy(ArgStr[2],ArgStr[1]); ArgCnt=3;
      END
     if (ArgCnt!=3) WrError(1110);
     else if (NOT DecodeFPReg(ArgStr[1],&Dest)) WrError(1350);
     else if (NOT DecodeFPReg(ArgStr[2],&Src1)) WrError(1350);
     else if (NOT DecodeFPReg(ArgStr[3],&Src2)) WrError(1350);
     else
      BEGIN
       PutCode((59u << 26)+(25 << 1)+(Dest << 21)+(Src1 << 16)+(Src2 << 6));
       if (PMemo("FMUL")) IncCode(4u << 26);
       IncPoint();
       CodeLen=4;
      END
     return;
    END

   if (NOT Convert6000("LSI","LSWI")) return;
   if (NOT Convert6000("STSI","STSWI")) return;

   if ((Memo("LSWI")) OR (Memo("STSWI")))
    BEGIN
     if (ArgCnt!=3) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
     else
      BEGIN
       Src2=EvalIntExpression(ArgStr[3],UInt5,&OK);
       if (OK)
	BEGIN
         PutCode((31u << 26)+(597 << 1)+(Dest << 21)+(Src1 << 16)+(Src2 << 11));
         if (Memo("STSWI")) IncCode(128 << 1);
	 CodeLen=4;
	END
      END
     return;
    END

   if ((Memo("MFSPR")) OR (Memo("MTSPR")))
    BEGIN
     if (Memo("MTSPR"))
      BEGIN
       strcpy(ArgStr[3],ArgStr[1]); strcpy(ArgStr[1],ArgStr[2]); strcpy(ArgStr[2],ArgStr[3]);
      END
     if (ArgCnt!=2) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
     else
      BEGIN
       Src1=EvalIntExpression(ArgStr[2],UInt10,&OK);
       if (OK)
        BEGIN
         SwapCode(&Src1);
         PutCode((31u << 26)+(Dest << 21)+(Src1 << 11));
         IncCode((Memo("MFSPR") ? 339 : 467) << 1);
         CodeLen=4;
        END
      END
     return;
    END

   if ((Memo("MFDCR")) OR (Memo("MTDCR")))
    BEGIN
     if (Memo("MTDCR"))
      BEGIN
       strcpy(ArgStr[3],ArgStr[1]); strcpy(ArgStr[1],ArgStr[2]); strcpy(ArgStr[2],ArgStr[3]);
      END
     if (ArgCnt!=2) WrError(1110);
     else if (MomCPU!=CPU403) WrXError(1500,OpPart);
     else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
     else
      BEGIN
       Src1=EvalIntExpression(ArgStr[2],UInt10,&OK);
       if (OK)
        BEGIN
         SwapCode(&Src1);
         PutCode((31u << 26)+(Dest << 21)+(Src1 << 11));
         IncCode((Memo("MFDCR") ? 323 : 451) << 1);
         CodeLen=4;
        END
      END
     return;
    END

   if ((Memo("MFSR")) OR (Memo("MTSR")))
    BEGIN
     if (Memo("MTSR"))
      BEGIN
       strcpy(ArgStr[3],ArgStr[1]); strcpy(ArgStr[1],ArgStr[2]); strcpy(ArgStr[2],ArgStr[3]);
      END
     if (ArgCnt!=2) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
     else
      BEGIN
       Src1=EvalIntExpression(ArgStr[2],UInt4,&OK);
       if (OK)
	BEGIN
         PutCode((31u << 26)+(Dest << 21)+(Src1 << 16));
         IncCode((Memo("MFSR") ? 595 : 210) << 1);
	 CodeLen=4; ChkSup();
	END
      END
     return;
    END

   if (Memo("MTCRF"))
    BEGIN
     if (ArgCnt!=2) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
     else
      BEGIN
       Dest=EvalIntExpression(ArgStr[1],UInt9,&OK);
       if (OK)
	if ((Dest&1)==1) WrError(1351);
	else
	 BEGIN
          PutCode((31u << 26)+(Src1 << 26)+(Dest << 11)+(144 << 1));
	  CodeLen=4;
	 END
      END
     return;
    END

   if (PMemo("MTFSF"))
    BEGIN
     if (ArgCnt!=2) WrError(1110);
     else if (NOT DecodeFPReg(ArgStr[2],&Src1)) WrError(1350);
     else
      BEGIN
       Dest=EvalIntExpression(ArgStr[1],UInt8,&OK);
       if (OK)
	BEGIN
         PutCode((63u << 26)+(Dest << 17)+(Src1 << 11)+(711 << 1));
	 IncPoint();
	 CodeLen=4;
	END
      END
     return;
    END

   if (PMemo("MTFSFI"))
    BEGIN
     if (ArgCnt!=2) WrError(1110);
     else if (NOT DecodeCondReg(ArgStr[1],&Dest)) WrError(1350);
     else if ((Dest & 3)!=0) WrError(1351);
     else
      BEGIN
       Src1=EvalIntExpression(ArgStr[2],UInt4,&OK);
       if (OK)
	BEGIN
         PutCode((63u << 26)+(Dest << 21)+(Src1 << 12)+(134 << 1));
	 IncPoint();
	 CodeLen=4;
	END
      END
     return;
    END

   if (PMemo("RLMI"))
    BEGIN
     if (ArgCnt!=5) WrError(1110);
     else if (MomCPU<CPU6000) WrXError(1500,OpPart);
     else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[3],&Src2)) WrError(1350);
     else
      BEGIN
       Src3=EvalIntExpression(ArgStr[4],UInt5,&OK);
       if (OK)
	BEGIN
	 Imm=EvalIntExpression(ArgStr[5],UInt5,&OK);
	 if (OK)
	  BEGIN
           PutCode((22u << 26)+(Src1 << 21)+(Dest << 16)
                       +(Src2 << 11)+(Src3 << 6)+(Imm << 1));
	   IncPoint();
	   CodeLen=4;
	  END
	END
      END
     return;
    END

   if (NOT Convert6000("RLNM","RLWNM")) return;
   if (NOT Convert6000("RLNM.","RLWNM.")) return;

   if (PMemo("RLWNM"))
    BEGIN
     if (ArgCnt!=5) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[3],&Src2)) WrError(1350);
     else
      BEGIN
       Src3=EvalIntExpression(ArgStr[4],UInt5,&OK);
       if (OK)
	BEGIN
	 Imm=EvalIntExpression(ArgStr[5],UInt5,&OK);
	 if (OK)
	  BEGIN
           PutCode((23u << 26)+(Src1 << 21)+(Dest << 16)
                       +(Src2 << 11)+(Src3 << 6)+(Imm << 1));
	   IncPoint();
	   CodeLen=4;
	  END
	END
      END
     return;
    END

   if (NOT Convert6000("RLIMI","RLWIMI")) return;
   if (NOT Convert6000("RLIMI.","RLWIMI.")) return;
   if (NOT Convert6000("RLINM","RLWINM")) return;
   if (NOT Convert6000("RLINM.","RLWINM.")) return;

   if ((PMemo("RLWIMI")) OR (PMemo("RLWINM")))
    BEGIN
     if (ArgCnt!=5) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[1],&Dest)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
     else
      BEGIN
       Src2=EvalIntExpression(ArgStr[3],UInt5,&OK);
       if (OK)
	BEGIN
	 Src3=EvalIntExpression(ArgStr[4],UInt5,&OK);
	 if (OK)
	  BEGIN
	   Imm=EvalIntExpression(ArgStr[5],UInt5,&OK);
	   if (OK)
	    BEGIN
             PutCode((20u << 26)+(Dest << 16)+(Src1 << 21)
                         +(Src2 << 11)+(Src3 << 6)+(Imm << 1));
             if (PMemo("RLWINM")) IncCode(1u << 26);
	     IncPoint();
	     CodeLen=4;
	    END
	  END
	END
      END
     return;
    END

   if (NOT Convert6000("TLBI","TLBIE")) return;

   if (Memo("TLBIE"))
    BEGIN
     if (ArgCnt!=1) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[1],&Src1)) WrError(1350);
     else
      BEGIN
       PutCode((31u << 26)+(Src1 << 11)+(306 << 1));
       CodeLen=4; ChkSup();
      END
     return;
    END

   if (NOT Convert6000("T","TW")) return;

   if (Memo("TW"))
    BEGIN
     if (ArgCnt!=3) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[3],&Src2)) WrError(1350);
     else
      BEGIN
       Dest=EvalIntExpression(ArgStr[1],UInt5,&OK);
       if (OK)
	BEGIN
         PutCode((31u << 26)+(Dest << 21)+(Src1 << 16)+(Src2 << 11)+(4 << 1));
	 CodeLen=4;
	END
      END
     return;
    END

   if (NOT Convert6000("TI","TWI")) return;

   if (Memo("TWI"))
    BEGIN
     if (ArgCnt!=3) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[2],&Src1)) WrError(1350);
     else
      BEGIN
       Imm=EvalIntExpression(ArgStr[3],Int16,&OK);
       if (OK)
	BEGIN
	 Dest=EvalIntExpression(ArgStr[1],UInt5,&OK);
	 if (OK)
	  BEGIN
           PutCode((3u << 26)+(Dest << 21)+(Src1 << 16)+(Imm & 0xffff));
	   CodeLen=4;
	  END
	END
      END
     return;
    END

   if (Memo("WRTEEI"))
    BEGIN
     if (ArgCnt!=1) WrError(1110);
     else if (MomCPU!=CPU403) WrXError(1500,OpPart);
     else
      BEGIN
       Src1=EvalIntExpression(ArgStr[1],UInt1,&OK) << 15;
       if (OK)
        BEGIN
         PutCode((31u << 26)+Src1+(163 << 1));
         CodeLen=4;
        END
      END
     return;
    END

   /* Vergleiche */

   if ((Memo("CMP")) OR (Memo("CMPL")))
    BEGIN
     if (ArgCnt==3)
      BEGIN
       strcpy(ArgStr[4],ArgStr[3]); strcpy(ArgStr[3],ArgStr[2]); strmaxcpy(ArgStr[2],"0",255); ArgCnt=4;
      END
     if (ArgCnt!=4) WrError(1110);
     else if (NOT DecodeGenReg(ArgStr[4],&Src2)) WrError(1350);
     else if (NOT DecodeGenReg(ArgStr[3],&Src1)) WrError(1350);
     else if (NOT DecodeCondReg(ArgStr[1],&Dest)) WrError(1350);
     else if ((Dest & 3)!=0) WrError(1351);
     else
      BEGIN
       Src3=EvalIntExpression(ArgStr[2],UInt1,&OK);
       if (OK)
	BEGIN
         PutCode((31u << 26)+(Dest << 21)+(Src3 << 21)+(Src1 << 16)
                     +(Src2 << 11));
         if (Memo("CMPL")) IncCode(32 << 1);
	 CodeLen=4;
	END
      END
     return;
    END

   if ((Memo("FCMPO")) OR (Memo("FCMPU")))
    BEGIN
     if (ArgCnt!=3) WrError(1110);
     else if (NOT DecodeFPReg(ArgStr[3],&Src2)) WrError(1350);
     else if (NOT DecodeFPReg(ArgStr[2],&Src1)) WrError(1350);
     else if (NOT DecodeCondReg(ArgStr[1],&Dest)) WrError(1350);
     else if ((Dest & 3)!=0) WrError(1351);
     else
      BEGIN
       PutCode((63u << 26)+(Dest << 21)+(Src1 << 16)+(Src2 << 11));
       if (Memo("FCMPO")) IncCode(32 << 1);
       CodeLen=4;
      END
     return;
    END

   if ((Memo("CMPI")) OR (Memo("CMPLI")))
    BEGIN
     if (ArgCnt==3)
      BEGIN
       strcpy(ArgStr[4],ArgStr[3]); strcpy(ArgStr[3],ArgStr[2]); strmaxcpy(ArgStr[2],"0",255); ArgCnt=4;
      END
     if (ArgCnt!=4) WrError(1110);
     else
      BEGIN
       Src2=EvalIntExpression(ArgStr[4],Int16,&OK);
       if (OK)
	if (NOT DecodeGenReg(ArgStr[3],&Src1)) WrError(1350);
	else if (NOT DecodeCondReg(ArgStr[1],&Dest)) WrError(1350);
	else if ((Dest & 3)!=0) WrError(1351);
	else
	 BEGIN
	  Src3=EvalIntExpression(ArgStr[2],UInt1,&OK);
	  if (OK)
	   BEGIN
            PutCode((10u << 26)+(Dest << 21)+(Src3 << 21)
                        +(Src1 << 16)+(Src2 AND 0xffff));
            if (Memo("CMPI")) IncCode(1u << 26);
	    CodeLen=4;
	   END
	 END
      END
     return;
    END

   /* Spruenge */

   if ((Memo("B")) OR (Memo("BL")) OR (Memo("BA")) OR (Memo("BLA")))
    BEGIN
     if (ArgCnt!=1) WrError(1110);
     else
      BEGIN
       Dist=EvalIntExpression(ArgStr[1],Int32,&OK);
       if (OK)
	BEGIN
	 if ((Memo("B")) OR (Memo("BL"))) Dist-=EProgCounter();
	 if ((NOT SymbolQuestionable) AND (Dest>0x1ffffff)) WrError(1320);
	 else if ((NOT SymbolQuestionable) AND (Dist<-0x2000000)) WrError(1315);
	 else if ((Dist & 3)!=0) WrError(1375);
	 else
	  BEGIN
           PutCode((18u << 26)+(Dist & 0x03fffffc));
           if ((Memo("BA")) OR (Memo("BLA"))) IncCode(2);
           if ((Memo("BL")) OR (Memo("BLA"))) IncCode(1);
	   CodeLen=4;
	  END
	END
      END
     return;
    END

   if ((Memo("BC")) OR (Memo("BCL")) OR (Memo("BCA")) OR (Memo("BCLA")))
    BEGIN
     if (ArgCnt!=3) WrError(1110);
     else
      BEGIN
       Src1=EvalIntExpression(ArgStr[1],UInt5,&OK); /* BO */
       if (OK)
	BEGIN
         Src2=EvalIntExpression(ArgStr[2],UInt5,&OK); /* BI */
	 if (OK)
	  BEGIN
           Dist=EvalIntExpression(ArgStr[3],Int32,&OK); /* ADR */
	   if (OK)
	    BEGIN
	     if ((Memo("BC")) OR (Memo("BCL"))) Dist-=EProgCounter();
	     if ((NOT SymbolQuestionable) AND (Dist>0x7fff)) WrError(1320);
	     else if ((NOT SymbolQuestionable) AND (Dist<-0x8000)) WrError(1315);
	     else if ((Dist & 3)!=0) WrError(1375);
	     else
	      BEGIN
               PutCode((16u << 26)+(Src1 << 21)+(Src2 << 16)+(Dist & 0xfffc));
               if ((Memo("BCA")) OR (Memo("BCLA"))) IncCode(2);
               if ((Memo("BCL")) OR (Memo("BCLA"))) IncCode(1);
	       CodeLen=4;
	      END
	    END
	  END
	END
      END
     return;
    END

   if (NOT Convert6000("BCC","BCCTR")) return;
   if (NOT Convert6000("BCCL","BCCTRL")) return;
   if (NOT Convert6000("BCR","BCLR")) return;
   if (NOT Convert6000("BCRL","BCLRL")) return;

   if ((Memo("BCCTR")) OR (Memo("BCCTRL")) OR (Memo("BCLR")) OR (Memo("BCLRL")))
    BEGIN
     if (ArgCnt!=2) WrError(1110);
     else
      BEGIN
       Src1=EvalIntExpression(ArgStr[1],UInt5,&OK);
       if (OK)
	BEGIN
	 Src2=EvalIntExpression(ArgStr[2],UInt5,&OK);
	 if (OK)
	  BEGIN
           PutCode((19u << 26)+(Src1 << 21)+(Src2 << 16));
	   if ((Memo("BCCTR")) OR (Memo("BCCTRL")))
            IncCode(528 << 1);
	   else
            IncCode(16 << 1);
           if ((Memo("BCCTRL")) OR (Memo("BCLRL"))) IncCode(1);
	   CodeLen=4;
	  END
	END
      END
     return;
    END

   /* unbekannter Befehl */

   WrXError(1200,OpPart);
END

	static Boolean ChkPC_601(void)
BEGIN
#ifdef HAS64
   return ((ActPC==SegCode) AND (ProgCounter()<=0xffffffffll));
#else
   return (ActPC==SegCode);
#endif
END

	static Boolean IsDef_601(void)
BEGIN
   return False;
END

        static void InitPass_601(void)
BEGIN
   SaveInitProc();
   SetFlag(&BigEnd,BigEndianName,False);
END

	static void InternSymbol_601(char *Asc, TempResult *Erg)
BEGIN
   int l=strlen(Asc);

   Erg->Typ=TempNone;
   if ((l==3) OR (l==4))
    if ((toupper(*Asc)=='C') AND (toupper(Asc[1])=='R'))
     if ((Asc[l-1]>='0') AND (Asc[l-1]<='7'))
      if ((l==3) != ((toupper(Asc[2])=='F') OR (toupper(Asc[3])=='B')))
       BEGIN
        Erg->Typ=TempInt; Erg->Contents.Int=Asc[l-1]-'0';
       END
END

	static void SwitchFrom_601(void)
BEGIN
   DeinitFields();
END

	static void SwitchTo_601(void)
BEGIN
   TurnWords=False; ConstMode=ConstModeC; SetIsOccupied=False;

   PCSymbol="*"; HeaderID=0x05; NOPCode=0x000000000;
   DivideChars=","; HasAttrs=False;

   ValidSegs=(1<<SegCode);
   Grans[SegCode]=1; ListGrans[SegCode]=1; SegInits[SegCode]=0;

   MakeCode=MakeCode_601; ChkPC=ChkPC_601; IsDef=IsDef_601;
   SwitchFrom=SwitchFrom_601; InternSymbol=InternSymbol_601;

   InitFields();
END


	void code601_init(void)
BEGIN
   CPU403 =AddCPU("PPC403",SwitchTo_601);
   CPU505 =AddCPU("MPC505",SwitchTo_601);
   CPU601 =AddCPU("MPC601",SwitchTo_601);
   CPU6000=AddCPU("RS6000",SwitchTo_601);

   SaveInitProc=InitPassProc; InitPassProc=InitPass_601;
END
