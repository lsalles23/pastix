/**
 *
 * @file core_zgemmsp.c
 *
 *  PaStiX kernel routines
 *  PaStiX is a software package provided by Inria Bordeaux - Sud-Ouest,
 *  LaBRI, University of Bordeaux 1 and IPB.
 *
 * @version 1.0.0
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

static pastix_complex64_t mzone = -1.;
static pastix_complex64_t zone  =  1.;
static pastix_complex64_t zzero =  0.;

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zgemmsp_1d1d - Computes the updates that are generated by the
 * transposition of one single off-diagonal block. Both cblk involved in the
 * computation are stored with the 1D storage: Column Major Layout with blocks
 * interleaved.
 *
 * All the off-diagonal block below block are multiplied by the selected block
 * and added to the facing cblk.
 *
 *******************************************************************************
 *
 * @param[in] uplo
 *          If uplo == PastixLower, the contribution of:
 *          (block .. (cblk[1].fblokptr-1)) -by- block is computed and added to
 *          C, otherwise the contribution:
 *          (block+1 .. (cblk[1].fblokptr-1)) -by- block is computed and added
 *          to C.
 *          The pointer to the data structure that describes the panel from
 *          which we compute the contributions. Next column blok must be
 *          accessible through cblk[1].
 *
 * @param[in] trans
 *          Specify the transposition used for the B matrix. It has to be either
 *          PastixTrans or PastixConjTrans.
 *
 * @param[in] cblk
 *          The cblk structure to which block belongs to. The A and B pointers
 *          must be the coeftab of this column block.
 *          Next column blok must be accessible through cblk[1].
 *
 * @param[in] blok
 *          The block from which we compute the contributions.
 *
 * @param[in] fcblk
 *          The pointer to the data structure that describes the panel on which
 *          we compute the contributions. The C pointer must be one of the
 *          oceftab from this fcblk. Next column blok must be accessible through
 *          fcblk[1].
 *
 * @param[in] A
 *          The pointer to the coeftab of the cblk.lcoeftab matrix storing the
 *          coefficients of the panel when the Lower part is computed,
 *          cblk.ucoeftab otherwise. Must be of size cblk.stride -by- cblk.width
 *
 * @param[in] B The pointer to the coeftab of the cblk.lcoeftab matrix storing
 *          the coefficients of the panel, if Symmetric/Hermitian cases or if
 *          upper part is computed; cblk.ucoeftab otherwise. Must be of size
 *          cblk.stride -by- cblk.width
 *
 * @param[in,out] C
 *          The pointer to the fcblk.lcoeftab if the lower part is computed,
 *          fcblk.ucoeftab otherwise.
 *
 * @param[in] work
 *          Temporary memory buffer that is at least equal to the height of the
 *          block B by the sum of the height of all the blocks below the block
 *          B.
 *
 *******************************************************************************
 *
 * @sa core_zgemmsp_1d2d
 * @sa core_zgemmsp_2d2d
 *
 *******************************************************************************/
