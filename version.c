/* version.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Lagert die Versionsnummer                                                 */
/*                                                                           */
/* Historie: 14.10.1997 Grundsteinlegung                                     */
/*           25. 7.1998 Version 1.41r8beta                                   */
/*           18.10.1998 Build 4                                              */
/*           25.10.1998 Build 5                                              */
/*           10. 1.1999 Build 6                                              */
/*           17. 1.1999 Build 7                                              */
/*           27. 1.1999 Build 8                                              */
/*           27. 1.1999 Build 9                                              */
/*            7. 2.1999 Build 10                                             */
/*           19. 4.1999 Build 11                                             */
/*           20. 4.1999 Build 12                                             */
/*            2. 5.1999 Build 13                                             */
/*            6. 7.1999 Build 14                                             */
/*           15. 9.1999 Build 15                                             */
/*            7.11.1999 Final Build                                          */
/*            9. 1.2000 Version 1.42beta                                     */
/*           13. 2.2000 Build 2                                              */
/*           13. 3.2000 Build 3                                              */
/*            4. 6.2000 Build 5                                              */
/*           18. 6.2000 Build 6                                              */
/*            6. 8.2000 Build 7                                              */
/*           20. 9.2000 Build 8                                              */
/*           15.10.2000 Build 9                                              */
/*            7. 1.2001 Build 10                                             */
/*           14. 1.2001 Build 11                                             */
/*            4. 2.2001 Build 12                                             */
/*           30. 5.2001 Build 13                                             */
/*            1. 7.2001 Build 14                                             */
/*           20. 8.2001 Build 15                                             */
/*            7.10.2001 Build 16                                             */
/*           2001-11-04 Build 17                                             */
/*           2002-01-13 Build 18                                             */
/*           2002-01-27 Build 19                                             */
/*                                                                           */
/*****************************************************************************/
/* $Id: version.c,v 1.8 2003/02/26 19:27:50 alfred Exp $                     */
/***************************************************************************** 
 * $Log: version.c,v $
 * Revision 1.8  2003/02/26 19:27:50  alfred
 * - Build 26
 *
 * Revision 1.7  2003/02/02 13:32:24  alfred
 * - Build 25
 *
 * Revision 1.6  2002/11/23 14:48:02  alfred
 * . Build 24
 *
 * Revision 1.5  2002/10/11 08:34:54  alfred
 * - 1.42 Bld 23
 *
 * Revision 1.4  2002/10/09 17:55:07  alfred
 * - 1.42 Bld 22
 *
 * Revision 1.3  2002/05/25 20:39:23  alfred
 * - 1.42 Bld 21
 *
 * Revision 1.2  2002/03/10 10:50:48  alfred
 * - build 20
 *
 *****************************************************************************/

#include "stdinc.h"
#include <string.h>

char *Version="1.42 Beta [Bld 26]";
LongInt VerNo=0x142f;

char *InfoMessCopyright="(C) 1992,2003 Alfred Arnold";

LongInt Magic=0x12372f44;

void version_init(void)
{
  unsigned int z;
  char *CMess = InfoMessCopyright;
  LongWord XORVal;

  for (z = 0; z < strlen(CMess); z++)
  {
    XORVal = CMess[z];
    XORVal = XORVal << (((z + 1) % 4) * 8);
    Magic = Magic ^ XORVal;
  }
}
