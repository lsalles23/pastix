/**
 *
 * @file core_zpotrfsp.c
 *
 * PaStiX kernel routines for Cholesky factorization.
 *
 * @copyright 2011-2017 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 * @version 6.0.0
 * @author Mathieu Faverge
 * @author Pierre Ramet
 * @author Xavier Lacoste
 * @date 2011-11-11
 * @precisions normal z -> c d s
 *
 **/
#include "common.h"
#include "cblas.h"
#include "blend/solver.h"
#include "pastix_zcores.h"
#include "kernels_trace.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define MAXSIZEOFBLOCKS 64
static pastix_complex64_t zone  =  1.0;
static pastix_complex64_t mzone = -1.0;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 *******************************************************************************
 *
 * @ingroup kernel_blas_lapack_null
 *
 * @brief Compute the sequential static pivoting Cholesky factorization of the
 * matrix n-by-n A = L * L^t .
 *
 *******************************************************************************
 *
 * @param[in] n
 *          The number of rows and columns of the matrix A.
 *
 * @param[inout] A
 *          The matrix A to factorize with Cholesky factorization. The matrix
 *          is of size lda -by- n.
 *
 * @param[in] lda
 *          The leading dimension of the matrix A.
 *
 * @param[inout] nbpivot
 *          Pointer to the number of piovting operations made during
 *          factorization. It is updated during this call
 *
 * @param[in] criteria
 *          Threshold use for static pivoting. If diagonal value is under this
 *          threshold, its value is replaced by the threshold and the number of
 *          pivots is incremented.
 *
 *******************************************************************************
 *
 * @warning This routine will fail if it discovers a null or negative value on
 *          the diagonal during factorization.
 *
 *******************************************************************************/
static inline void
core_zpotf2sp( pastix_int_t        n,
               pastix_complex64_t *A,
               pastix_int_t        lda,
               pastix_int_t       *nbpivot,
               double              criteria )
{
    pastix_int_t k;
    pastix_complex64_t *Akk = A;   /* A [k  ][k] */
    pastix_complex64_t *Amk = A+1; /* A [k+1][k] */
    pastix_complex64_t  alpha;

    for (k=0; k<n; k++){
        if ( cabs(*Akk) < criteria ) {
            (*Akk) = (pastix_complex64_t)criteria;
            (*nbpivot)++;
        }

        /* Hermitian matrices, so imaginary part should be 0 */
        if ( creal(*Akk) < 0.0 )
        {
            errorPrint("Negative diagonal term\n");
            assert(0);
            EXIT(MOD_SOPALIN, INTERNAL_ERR);
        }

        *Akk = csqrt(*Akk);
        alpha = 1.0 / (*Akk);

        /* Scale the diagonal to compute L((k+1):n,k) */
        cblas_zscal(n-k-1, CBLAS_SADDR( alpha ), Amk, 1 );

        /* Move to next Akk */
        Akk += (lda+1);

        cblas_zher(CblasColMajor, CblasLower,
                   n-k-1, -1.0,
                   Amk, 1,
                   Akk, lda);

        /* Move to next Amk */
        Amk = Akk+1;
    }
}

/**
 *******************************************************************************
 *
 * @brief Compute the block static pivoting Cholesky factorization of the matrix
 * n-by-n A = L * L^t .
 *
 *******************************************************************************
 *
 * @param[in] n
 *          The number of rows and columns of the matrix A.
 *
 * @param[inout] A
 *          The matrix A to factorize with Cholesky factorization. The matrix
 *          is of size lda -by- n.
 *
 * @param[in] lda
 *          The leading dimension of the matrix A.
 *
 * @param[inout] nbpivot
 *          Pointer to the number of piovting operations made during
 *          factorization. It is updated during this call
 *
 * @param[in] criteria
 *          Threshold use for static pivoting. If diagonal value is under this
 *          threshold, its value is replaced by the threshold and the nu,ber of
 *          pivots is incremented.
 *
 *******************************************************************************
 *
 * @warning This routine will fail if it discovers a null or negative value on
 *          the diagonal during factorization.
 *
 *******************************************************************************/
