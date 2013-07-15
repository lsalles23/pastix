/**
 *
 * @file order_base.c
 *
 *  PaStiX order routines
 *  PaStiX is a software package provided by Inria Bordeaux - Sud-Ouest,
 *  LaBRI, University of Bordeaux 1 and IPB.
 *
 * Contains function to adjust the base value of the ordering.
 *
 * @version 5.1.0
 * @author Francois Pellegrini
 * @date 2013-06-24
 *
 **/
#include "common.h"
#include "order.h"

/**
 *******************************************************************************
 *
 * @ingroup pastix_ordering
 *
 * orderAddIsolate - This routine combines two pemutation arrays when a subset
 * of vertices has been isolated from the original graph.
 *
 *******************************************************************************
 *
 * @param[in,out] ordemesh
 *          The ordering generated by the ordering tool.
 *
 * @param[in] new_n
 *          The total number of vertices in the combined graph
 *
 * @param[in] perm
 *          Array of size new_n
 *          The permutation array that isolated the extra vertices at the end of
 *          the graph. this permutation will be combined with the one stored in
 *          ordemesh to generate a permutation array for the full graph.
 *
 *******************************************************************************/
int orderAddIsolate( Order        *ordemesh,
                     pastix_int_t  new_n,
                     const pastix_int_t *perm )
{
    Order ordesave;
    pastix_int_t i, ip;
    pastix_int_t n       = ordemesh->vertnbr;
    pastix_int_t cblknbr = ordemesh->cblknbr;
    int          baseval = ordemesh->baseval;

    assert( n <= new_n );

    /* Quick return */
    if ( n == new_n )
        return PASTIX_SUCCESS;

    memcpy( &ordesave, ordemesh, sizeof(Order) );
    orderInit( ordemesh, new_n, cblknbr + 1 );
    ordemesh->baseval = baseval;

    for(i=0; i< new_n; i++) {
        ip = perm[i];
        if (ip < n-baseval)
            ordemesh->permtab[i] = ordesave.permtab[ ip ];
        else
            ordemesh->permtab[i] = ip+baseval;
    }
    for(i=0; i<new_n; i++) {
        ip = ordemesh->permtab[i] - baseval;
        assert( (ip > -1) && (ip < new_n) );
        ordemesh->peritab[ip] = i + baseval;
    }

    /* Copy the cblknbr+1 first element of old rangtab and add last element */
    memcpy( &(ordemesh->rangtab), &(ordesave.rangtab), ordemesh->cblknbr * sizeof(pastix_int_t) );
    ordemesh->rangtab[ ordemesh->cblknbr ] = new_n + baseval;

    orderExit( &ordesave );
    return PASTIX_SUCCESS;
}