static inline void
core_zgemmsp_1d1d( int uplo, int trans,
                   const SolverCblk         *cblk,
                   const SolverBlok         *blok,
                         SolverCblk         *fcblk,
                   const pastix_complex64_t *A,
                   const pastix_complex64_t *B,
                         pastix_complex64_t *C,
                         pastix_complex64_t *work )
{
    const SolverBlok *iterblok;
    const SolverBlok *fblok;
    const SolverBlok *lblok;

    pastix_complex64_t *tmpC;
    pastix_complex64_t *wtmp;
    pastix_int_t stride, stridef, indblok;
    pastix_int_t M, N, K, m;
    int shift;

    /* Both cblk and fcblk are stored in 1D */
    assert(!(cblk->cblktype  & CBLK_SPLIT));
    assert(!(fcblk->cblktype & CBLK_SPLIT));

    shift = (uplo == PastixUpper) ? 1 : 0;

    stride  = cblk->stride;
    stridef = fcblk->stride;
    K = cblk_colnbr( cblk );

    /* First blok */
    indblok = blok->coefind;

    N = blok_rownbr( blok );
    M = stride - indblok - (shift * N);

    /* Matrix A = Aik */
    A = A + indblok + (shift * N);
    B = B + indblok;

    /*
     * Compute update A * B'
     */
    wtmp = work;
    cblas_zgemm( CblasColMajor, CblasNoTrans, trans,
                 M, N, K,
                 CBLAS_SADDR(zone),  A,    stride,
                                     B,    stride,
                 CBLAS_SADDR(zzero), wtmp, M );

    /*
     * Add contribution to C in fcblk
     */

    /* Get the first block of the distant panel */
    fblok = fcblk->fblokptr;

    /* Move the pointer to the top of the right column */
    C = C + (blok->frownum - fcblk->fcolnum) * stridef;

    lblok = cblk[1].fblokptr;

    /* for all following blocks in block column */
    for (iterblok=blok+shift; iterblok<lblok; iterblok++) {

        /* Find facing blok */
        while (!is_block_inside_fblock( iterblok, fblok ))
        {
            fblok++;
            assert( fblok < fcblk[1].fblokptr );
        }

        tmpC = C + fblok->coefind + iterblok->frownum - fblok->frownum;
        m = blok_rownbr( iterblok );

        pastix_cblk_lock( fcblk );
        core_zgeadd( CblasNoTrans, m, N,
                     -1.0, wtmp, M,
                      1.0, tmpC, stridef );
        pastix_cblk_unlock( fcblk );

        /* Displacement to next block */
        wtmp += m;
    }
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zgemmsp_1d2d - Computes the updates that are generated by the
 * transposition of one single off-diagonal block. The cblk involved in the
 * matrices A and B are stored with the 1D storage: Column Major Layout with
 * blocks interleaved. The facing cblk of the atrix C, is stored with the 2D
 * storage where each block is stored continuously one after another. (Similar
 * to dense tile storage with variant tile size)
 *
 * All the off-diagonal block below block are multiplied by the selected block
 * and added to the facing cblk.
 *
 *******************************************************************************
 *
 * @param[in] uplo
 *          If uplo == PastixLower, the contribution of:
 *          (block .. (cblk[1].fblokptr-1)) -by- block is computed and added to
 *          C, otherwise the contribution:
 *          (block+1 .. (cblk[1].fblokptr-1)) -by- block is computed and added
 *          to C.
 *          The pointer to the data structure that describes the panel from
 *          which we compute the contributions. Next column blok must be
 *          accessible through cblk[1].
 *
 * @param[in] trans
 *          Specify the transposition used for the B matrix. It has to be either
 *          PastixTrans or PastixConjTrans.
 *
 * @param[in] cblk
 *          The cblk structure to which block belongs to. The A and B pointers
 *          must be the coeftab of this column block.
 *          Next column blok must be accessible through cblk[1].
 *
 * @param[in] blok
 *          The block from which we compute the contributions.
 *
 * @param[in] fcblk
 *          The pointer to the data structure that describes the panel on which
 *          we compute the contributions. The C pointer must be one of the
 *          oceftab from this fcblk. Next column blok must be accessible through
 *          fcblk[1].
 *
 * @param[in] A
 *          The pointer to the coeftab of the cblk.lcoeftab matrix storing the
 *          coefficients of the panel when the Lower part is computed,
 *          cblk.ucoeftab otherwise. Must be of size cblk.stride -by- cblk.width
 *
 * @param[in] B The pointer to the coeftab of the cblk.lcoeftab matrix storing
 *          the coefficients of the panel, if Symmetric/Hermitian cases or if
 *          upper part is computed; cblk.ucoeftab otherwise. Must be of size
 *          cblk.stride -by- cblk.width
 *
 * @param[in,out] C
 *          The pointer to the fcblk.lcoeftab if the lower part is computed,
 *          fcblk.ucoeftab otherwise.
 *
 *******************************************************************************
 *
 * @sa core_zgemmsp_1d1d
 * @sa core_zgemmsp_2d2d
 *
 *******************************************************************************/
static inline void
core_zgemmsp_1d2d( int uplo, int trans,
                   const SolverCblk         *cblk,
                   const SolverBlok         *blok,
                         SolverCblk         *fcblk,
                   const pastix_complex64_t *A,
                   const pastix_complex64_t *B,
                         pastix_complex64_t *C )
{
    const SolverBlok *iterblok;
    const SolverBlok *fblok;
    const SolverBlok *lblok;
    const pastix_complex64_t *blokA;
    const pastix_complex64_t *blokB;
    pastix_complex64_t *blokC;

    pastix_int_t stride, stridef;
    pastix_int_t M, N, K;
    int shift;

    /* cblk is stored in 1D and fcblk in 2D */
    assert(!(cblk->cblktype & CBLK_SPLIT));
    assert( fcblk->cblktype & CBLK_SPLIT );

    shift = (uplo == PastixUpper) ? 1 : 0;
    stride  = cblk->stride;

    /* Get the B block and its dimensions */
    blokB = B + blok->coefind;

    stride  = cblk->stride;
    K = cblk_colnbr( cblk );
    N = blok_rownbr( blok );

    /**
     * Add contribution to C in fcblk:
     *    Get the first facing block of the distant panel, and the last block of
     *    the current cblk
     */
    fblok = fcblk->fblokptr;
    lblok = cblk[1].fblokptr;

    for (iterblok=blok+shift; iterblok<lblok; iterblok++) {

        /* Find facing blok */
        while (!is_block_inside_fblock( iterblok, fblok ))
        {
            fblok++;
            assert( fblok < fcblk[1].fblokptr );
        }

        stridef = blok_rownbr(fblok);

        /* Get the A block and its dimensions */
        blokA = A + iterblok->coefind;
        M = blok_rownbr( iterblok );

        blokC = C + fblok->coefind
            + iterblok->frownum - fblok->frownum
            + (blok->frownum - fcblk->fcolnum) * stridef;

        pastix_cblk_lock( fcblk );
        cblas_zgemm( CblasColMajor, CblasNoTrans, trans,
                     M, N, K,
                     CBLAS_SADDR(mzone), blokA, stride,
                                         blokB, stride,
                     CBLAS_SADDR(zone),  blokC, stridef );
        pastix_cblk_unlock( fcblk );
    }
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zgemmsp_1d2d - Computes the updates that are generated by the
 * transposition of one single off-diagonal block. Both cblk involved in the
 * matrices A, B and C are stored with the 2D storage where each block is stored
 * continuously one after another. (Similar to dense tile storage with variant
 * tile size)
 *
 * All the off-diagonal block below block are multiplied by the selected block
 * and added to the facing cblk.
 *
 *******************************************************************************
 *
 * @param[in] uplo
 *          If uplo == PastixLower, the contribution of:
 *          (block .. (cblk[1].fblokptr-1)) -by- block is computed and added to
 *          C, otherwise the contribution:
 *          (block+1 .. (cblk[1].fblokptr-1)) -by- block is computed and added
 *          to C.
 *          The pointer to the data structure that describes the panel from
 *          which we compute the contributions. Next column blok must be
 *          accessible through cblk[1].
 *
 * @param[in] trans
 *          Specify the transposition used for the B matrix. It has to be either
 *          PastixTrans or PastixConjTrans.
 *
 * @param[in] cblk
 *          The cblk structure to which block belongs to. The A and B pointers
 *          must be the coeftab of this column block.
 *          Next column blok must be accessible through cblk[1].
 *
 * @param[in] blok
 *          The block from which we compute the contributions.
 *
 * @param[in] fcblk
 *          The pointer to the data structure that describes the panel on which
 *          we compute the contributions. The C pointer must be one of the
 *          oceftab from this fcblk. Next column blok must be accessible through
 *          fcblk[1].
 *
 * @param[in] A
 *          The pointer to the coeftab of the cblk.lcoeftab matrix storing the
 *          coefficients of the panel when the Lower part is computed,
 *          cblk.ucoeftab otherwise. Must be of size cblk.stride -by- cblk.width
 *
 * @param[in] B The pointer to the coeftab of the cblk.lcoeftab matrix storing
 *          the coefficients of the panel, if Symmetric/Hermitian cases or if
 *          upper part is computed; cblk.ucoeftab otherwise. Must be of size
 *          cblk.stride -by- cblk.width
 *
 * @param[in,out] C
 *          The pointer to the fcblk.lcoeftab if the lower part is computed,
 *          fcblk.ucoeftab otherwise.
 *
 *******************************************************************************
 *
 * @sa core_zgemmsp_1d1d
 * @sa core_zgemmsp_1d2d
 *
 *******************************************************************************/
static inline void
core_zgemmsp_2d2d( int uplo, int trans,
                   const SolverCblk         *cblk,
                   const SolverBlok         *blok,
                         SolverCblk         *fcblk,
                   const pastix_complex64_t *A,
                   const pastix_complex64_t *B,
                         pastix_complex64_t *C )
{
    const SolverBlok *iterblok;
    const SolverBlok *fblok;
    const SolverBlok *lblok;
    const pastix_complex64_t *blokA;
    const pastix_complex64_t *blokB;
    pastix_complex64_t *blokC;

    pastix_int_t M, N, K, lda, ldb, ldc;
    int shift;

    /* cblk is stored in 1D and fcblk in 2D */
    assert( cblk->cblktype  & CBLK_SPLIT );
    assert( fcblk->cblktype & CBLK_SPLIT );

    shift = (uplo == PastixUpper) ? 1 : 0;

    /* Get the B block and its dimensions */
    blokB = B + blok->coefind;

    ldb = blok_rownbr( blok );
    K = cblk_colnbr( cblk );
    N = blok_rownbr( blok );

    /**
     * Add contribution to C in fcblk:
     *    Get the first facing block of the distant panel, and the last block of
     *    the current cblk
     */
    fblok = fcblk->fblokptr;
    lblok = cblk[1].fblokptr;

    for (iterblok=blok+shift; iterblok<lblok; iterblok++) {

        /* Find facing blok */
        while (!is_block_inside_fblock( iterblok, fblok ))
        {
            fblok++;
            assert( fblok < fcblk[1].fblokptr );
        }

        ldc = blok_rownbr(fblok);

        /* Get the A block and its dimensions */
        blokA = A + iterblok->coefind;
        M = blok_rownbr( iterblok );
        lda = M;

        blokC = C + fblok->coefind
            + iterblok->frownum - fblok->frownum
            + (blok->frownum - fcblk->fcolnum) * ldc;

        pastix_cblk_lock( fcblk );
        cblas_zgemm( CblasColMajor, CblasNoTrans, trans,
                     M, N, K,
                     CBLAS_SADDR(mzone), blokA, lda,
                                         blokB, ldb,
                     CBLAS_SADDR(zone),  blokC, ldc );
        pastix_cblk_unlock( fcblk );
    }
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zgemmsp_2d2dsub - Computes the updates that are generated by the
 * transposition of all the blocks facing a common diagonal block, by another
 * similar set of blocks.
 * This is used to performed an update:
 *
 *                      C_mn = C_mn - A_mk * op(B_kn)
 *
 * where A_mk is the set of blocks in cblk k, facing the diagonal block of the
 * cblk m; B_kn is the set of blocks in cblk n facing the diagonal block of the
 * cblk k; and C_mn is the set of blocks impacted by this update, it necessarily
 * belongs to the set of block of the cblk n facing the diagonal block of the
 * cblk m.
 *
 *******************************************************************************
 *
 * @param[in] uplo
 *          - PastixLower, all the contributions to the lower triangular block
 *          structure, diagonal blocks *included*, are applied.
 *          - PastixUpper, all the contributions to the upper triangular block
 *          structure, diagonal blocks *excluded*, are applied.
 *          - PastixUpperLower, all the contributions are applied
 *
 * @param[in] trans
 *          Specify the transposition used for the B matrices. It has to be either
 *          PastixTrans or PastixConjTrans.
 *
 * @param[in] blok_mk
 *          Index of the first off-diagonal block in cblk, that is used for A.
 *
 * @param[in] blok_nk
 *          Index of the first off-diagonal block in cblk, that is used for B.
 *
 * @param[in] blok_mn
 *          Index of the first off-diagonal block in fcblk, that is used for C.
 *
 * @param[in] cblk
 *          The cblk structure to which block belongs to. The A and B pointers
 *          must be the coeftab of this column block.
 *          Next column blok must be accessible through cblk[1].
 *
 * @param[in] blok
 *          The block from which we compute the contributions.
 *
 * @param[in] fcblk
 *          The pointer to the data structure that describes the panel on which
 *          we compute the contributions. The C pointer must be one of the
 *          oceftab from this fcblk. Next column blok must be accessible through
 *          fcblk[1].
 *
 * @param[in] A
 *          The pointer to the coeftab of the cblk.lcoeftab matrix storing the
 *          coefficients of the panel when the Lower part is computed,
 *          cblk.ucoeftab otherwise. Must be of size cblk.stride -by- cblk.width
 *
 * @param[in] B The pointer to the coeftab of the cblk.lcoeftab matrix storing
 *          the coefficients of the panel, if Symmetric/Hermitian cases or if
 *          upper part is computed; cblk.ucoeftab otherwise. Must be of size
 *          cblk.stride -by- cblk.width
 *
 * @param[in,out] C
 *          The pointer to the fcblk.lcoeftab if the lower part is computed,
 *          fcblk.ucoeftab otherwise.
 *
 *******************************************************************************
 *
 * @sa core_zgemmsp_1d1d
 * @sa core_zgemmsp_1d2d
 *
 *******************************************************************************/
void
core_zgemmsp_2d2dsub( int uplo, int trans,
                      pastix_int_t blok_mk,
                      pastix_int_t blok_kn,
                      pastix_int_t blok_mn,
                      const SolverCblk         *cblk,
                            SolverCblk         *fcblk,
                      const pastix_complex64_t *A,
                      const pastix_complex64_t *B,
                            pastix_complex64_t *C )
{
    const SolverBlok *blokA, *blokB, *blokC;
    const SolverBlok *bA, *bB, *bC;
    const SolverBlok *fblokK, *lblokK;
    const SolverBlok *fblokN, *lblokN;

    const pastix_complex64_t *Aptr, *Bptr;
    pastix_complex64_t *Cptr;
    pastix_int_t M, N, K, lda, ldb, ldc, cblk_n, cblk_m;
    size_t offsetA, offsetB, offsetC;

    /* cblk is stored in 1D and fcblk in 2D */
    assert( cblk->cblktype  & CBLK_SPLIT );
    assert( fcblk->cblktype & CBLK_SPLIT );

    /**
     * Blocs on column K
     */
    fblokK = cblk[0].fblokptr;
    lblokK = cblk[1].fblokptr;

    blokB = fblokK + blok_kn;
    offsetB = blokB->coefind;

    blokA = fblokK + blok_mk;
    offsetA = blokA->coefind;
    cblk_m = blokA->fcblknm;

    /**
     * Blocs on column N
     */
    fblokN = fcblk[0].fblokptr;
    lblokN = fcblk[1].fblokptr;

    blokC = fblokN + blok_mn;
    offsetC = blokC->coefind;
    assert( blokC->fcblknm == cblk_m );

    K = cblk_colnbr( cblk );

    bC = blokC;
    for (bA = blokA; (bA < lblokK) && (bA->fcblknm == cblk_m); bA++) {
        M = blok_rownbr(bA);
        Aptr = A + bA->coefind - offsetA;
        lda = M;

        /* Find facing C blok */
        while (!is_block_inside_fblock( bA, bC )) {
            bC++;
            assert( bC < lblokN );
        }

        Cptr = C + bC->coefind - offsetC;
        ldc = blok_rownbr(bC);

        switch (uplo) {
        case PastixLower:
            for (bB = blokB; (bB <= bA) && (bB->fcblknm == cblk_n); bB++) {
                N = blok_rownbr( bB );
                Bptr = B + bB->coefind - offsetB;
                ldb = N;

                cblas_zgemm( CblasColMajor, CblasNoTrans, trans,
                             M, N, K,
                             CBLAS_SADDR(mzone), Aptr, lda,
                             Bptr, ldb,
                             CBLAS_SADDR(zone),  Cptr + (bA->frownum - bC->frownum)
                             + (bB->frownum - fcblk->fcolnum) * ldc , ldc );
            }
            break;
        case PastixUpper:
            for (bB = blokB; (bB < bA) && (bB->fcblknm == cblk_n); bB++) {
                N = blok_rownbr( bB );
                Bptr = B + bB->coefind - offsetB;
                ldb = N;

                cblas_zgemm( CblasColMajor, CblasNoTrans, trans,
                             M, N, K,
                             CBLAS_SADDR(mzone), Aptr, lda,
                             Bptr, ldb,
                             CBLAS_SADDR(zone),  Cptr + (bA->frownum - bC->frownum)
                             + (bB->frownum - fcblk->fcolnum) * ldc , ldc );
            }
            break;
        case PastixUpperLower:
        default:
            for (bB = blokB; (bB < lblokK) && (bB->fcblknm == cblk_n); bB++) {
                N = blok_rownbr( bB );
                Bptr = B + bB->coefind - offsetB;
                ldb = N;

                cblas_zgemm( CblasColMajor, CblasNoTrans, trans,
                             M, N, K,
                             CBLAS_SADDR(mzone), Aptr, lda,
                             Bptr, ldb,
                             CBLAS_SADDR(zone),  Cptr + (bA->frownum - bC->frownum)
                             + (bB->frownum - fcblk->fcolnum) * ldc , ldc );
            }
        }
    }

    (void)lblokN;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zgemmsp_fulllr - Computes the updates associated to one off-diagonal block.
 *
 *******************************************************************************
 *
 * @param[in] uplo
 *          If uplo == PastixLower, the contribution of:
 *          (block .. (cblk[1].fblokptr-1)) -by- block is computed and added to
 *          C, otherwise the contribution:
 *          (block+1 .. (cblk[1].fblokptr-1)) -by- block is computed and added
 *          to C.
 *          The pointer to the data structure that describes the panel from
 *          which we compute the contributions. Next column blok must be
 *          accessible through cblk[1].
 *
 * @param[in] trans
 *          Specify the transposition used for the B matrix. It has to be either
 *          PastixTrans or PastixConjTrans.
 *
 * @param[in] cblk
 *          The cblk structure to which block belongs to. The A and B pointers
 *          must be the coeftab of this column block.
 *          Next column blok must be accessible through cblk[1].
 *
 * @param[in] blok
 *          The block from which we compute the contributions.
 *
 * @param[in] fcblk
 *          The pointer to the data structure that describes the panel on which
 *          we compute the contributions. The C pointer must be one of the
 *          oceftab from this fcblk. Next column blok must be accessible through
 *          fcblk[1].
 *
 * @param[in] A
 *          The pointer to the coeftab of the cblk.lcoeftab matrix storing the
 *          coefficients of the panel when the Lower part is computed,
 *          cblk.ucoeftab otherwise. Must be of size cblk.stride -by- cblk.width
 *
 * @param[in] B The pointer to the coeftab of the cblk.lcoeftab matrix storing
 *          the coefficients of the panel, if Symmetric/Hermitian cases or if
 *          upper part is computed; cblk.ucoeftab otherwise. Must be of size
 *          cblk.stride -by- cblk.width
 *
 * @param[in,out] C
 *          The pointer to the fcblk.lcoeftab if the lower part is computed,
 *          fcblk.ucoeftab otherwise.
 *
 * @param[in] work
 *          Temporary memory buffer.
 *
 * @param[in] tol
 *          Tolerance for low-rank compression kernels
 *
 *******************************************************************************
 *
 * @return
 *          The number of static pivoting during factorization of the diagonal
 *          block.
 *
 *******************************************************************************/
static inline void
core_zgemmsp_fulllr( int uplo, int trans,
                     const SolverCblk         *cblk,
                     const SolverBlok         *blok,
                           SolverCblk         *fcblk,
                     const pastix_complex64_t *A,
                     const pastix_complex64_t *B,
                           pastix_complex64_t *work,
                           double              tol)
{
    const SolverBlok *iterblok;
    const SolverBlok *fblok;
    const SolverBlok *lblok;
    pastix_lrblock_t lrA, lrB, *lrC;

    pastix_int_t stride;
    pastix_int_t M, N, K;
    int shift;

    assert(cblk->cblktype  & CBLK_DENSE);
    assert(!(fcblk->cblktype & CBLK_DENSE));
    assert(fcblk->cblktype & CBLK_SPLIT);

    shift = (uplo == PastixUpper) ? 1 : 0;
    stride  = cblk->stride;

    K = cblk_colnbr( cblk );
    N = blok_rownbr( blok );

    /* Get the B block and its dimensions */
    lrB.rk = -1;
    lrB.rkmax = (cblk->cblktype & CBLK_SPLIT) ? N : stride;
    lrB.u = (pastix_complex64_t*)B + blok->coefind; /* lrB is const, we can cast the B pointer */
    lrB.v = NULL;

    /**
     * Add contribution to C in fcblk:
     *    Get the first facing block of the distant panel, and the last block of
     *    the current cblk
     */
    fblok = fcblk->fblokptr;
    lblok = cblk[1].fblokptr;

    for (iterblok=blok+shift; iterblok<lblok; iterblok++) {

        /* Find facing blok */
        while (!is_block_inside_fblock( iterblok, fblok ))
        {
            fblok++;
            assert( fblok < fcblk[1].fblokptr );
        }

        /* Get the A block and its dimensions */
        M = blok_rownbr( iterblok );
        lrA.rk = -1;
        lrA.rkmax = (cblk->cblktype & CBLK_SPLIT) ? M : stride;
        lrA.u = (pastix_complex64_t*)A + iterblok->coefind; /* Same as for B */
        lrA.v = NULL;

        lrC = fblok->LRblock + shift;

        /* pastix_cblk_lock( fcblk ); */
        core_zlrmm( tol, PastixNoTrans, trans,
                    M, N, K,
                    blok_rownbr( fblok ), cblk_colnbr( fcblk ),
                    iterblok->frownum - fblok->frownum,
                    (blok->frownum - fcblk->fcolnum),
                    -1., &lrA, &lrB,
                     1., lrC,
                    work, -1,
                    fcblk );
        /* pastix_cblk_unlock( fcblk ); */
    }
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zgemmsp - Computes the updates associated to one off-diagonal block.
 *
 *******************************************************************************
 *
 * @param[in] uplo
 *          If uplo == PastixLower, the contribution of:
 *          (block .. (cblk[1].fblokptr-1)) -by- block is computed and added to
 *          C, otherwise the contribution:
 *          (block+1 .. (cblk[1].fblokptr-1)) -by- block is computed and added
 *          to C.
 *          The pointer to the data structure that describes the panel from
 *          which we compute the contributions. Next column blok must be
 *          accessible through cblk[1].
 *
 * @param[in] trans
 *          Specify the transposition used for the B matrix. It has to be either
 *          PastixTrans or PastixConjTrans.
 *
 * @param[in] cblk
 *          The cblk structure to which block belongs to. The A and B pointers
 *          must be the coeftab of this column block.
 *          Next column blok must be accessible through cblk[1].
 *
 * @param[in] blok
 *          The block from which we compute the contributions.
 *
 * @param[in] fcblk
 *          The pointer to the data structure that describes the panel on which
 *          we compute the contributions. The C pointer must be one of the
 *          oceftab from this fcblk. Next column blok must be accessible through
 *          fcblk[1].
 *
 * @param[in] A
 *          The pointer to the coeftab of the cblk.lcoeftab matrix storing the
 *          coefficients of the panel when the Lower part is computed,
 *          cblk.ucoeftab otherwise. Must be of size cblk.stride -by- cblk.width
 *
 * @param[in] B The pointer to the coeftab of the cblk.lcoeftab matrix storing
 *          the coefficients of the panel, if Symmetric/Hermitian cases or if
 *          upper part is computed; cblk.ucoeftab otherwise. Must be of size
 *          cblk.stride -by- cblk.width
 *
 * @param[in,out] C
 *          The pointer to the fcblk.lcoeftab if the lower part is computed,
 *          fcblk.ucoeftab otherwise.
 *
 * @param[in] work
 *          Temporary memory buffer.
 *
 * @param[in] tol
 *          Tolerance for low-rank compression kernels
 *
 *******************************************************************************
 *
 * @return
 *          The number of static pivoting during factorization of the diagonal
 *          block.
 *
 *******************************************************************************/
static inline void
core_zgemmsp_lr( int uplo, int trans,
                 const SolverCblk         *cblk,
                 const SolverBlok         *blok,
                       SolverCblk         *fcblk,
                       pastix_complex64_t *work,
                       double              tol )
{
    const SolverBlok *iterblok;
    const SolverBlok *fblok;
    const SolverBlok *lblok;

    pastix_complex64_t *C, *Cfull;
    pastix_int_t M, N, K, stridef;
    int shift;

    pastix_lrblock_t *lrA, *lrB;

    assert( !(cblk->cblktype & CBLK_DENSE) );
    assert( !(fcblk->cblktype & CBLK_DENSE) );
    assert( cblk->cblktype & CBLK_SPLIT );
    assert( fcblk->cblktype & CBLK_SPLIT );

    shift = (uplo == PastixUpper) ? 1 : 0;

    /* Move the Cfull pointer to the top of the right column */
    stridef = fcblk->stride;
    Cfull = (uplo == PastixUpper) ? fcblk->ucoeftab : fcblk->lcoeftab;
    Cfull = Cfull + (blok->frownum - fcblk->fcolnum) * stridef;

    /* Get the B block and its dimensions */
    lrB = (uplo == PastixUpper) ? blok->LRblock : blok->LRblock+1;
    K = cblk_colnbr( cblk );
    N = blok_rownbr( blok );

    /**
     * Add contribution to C in fcblk:
     *    Get the first facing block of the distant panel, and the last block of
     *    the current cblk
     */
    fblok = fcblk->fblokptr;
    lblok = cblk[1].fblokptr;

    /* for all following blocks in block column */
    for (iterblok=blok+shift; iterblok<lblok; iterblok++) {

        /* Find facing blok */
        while (!is_block_inside_fblock( iterblok, fblok ))
        {
            fblok++;
            assert( fblok < fcblk[1].fblokptr );
        }

        lrA = iterblok->LRblock + shift;
        M = blok_rownbr( iterblok );

        /* pastix_cblk_lock( fcblk ); */
        if ( fcblk->cblktype & CBLK_DENSE ) {
            C = Cfull + fblok->coefind + iterblok->frownum - fblok->frownum;
            core_zlrmge( tol, PastixNoTrans, trans,
                         M, N, K,
                         -1., lrA, lrB, 1., C, stridef,
                         work, -1,
                         fcblk );
        }
        else {
            core_zlrmm( tol, PastixNoTrans, trans,
                        M, N, K,
                        blok_rownbr( fblok ), cblk_colnbr( fcblk ),
                        iterblok->frownum - fblok->frownum,
                        (blok->frownum - fcblk->fcolnum),
                        -1., lrA, lrB,
                         1., fblok->LRblock + shift,
                        work, -1,
                        fcblk );
        }
        /* pastix_cblk_unlock( fcblk ); */
    }
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zgemmsp - Computes the updates associated to one off-diagonal block.
 *
 *******************************************************************************
 *
 * @param[in] uplo
 *          If uplo == PastixLower, the contribution of:
 *          (block .. (cblk[1].fblokptr-1)) -by- block is computed and added to
 *          C, otherwise the contribution:
 *          (block+1 .. (cblk[1].fblokptr-1)) -by- block is computed and added
 *          to C.
 *          The pointer to the data structure that describes the panel from
 *          which we compute the contributions. Next column blok must be
 *          accessible through cblk[1].
 *
 * @param[in] trans
 *          Specify the transposition used for the B matrix. It has to be either
 *          PastixTrans or PastixConjTrans.
 *
 * @param[in] cblk
 *          The cblk structure to which block belongs to. The A and B pointers
 *          must be the coeftab of this column block.
 *          Next column blok must be accessible through cblk[1].
 *
 * @param[in] blok
 *          The block from which we compute the contributions.
 *
 * @param[in] fcblk
 *          The pointer to the data structure that describes the panel on which
 *          we compute the contributions. The C pointer must be one of the
 *          oceftab from this fcblk. Next column blok must be accessible through
 *          fcblk[1].
 *
 * @param[in] A
 *          The pointer to the coeftab of the cblk.lcoeftab matrix storing the
 *          coefficients of the panel when the Lower part is computed,
 *          cblk.ucoeftab otherwise. Must be of size cblk.stride -by- cblk.width
 *
 * @param[in] B The pointer to the coeftab of the cblk.lcoeftab matrix storing
 *          the coefficients of the panel, if Symmetric/Hermitian cases or if
 *          upper part is computed; cblk.ucoeftab otherwise. Must be of size
 *          cblk.stride -by- cblk.width
 *
 * @param[in,out] C
 *          The pointer to the fcblk.lcoeftab if the lower part is computed,
 *          fcblk.ucoeftab otherwise.
 *
 * @param[in] work
 *          Temporary memory buffer.
 *
 * @param[in] tol
 *          Tolerance for low-rank compression kernels
 *
 *******************************************************************************
 *
 * @return
 *          The number of static pivoting during factorization of the diagonal
 *          block.
 *
 *******************************************************************************/
void core_zgemmsp( int uplo, int trans,
                   const SolverCblk         *cblk,
                   const SolverBlok         *blok,
                         SolverCblk         *fcblk,
                   const pastix_complex64_t *A,
                   const pastix_complex64_t *B,
                         pastix_complex64_t *C,
                         pastix_complex64_t *work,
                         double              tol )
{
    if ( !(fcblk->cblktype & CBLK_DENSE ) ) {
        if ( !(cblk->cblktype & CBLK_DENSE) ) {
            core_zgemmsp_lr( uplo, trans, cblk, blok, fcblk, work, tol );
        }
        else {
            core_zgemmsp_fulllr( uplo, trans,
                                 cblk, blok, fcblk,
                                 A, B, C, tol );
        }
    }
    else if ( fcblk->cblktype & CBLK_SPLIT ) {
        if ( cblk->cblktype & CBLK_SPLIT ) {
            core_zgemmsp_2d2d( uplo, trans,
                               cblk, blok, fcblk,
                               A, B, C );
        }
        else {
            core_zgemmsp_1d2d( uplo, trans,
                               cblk, blok, fcblk,
                               A, B, C );
        }
    }
    else {
        core_zgemmsp_1d1d( uplo, trans,
                           cblk, blok, fcblk,
                           A, B, C, work );
    }
}