void
core_zpotrfsp( pastix_int_t        n,
               pastix_complex64_t *A,
               pastix_int_t        lda,
               pastix_int_t       *nbpivot,
               double              criteria )
{
    pastix_int_t k, blocknbr, blocksize, matrixsize;
    pastix_complex64_t *tmp,*tmp1,*tmp2;

    /* diagonal supernode is divided into MAXSIZEOFBLOCK-by-MAXSIZEOFBLOCKS blocks */
    blocknbr = (n + MAXSIZEOFBLOCKS - 1) / MAXSIZEOFBLOCKS;

    for (k=0; k<blocknbr; k++) {

        blocksize = pastix_imin(MAXSIZEOFBLOCKS, n-k*MAXSIZEOFBLOCKS);
        tmp  = A+(k*MAXSIZEOFBLOCKS)*(lda+1);      /* Lk,k     */

        /* Factorize the diagonal block Akk*/
        core_zpotf2sp(blocksize, tmp, lda, nbpivot, criteria);

        if ((k*MAXSIZEOFBLOCKS+blocksize) < n) {

            tmp1 = tmp  + blocksize;       /* Lk+1,k   */
            tmp2 = tmp1 + blocksize * lda; /* Lk+1,k+1 */

            matrixsize = n-(k*MAXSIZEOFBLOCKS+blocksize);

            /* Compute the column L(k+1:n,k) = (L(k,k)D(k,k))^{-1}A(k+1:n,k)    */
            /* 1) Compute A(k+1:n,k) = A(k+1:n,k)L(k,k)^{-T} = D(k,k)L(k+1:n,k) */
                        /* input: L(k,k) in tmp, A(k+1:n,k) in tmp1   */
                        /* output: A(k+1:n,k) in tmp1                 */
            cblas_ztrsm(CblasColMajor,
                        CblasRight, CblasLower,
                        CblasConjTrans, CblasNonUnit,
                        matrixsize, blocksize,
                        CBLAS_SADDR(zone), tmp,  lda,
                                           tmp1, lda);

            /* Update Ak+1k+1 = Ak+1k+1 - Lk+1k * Lk+1kT */
            cblas_zherk(CblasColMajor, CblasLower, CblasNoTrans,
                        matrixsize, blocksize,
                        (double)mzone, tmp1, lda,
                        (double)zone,  tmp2, lda);
        }
    }
}

/**
 *******************************************************************************
 *
 * @brief Compute the Cholesky factorization of the diagonal block in a panel.
 *
 * @warning This routine will fail if it discovers a null or negative value on
 *          the diagonal during factorization.
 *
 *******************************************************************************
 *
 * @param[in] solvmtx
 *          Solver Matrix structure of the problem
 *
 * @param[in] cblk
 *          Pointer to the structure representing the panel to factorize in the
 *          cblktab array.  Next column blok must be accessible through cblk[1].
 *
 * @param[inout] L
 *          The pointer to the matrix storing the coefficients of the
 *          panel. Must be of size cblk.stride -by- cblk.width
 *
 *******************************************************************************
 *
 * @return The number of static pivoting performed during the diagonal block
 *         factorization.
 *
 *******************************************************************************/
