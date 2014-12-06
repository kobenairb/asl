#ifndef _TIPSEUDO_H
#define _TIPSEUDO_H
/* tipseudo.h */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Haeufiger benutzte Texas Instruments Pseudo-Befehle                       */
/*                                                                           */
/*****************************************************************************/
/* $Id: tipseudo.h,v 1.2 2014/11/03 17:36:12 alfred Exp $                    */
/***************************************************************************** 
 * $Log: tipseudo.h,v $
 * Revision 1.2  2014/11/03 17:36:12  alfred
 * - relocate IsDef() for common TI pseudo instructions
 *
 * Revision 1.1  2004/05/29 12:18:06  alfred
 * - relocated DecodeTIPseudo() to separate module
 *
 *****************************************************************************/

/*****************************************************************************
 * Global Functions
 *****************************************************************************/

extern Boolean DecodeTIPseudo(void);

extern Boolean IsTIDef(void);

#endif /* _TIPSEUDO_H */
