/* Copyright INRIA 2004
**
** This file is part of the Scotch distribution.
**
** The Scotch distribution is libre/free software; you can
** redistribute it and/or modify it under the terms of the
** GNU Lesser General Public License as published by the
** Free Software Foundation; either version 2.1 of the
** License, or (at your option) any later version.
**
** The Scotch distribution is distributed in the hope that
** it will be useful, but WITHOUT ANY WARRANTY; without even
** the implied warranty of MERCHANTABILITY or FITNESS FOR A
** PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General
** Public License along with the Scotch distribution; if not,
** write to the Free Software Foundation, Inc.,
** 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
**
** $Id: fax.h 316 2005-06-06 16:17:44Z ramet $
*/
/************************************************************/
/**                                                        **/
/**   NAME       : fax.h                                   **/
/**                                                        **/
/**   AUTHORS    : Francois PELLEGRINI                     **/
/**                Jean ROMAN (v0.0)                       **/
/**                                                        **/
/**   FUNCTION   : Part of a parallel direct block solver. **/
/**                These lines are the data declarations   **/
/**                for the symbolic factorization routine. **/
/**                                                        **/
/**   DATES      : # Version 0.0  : from : 22 jul 1998     **/
/**                                 to     24 sep 1998     **/
/**                # Version 0.1  : from : 04 apr 1999     **/
/**                                 to     01 may 1999     **/
/**                # Version 1.0  : from : 01 jun 2002     **/
/**                                 to     25 jun 2002     **/
/**                # Version 1.1  : from : 26 jun 2002     **/
/**                                 to     25 sep 2002     **/
/**                # Version 1.3  : from : 17 jun 2003     **/
/**                                 to     17 jul 2003     **/
/**                # Version 2.0  : from : 21 mar 2003     **/
/**                                 to     29 oct 2003     **/
/**                # Version 2.0  : from : 03 mar 2004     **/
/**                                 to     03 mar 2004     **/
/**                # Version 3.0  : from : 23 nov 2004     **/
/**                                 to     03 mar 2005     **/
/**                                                        **/
/************************************************************/

#ifndef _FAX_H_
#define _FAX_H_

int symbolFaxGraph(SymbolMatrix * const symbptr,
                   pastix_int_t   baseval,
                   pastix_int_t   vertnbr,
                   pastix_int_t * verttab,
                   pastix_int_t   edgenbr,
                   pastix_int_t * edgetab,
                   const Order  * const ordeptr);

void pastix_task_symbfact(pastix_data_t *pastix_data,
                          pastix_int_t  *perm,
                          pastix_int_t  *invp,
                          int flagWinvp);

#endif /* _FAX_H_ */