int
cpucblk_zpotrfsp1d_potrf( SolverMatrix       *solvmtx,
                          SolverCblk         *cblk,
                          pastix_complex64_t *L )
{
    pastix_int_t  ncols, stride;
    pastix_int_t  nbpivot = 0;
    pastix_fixdbl_t time;
    double criteria = solvmtx->diagthreshold;

    time = kernel_trace_start( PastixKernelPOTRF );

    ncols   = cblk->lcolnum - cblk->fcolnum + 1;
    stride  = (cblk->cblktype & CBLK_LAYOUT_2D) ? ncols : cblk->stride;

    /* check if diagonal column block */
    assert( cblk->fcolnum == cblk->fblokptr->frownum );
    assert( cblk->lcolnum == cblk->fblokptr->lrownum );

    if ( cblk->cblktype & CBLK_COMPRESSED ) {
        assert( cblk->fblokptr->LRblock[0].rk == -1 );
        L = cblk->fblokptr->LRblock[0].u;
        stride = ncols;

        assert( stride == cblk->fblokptr->LRblock[0].rkmax );
    }

    /* Factorize diagonal block */
    kernel_trace_start_lvl2( PastixKernelLvl2POTRF );
    core_zpotrfsp(ncols, L, stride, &nbpivot, criteria );
    kernel_trace_stop_lvl2( FLOPS_ZPOTRF( ncols ) );

    kernel_trace_stop( PastixKernelPOTRF, ncols, 0, 0, FLOPS_ZPOTRF( ncols ), time );

    return nbpivot;
}

/**
 *******************************************************************************
 *
 * @brief Compute the Cholesky factorization of one panel.
 *
 *******************************************************************************
 *
 * @param[in] solvmtx
 *          Solver Matrix structure of the problem
 *
 * @param[in] cblk
 *          Pointer to the structure representing the panel to factorize in the
 *          cblktab array.  Next column blok must be accessible through cblk[1].
 *
 * @param[inout] L
 *          The pointer to the matrix storing the coefficients of the
 *          panel. Must be of size cblk.stride -by- cblk.width
 *
 *******************************************************************************
 *
 * @return The number of static pivoting during factorization of the diagonal
 * block.
 *
 *******************************************************************************/
int
cpucblk_zpotrfsp1d_panel( SolverMatrix       *solvmtx,
                          SolverCblk         *cblk,
                          pastix_complex64_t *L )
{
    pastix_int_t nbpivot;
    nbpivot = cpucblk_zpotrfsp1d_potrf( solvmtx, cblk, L );

    cpucblk_ztrsmsp( PastixLCoef, PastixRight, PastixLower,
                     PastixConjTrans, PastixNonUnit,
                     cblk, L, L, &(solvmtx->lowrank) );
    return nbpivot;
}


/**
 *******************************************************************************
 *
 * @brief Perform the Cholesky factorization of a given panel and apply all its
 * updates.
 *
 *******************************************************************************
 *
 * @param[in] solvmtx
 *          Solver Matrix structure of the problem
 *
 * @param[in] cblk
 *          Pointer to the structure representing the panel to factorize in the
 *          cblktab array.  Next column blok must be accessible through cblk[1].
 *
 * @param[in] work
 *          Temporary memory buffer.
 *
 * @param[in] lwork
 *          Temporary workspace dimension.
 *
 *******************************************************************************
 *
 * @return The number of static pivoting during factorization of the diagonal
 * block.
 *
 *******************************************************************************/
int
cpucblk_zpotrfsp1d( SolverMatrix       *solvmtx,
                    SolverCblk         *cblk,
                    pastix_complex64_t *work,
                    pastix_int_t        lwork )
{
    pastix_complex64_t *L = cblk->lcoeftab;
    SolverCblk  *fcblk;
    SolverBlok  *blok, *lblk;
    pastix_int_t nbpivot;

    nbpivot = cpucblk_zpotrfsp1d_panel( solvmtx, cblk, L );

    blok = cblk->fblokptr + 1; /* First off-diagonal block */
    lblk = cblk[1].fblokptr;   /* Next diagonal block      */

    /* If there are off-diagonal blocks, perform the updates */
    for( ; blok < lblk; blok++ )
    {
        fcblk = (solvmtx->cblktab + blok->fcblknm);

        cpucblk_zgemmsp( PastixLCoef, PastixLCoef, PastixConjTrans,
                         cblk, blok, fcblk,
                         L, L, fcblk->lcoeftab,
                         work, lwork, &(solvmtx->lowrank) );

        pastix_atomic_dec_32b( &(fcblk->ctrbcnt) );
   }

    return nbpivot;
}
