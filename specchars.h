#ifndef _SPECCHARS_H
#define _SPECCHARS_H
/* specchars.h */
/*****************************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only                     */
/*                                                                           */
/* AS-Portierung                                                             */
/*                                                                           */
/* special character encodings                                               */
/*                                                                           */
/*****************************************************************************/
/* $Id: specchars.h,v 1.1 2003/11/06 02:49:24 alfred Exp $                   */
/***************************************************************************** 
 * $Log: specchars.h,v $
 * Revision 1.1  2003/11/06 02:49:24  alfred
 * - recreated
 *
 * Revision 1.1  2002/03/10 10:55:37  alfred
 * - created
 *
 *****************************************************************************/

static unsigned char specchars[][2]=
            {{0204,0344},        /* adieresis */
             {0224,0366},        /* odieresis */
             {0201,0374},        /* udieresis */
             {0216,0304},        /* Adieresis */
             {0231,0326},        /* Odieresis */
             {0232,0334},        /* Udieresis */
             {0341,0337},        /* ssharp */
             {0375,0262},        /* 2-exp */
             {0346,0265},        /* mu */
             {0205,0340},        /* a graph */
             {0000,0000}};

#endif /* _SPECCHARS_H */
