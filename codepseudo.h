#ifndef _CODEPSEUDO_H
#define _CODEPSEUDO_H
/* codepseudo.h */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Haeufiger benutzte Pseudo-Befehle                                         */
/*                                                                           */
/* Historie: 23. 5.1996 Grundsteinlegung                                     */
/*                                                                           */
/*****************************************************************************/

typedef struct
         {
          char *Name;
          LongInt *Dest;
          LongInt Min,Max;
          LongInt NothingVal;
         } ASSUMERec;

extern int FindInst(void *Field, int Size, int Count);

extern Boolean IsIndirect(char *Asc);

extern void ConvertDec(Double F, Word *w);

extern Boolean DecodeIntelPseudo(Boolean Turn);

extern Boolean DecodeMotoPseudo(Boolean Turn);

extern void AddMoto16PseudoONOFF(void);

extern Boolean DecodeMoto16Pseudo(ShortInt OpSize, Boolean Turn);

extern void CodeEquate(ShortInt DestSeg, LargeInt Min, LargeInt Max);

extern void CodeASSUME(ASSUMERec *Def, Integer Cnt);

extern void codepseudo_init(void);
#endif /* _CODEPSEUDO_H */
