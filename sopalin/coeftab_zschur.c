/**
 *
 * @file coeftab_zschur.c
 *
 * Precision dependent routines to extract the Schur complement.
 *
 * @copyright 2015-2017 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 * @version 6.0.0
 * @author Gregoire Pichon
 * @author Mathieu Faverge
 * @date 2017-04-28
 *
 * @precisions normal z -> s d c
 *
 **/
#include "common.h"
#include "solver.h"
#include "lapacke.h"
#include "sopalin/coeftab_z.h"
#include "pastix_zcores.h"

/**
 *******************************************************************************
 *
 * @brief Extract a low-rank cblk panel to a dense lapack form
 *
 *******************************************************************************
 *
 * @param[in] cblk
 *          The column block to extract in the Schur array
 *
 * @param[in] upper_part
 *          If true, the upper part is also extracted in S.
 *
 * @param[inout] S
 *          The pointer to the top of the column of the cblk in the Schur array.
 *          On exit, the computed coefficient are copy to this array.
 *
 * @param[in] lds
 *          The leading dimension of the S array.
 *
 *******************************************************************************/
void
coeftab_zgetschur_one_lowrank( const SolverCblk *cblk, int upper_part,
                               pastix_complex64_t *S, pastix_int_t lds )
{
    SolverBlok *blok  = cblk[0].fblokptr;
    SolverBlok *lblok = cblk[1].fblokptr;
    pastix_int_t coefind;
    pastix_int_t ncols = cblk_colnbr( cblk );
    int ret;

    assert( cblk->cblktype & CBLK_COMPRESSED );
    assert( cblk->cblktype & CBLK_LAYOUT_2D );

    for (; blok<lblok; blok++)
    {
        pastix_int_t nrows = blok_rownbr( blok );

        nrows   = blok_rownbr( blok );
        coefind = blok->coefind / ncols;

        ret = core_zlr2ge( PastixNoTrans, nrows, ncols,
                           blok->LRblock,
                           S + coefind, lds );
        assert( ret == 0 );
        /* TODO: check/fix with respect to full rank (L+U instead of just L or U)*/

        if ( upper_part ) {
            if ( blok == cblk[0].fblokptr ) {
                assert( cblk->fblokptr->LRblock[1].rk    == -1    );
                assert( cblk->fblokptr->LRblock[1].rkmax == ncols );

                core_zgeadd( PastixTrans, ncols, ncols,
                             1.0, cblk->fblokptr->LRblock[1].u, ncols,
                             1.0, S + coefind * lds, lds );

            } else {
                ret = core_zlr2ge( PastixTrans, nrows, ncols,
                                   blok->LRblock+1,
                                   S + coefind * lds, lds );
                assert( ret == 0 );
            }
        }
    }

    (void)ret;
}

/**
 *******************************************************************************
 *
 * @brief Extract a full-rank cblk panel to a dense lapack form
 *
 *******************************************************************************
 *
 * @param[in] cblk
 *          The column block to extract in the Schur array
 *
 * @param[in] upper_part
 *          If true, the upper part is also extracted in S.
 *
 * @param[inout] S
 *          The pointer to the top of the column of the cblk in the Schur array.
 *          On exit, the computed coefficient are copy to this array.
 *
 * @param[in] lds
 *          The leading dimension of the S array.
 *
 *******************************************************************************/
void
coeftab_zgetschur_one_fullrank( const SolverCblk *cblk, int upper_part,
                                pastix_complex64_t *S, pastix_int_t lds )
{
    SolverBlok *blok  = cblk[0].fblokptr;
    SolverBlok *lblok = cblk[1].fblokptr;
    pastix_int_t ncols = cblk_colnbr( cblk );
    pastix_int_t nrows, coefind, stride, ret;
    pastix_complex64_t *lcoeftab = cblk->lcoeftab;
    pastix_complex64_t *ucoeftab = cblk->ucoeftab;
    int layout2d = ( cblk->cblktype & CBLK_LAYOUT_2D );

    assert( !(cblk->cblktype & CBLK_COMPRESSED) );

    for (; blok<lblok; blok++)
    {
        nrows = blok_rownbr( blok );

        if (layout2d) {
            coefind = blok->coefind / ncols;
            stride  = nrows;
        }
        else {
            coefind = blok->coefind;
            stride  = cblk->stride;
        }

        ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', nrows, ncols,
                                   lcoeftab + blok->coefind, stride,
                                   S + coefind, lds );
        assert( ret == 0 );

        if ( upper_part )
        {
            core_zgeadd( PastixTrans, ncols, nrows,
                         1.0, ucoeftab + blok->coefind, stride,
                         1.0, S + coefind * lds, lds );
        }
    }

    (void)ret;
}


/**
 *******************************************************************************
 *
 * @brief Extract the Schur complement
 *
 * This routine is sequential and returns the full Schur complement
 * uncommpressed in Lapack format.
 *
 *******************************************************************************
 *
 * @param[in] solvmtx
 *          The solver matrix structure describing the problem.
 *
 * @param[inout] S
 *          The pointer to the allocated matrix array that will store the Schur
 *          complement.
 *
 * @param[in] lds
 *          The leading dimension of the S array.
 *
 *******************************************************************************/
void
coeftab_zgetschur( const SolverMatrix *solvmtx,
                   pastix_complex64_t *S, pastix_int_t lds )
{
    SolverCblk *cblk = solvmtx->cblktab + solvmtx->cblkschur;
    pastix_complex64_t *localS;
    pastix_int_t itercblk, fcolnum, nbcol;
    int upper_part = (solvmtx->factotype == PastixFactLU);
    fcolnum = cblk->fcolnum;

    nbcol = solvmtx->nodenbr - fcolnum;
    assert( nbcol <= lds );

    /* Initialize the array to 0 */
    LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', nbcol, nbcol, 0., 0., S, lds );

    for (itercblk=solvmtx->cblkschur; itercblk<solvmtx->cblknbr; itercblk++, cblk++)
    {
        assert( cblk->cblktype & CBLK_IN_SCHUR );
        assert( lds >= cblk->stride );

        localS = S + (cblk->fcolnum - fcolnum) * lds + (cblk->fcolnum - fcolnum);

        if ( cblk->cblktype & CBLK_COMPRESSED ) {
            coeftab_zgetschur_one_lowrank( cblk, upper_part, localS, lds );
        }
        else {
            coeftab_zgetschur_one_fullrank( cblk, upper_part, localS, lds );
        }
    }
}