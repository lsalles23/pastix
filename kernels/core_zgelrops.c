/**
 *
 * @file core_zgelrops.c
 *
 *  PaStiX kernel routines
 *  PaStiX is a software package provided by Inria Bordeaux - Sud-Ouest,
 *  LaBRI, University of Bordeaux 1 and IPB.
 *
 * @version 1.0.0
 * @author Gregoire Pichon
 * @date 2016-23-03
 * @precisions normal z -> c d s
 *
 **/
#include "common.h"
#include <cblas.h>
#include <lapacke.h>
#include "blend/solver.h"
#include "pastix_zcores.h"

static pastix_complex64_t mzone = -1.;
static pastix_complex64_t zone  =  1.;
static pastix_complex64_t zzero =  0.;

#define PASTIX_DEBUG_LR

//#define PASTIX_LR_CHECKNAN
#if defined(PASTIX_LR_CHECKNAN)
#define LAPACKE_zlacpy_work LAPACKE_zlacpy
#define LAPACKE_zlaset_work LAPACKE_zlaset

#define LAPACKE_zunmlq_work( _layout_, _side_, _trans_, _m_, _n_, _k_, _a_, _lda_, _tau_, _c_, _ldc_, _w_, _ldw_ ) \
    LAPACKE_zunmlq( _layout_, _side_, _trans_, _m_, _n_, _k_, _a_, _lda_, _tau_, _c_, _ldc_ )
#define LAPACKE_zunmqr_work( _layout_, _side_, _trans_, _m_, _n_, _k_, _a_, _lda_, _tau_, _c_, _ldc_, _w_, _ldw_ ) \
    LAPACKE_zunmqr( _layout_, _side_, _trans_, _m_, _n_, _k_, _a_, _lda_, _tau_, _c_, _ldc_ )

#define LAPACKE_zgeqrf_work( _layout_, _m_, _n_, _a_, _lda_, _tau_, _w_, _ldw_ ) \
    LAPACKE_zgeqrf( _layout_, _m_, _n_, _a_, _lda_, _tau_ )
#define LAPACKE_zgelqf_work( _layout_, _m_, _n_, _a_, _lda_, _tau_, _w_, _ldw_ ) \
    LAPACKE_zgelqf( _layout_, _m_, _n_, _a_, _lda_, _tau_ )

#if defined(PRECISION_z) || defined(PRECISION_c)
#define MYLAPACKE_zgesvd_work( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, _w_, _ldw_, _rw_ ) \
    LAPACKE_zgesvd( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, (double*)(_w_) )
#else
#define MYLAPACKE_zgesvd_work( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, _w_, _ldw_, _rw_ ) \
    LAPACKE_zgesvd( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, (double*)(_w_) )
#endif

#else

#if defined(PRECISION_z) || defined(PRECISION_c)
#define MYLAPACKE_zgesvd_work( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, _w_, _ldw_, _rw_ ) \
    LAPACKE_zgesvd_work( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, _w_, _ldw_, _rw_ )
#else
#define MYLAPACKE_zgesvd_work( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, _w_, _ldw_, _rw_ ) \
    LAPACKE_zgesvd_work( _layout_, _jobu_, jobv_, _m_, _n_, _a_, _lda_, _s_, _u_, _ldu_, _v_, _ldv_, _w_, _ldw_ )
#endif

#endif /* defined(PASTIX_LR_CHECKNAN) */


int
core_zge2lr_RRQR( double tol, pastix_int_t m, pastix_int_t n,
                  const pastix_complex64_t *A, pastix_int_t lda,
                  pastix_lrblock_t *Alr )
{
    int ret;
    pastix_int_t i, j;

    pastix_int_t nb          = 32;
    pastix_int_t ldwork      = pastix_imax(m, n);
    pastix_complex64_t *work = malloc((2 * nb + 1) * ldwork * sizeof(pastix_complex64_t));;
    double *rwork            = malloc(2 * n * sizeof(double));
    pastix_int_t *jpvt       = malloc(n * sizeof(pastix_int_t));
    pastix_complex64_t *tau  = malloc(n * sizeof(pastix_complex64_t));
    pastix_complex64_t *Acpy = malloc(m * n * sizeof(pastix_complex64_t));

    /**
     * Allocate a temorary Low rank matrix
     */
    core_zlralloc( m, n, pastix_imin( m, n ), Alr );

    ret = LAPACKE_zlacpy_work(LAPACK_COL_MAJOR, 'A', m, n,
                              A, lda, Acpy, m );
    assert(ret == 0);

    ret = core_zrrqr(m, n,
                     Acpy, m,
                     jpvt, tau,
                     work, ldwork,
                     rwork,
                     tol, nb, pastix_imin(m,n)/3);

    /**
     * Resize the space used by the low rank matrix
     */
    /* if (ret * 2 > pastix_imin(m, n) || ret == -1){ */
    /*     printf("NOT SUPPORTED YET\n"); */
    /*     exit(1); */
    /* } */
    ret = core_zlrsze( 0, m, n, Alr, ret, -1 );

    /**
     * It was not interesting to compress, so we store the dense version in Alr
     */
    if (Alr->rk == -1 || Alr->rk == pastix_imin(m,n)/3) {
        Alr->u  = malloc( m * n * sizeof(pastix_complex64_t) );
        Alr->rk = -1;
        Alr->rkmax = m;

        ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', m, n,
                                   A, lda, Alr->u, Alr->rkmax );
        assert(ret == 0);
    }
    /**
     * We compute Q/R obtained thanks to core_zrrqr
     */
    else if (Alr->rk != 0) {

        /* Temporary space to permute Alr->v */
        pastix_complex64_t *work3;
        pastix_complex64_t *U = Alr->u;
        pastix_complex64_t *V = Alr->v;

        work3 = malloc(n * Alr->rk * sizeof(pastix_complex64_t));
        memset(work3, 0, n * Alr->rk * sizeof(pastix_complex64_t));

        ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'U', Alr->rk, n,
                                   Acpy, m,
                                   work3, Alr->rk );
        assert(ret == 0);

        /* Permute V */
        for (i=0; i<n; i++){
            memcpy(V + jpvt[i] * Alr->rk,
                   work3 + i * Alr->rk,
                   Alr->rk * sizeof(pastix_complex64_t));
        }

        /* Compute Q factor on u */
        ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', m, Alr->rk,
                                   Acpy, m, U, m );
        assert(ret == 0);

        ret = LAPACKE_zungqr( LAPACK_COL_MAJOR, m, Alr->rk, Alr->rk,
                              U , m, tau );
        assert(ret == 0);

        /* To check the resulting solution */
        if (0){
            cblas_zgemm(CblasColMajor, CblasNoTrans, CblasNoTrans,
                        m, n, Alr->rk,
                        CBLAS_SADDR(zone),  Alr->u, m,
                        Alr->v, Alr->rkmax,
                        CBLAS_SADDR(zzero), Acpy, m);

            int stop = 0;
            for (i=0; i<m; i++){
                for (j=0; j<n; j++){
                    pastix_complex64_t v1 = A[lda*j+i];
                    pastix_complex64_t v2 = Acpy[m*j+i];
                    if (fabs(v1-v2) > 1e-15){
                        printf("A_orig %f A_comp %f INDEX %ld %ld PIV %ld %ld\n",
                               v1, v2, i, j, jpvt[i], jpvt[j]);
                        stop = 1;
                    }
                }
            }

            double norm_A_orig = LAPACKE_zlange_work( LAPACK_COL_MAJOR, 'f', m, n,
                                                      A, lda, NULL );
            double norm_A_comp = LAPACKE_zlange_work( LAPACK_COL_MAJOR, 'f', m, n,
                                                      Acpy, m, NULL );

            core_zgeadd( PastixNoTrans, m, n,
                         -1., A, lda,
                         1., Acpy, m );

            double norm_diff = LAPACKE_zlange_work( LAPACK_COL_MAJOR, 'f', m, n,
                                                    Acpy, m, NULL );

            printf("NORMS: ORIG %.10g COMP %.10g DIFF %.20g\n\n",
                   norm_A_orig, norm_A_comp, norm_diff);

            if (stop){
                exit(1);

                ret = LAPACKE_zlacpy_work(LAPACK_COL_MAJOR, 'A', m, n,
                                          A, lda, Acpy, m );
                assert(ret == 0);
                Alr->u  = Acpy;
                Alr->rk = -1;
                Alr->rkmax = m;
            }
        }
    }

    free(work);
    free(rwork);
    free(jpvt);
    free(tau);
    free(Acpy);

    return Alr->rk;
}

int
core_zrrqr( pastix_int_t m, pastix_int_t n,
            pastix_complex64_t *A, pastix_int_t lda,
            pastix_int_t *jpvt, pastix_complex64_t *tau,
            pastix_complex64_t *work, pastix_int_t ldwork,
            double *rwork,
            double tol, pastix_int_t nb, pastix_int_t maxrank){

    pastix_int_t minMN = pastix_imin(m, n);
    pastix_int_t ldf   = ldwork;
    pastix_int_t j, k, jb, itemp, lsticc, pvt;
    double temp, temp2;
    double machine_prec = sqrt(LAPACKE_dlamch('e'));
    pastix_complex64_t akk;

    pastix_complex64_t *auxv, *f;

    /* Partial (VN1) and exact (VN2) column norms */
    double *VN1, *VN2;

    /* Number or rows of A that have been factorized */
    pastix_int_t offset = 0;

    /* Rank */
    pastix_int_t rk = 0;


    if (m < 0)
        return -1;
    if (n < 0)
        return -2;
    if (lda < pastix_imax(1, m))
        return -4;
    if( ldwork < n)
        return -8;

    VN1 = rwork;
    VN2 = rwork + n;

    auxv = work;
    f    = work + ldwork;

    /* Initialize partial column norms. The first N elements of work */
    /* store the exact column norms. */
    /* TODO: call PLASMA/internal kernel */
    for (j=0; j<n; j++){
        VN1[j]  = cblas_dznrm2(m, A + j*lda, 1);
        VN2[j]  = VN1[j];
        jpvt[j] = j;
    }

    while(rk < maxrank){
        /* jb equivalent to kb in LAPACK xLAQPS: number of columns actually factorized */
        jb     = pastix_imin(nb, minMN-offset);
        lsticc = 0;

        /* column being factorized among jb */
        k = 0;

        while(k < jb && lsticc == 0){

            rk = offset+k;

            pvt = rk + cblas_izamax(n-rk, VN1 + rk, 1);

            if (VN1[pvt] < tol){
                return rk;
            }

            /* Rank is too large for compression */
            if (rk > maxrank){
                return rk;
            }

            /* Pivot is not within the current column: we swap */
            if (pvt != rk){
                assert( (pvt < n) && (rk < n) );
                cblas_zswap(m, A + pvt * lda, 1, A + rk * lda, 1);
                cblas_zswap(k, f + (pvt-offset), ldf, f + k, ldf);

                itemp     = jpvt[pvt];
                jpvt[pvt] = jpvt[rk];
                jpvt[rk]  = itemp;
                VN1[pvt]  = VN1[rk];
                VN2[pvt]  = VN2[rk];
            }

            /* Apply previous Householder reflectors to column K */
            /* A(RK:M,RK) := A(RK:M,RK) - A(RK:M,OFFSET+1:RK-1)*F(K,1:K-1)**H */
            if (k > 0){
#if defined(PRECISION_c) || defined(PRECISION_z)
                for (j=0; j<k; j++){
                    f[j * ldf + k] = conj(f[j * ldf + k]);
                }
#endif

                assert( (offset+1) < n );
                assert( (rk < n) && (rk < m) );
                cblas_zgemv(CblasColMajor, CblasNoTrans, m-rk, k, CBLAS_SADDR(mzone),
                            A + (offset) * lda + rk, lda,
                            f + k, ldf,
                            CBLAS_SADDR(zone), A + rk * lda + rk, 1);

#if defined(PRECISION_c) || defined(PRECISION_z)
                for (j=0; j<k; j++){
                    f[j * ldf + k] = conj(f[j * ldf + k]);
                }
#endif
            }

            /* Generate elementary reflector H(k). */
            if (rk < (m-1)){
                LAPACKE_zlarfg(m-rk, A + rk * lda + rk, A + rk * lda + (rk+1), 1, tau + rk);
            }
            else{
                LAPACKE_zlarfg(1, A + rk * lda + rk, A + rk * lda + rk, 1, tau + rk);
            }

            akk = A[rk * lda + rk];
            A[rk * lda + rk] = zone;

            /* Compute Kth column of F: */
            /* F(K+1:N,K) := tau(K)*A(RK:M,K+1:N)**H*A(RK:M,K). */
            if (rk < (n-1)){
                pastix_complex64_t alpha = tau[rk];
                cblas_zgemv(CblasColMajor, CblasConjTrans, m-rk, n-rk-1, CBLAS_SADDR(alpha),
                            A + (rk+1) * lda + rk, lda,
                            A + rk * lda + rk, 1,
                            CBLAS_SADDR(zzero), f + k * ldf + k + 1, 1);
            }

            /* Padding F(1:K,K) with zeros. */
            for (j=0; j<k; j++){
                f[k * ldf + j] = zzero;
            }

            /* Incremental updating of F: */
            /* F(1:N,K) := F(1:N-OFFSET,K) - tau(RK)*F(1:N,1:K-1)*A(RK:M,OFFSET+1:RK-1)**H*A(RK:M,RK). */
            if (k > 0){
                pastix_complex64_t alpha = -tau[rk];
                cblas_zgemv(CblasColMajor, CblasConjTrans, m-rk, k, CBLAS_SADDR(alpha),
                            A + (offset) * lda + rk, lda,
                            A + rk * lda + rk, 1,
                            CBLAS_SADDR(zzero), auxv, 1);

                cblas_zgemv(CblasColMajor, CblasNoTrans, n-offset, k, CBLAS_SADDR(zone),
                            f, ldf,
                            auxv, 1,
                            CBLAS_SADDR(zone), f + k * ldf, 1);
            }

            /* Update the current row of A: */
            /* A(RK,RK+1:N) := A(RK,RK+1:N) - A(RK,OFFSET+1:RK)*F(K+1:N,1:K)**H. */
            if (rk < (n-1)){

#if defined(PRECISION_c) || defined(PRECISION_z)

#else
                cblas_zgemv(CblasColMajor, CblasNoTrans, n-rk-1, k+1, CBLAS_SADDR(mzone),
                            f + (k+1), ldf,
                            A + (offset) * lda + rk, lda,
                            CBLAS_SADDR(zone), A + (rk+1) * lda + rk, lda);
#endif
            }

            /* Update partial column norms. */
            if (rk < (minMN-1)){
                for (j=rk+1; j<n; j++){
                    if (VN1[j] != 0.0){
                        /* NOTE: The following 4 lines follow from the analysis in */
                        /* Lapack Working Note 176. */
                        temp  = fabs( A[j * lda + rk] ) / VN1[j];
                        double temp3 = (1.0 + temp) * (1.0 - temp);
                        if (temp3 > 0.0){
                            temp = temp3;
                        }
                        else{
                            temp = 0.;
                        }
                        temp2 = temp * ( VN1[j] / VN2[j]) * ( VN1[j] / VN2[j]);
                        if (temp2 < machine_prec){
                            /* printf("LSTICC %ld\n", j); */
                            VN2[j] = lsticc;
                            lsticc = j;
                        }
                        else{
                            VN1[j] = VN1[j] * sqrt(temp);
                        }

                    }
                }
            }

            A[rk * lda + rk] = akk;

            k++;
        }

        /* Apply the block reflector to the rest of the matrix: */
        /* A(RK+1:M,RK+1:N) := A(RK+1:M,RK+1:N) - */
        /* A(RK+1:M,OFFSET+1:RK)*F(K+1:N-OFFSET,1:K)**H. */
        if (rk < (minMN-1)){
            cblas_zgemm(CblasColMajor, CblasNoTrans, CblasConjTrans,
                        m-rk-1, n-rk-1, k,
                        CBLAS_SADDR(mzone), A + (offset) * lda + rk + 1, lda,
                        f + k, ldf,
                        CBLAS_SADDR(zone), A + (rk+1) * lda + rk + 1, lda);
        }

        /* Recomputation of difficult columns. */
        while (lsticc > 0){
            itemp = (pastix_int_t) (VN2[lsticc]);
            assert(lsticc < n);
            VN1[lsticc] = cblas_dznrm2(m-rk-1, A + (lsticc) * lda + rk + 1, 1);

            /* NOTE: The computation of VN1( LSTICC ) relies on the fact that  */
            /* SNRM2 does not fail on vectors with norm below the value of */
            /* SQRT(DLAMCH('S'))  */
            VN2[lsticc] = VN1[lsticc];
            lsticc = itemp;
        }

        lsticc = 0;
        offset = rk+1;
    }

    return rk;
}


int
core_zlralloc( pastix_int_t M, pastix_int_t N, pastix_int_t rkmax,
               pastix_lrblock_t *A )
{
    pastix_complex64_t *u, *v;

    if ( rkmax == -1 ) {
        u = malloc( M * N * sizeof(pastix_complex64_t) );
        memset(u, 0, M * N * sizeof(pastix_complex64_t) );
        A->rk = -1;
        A->rkmax = M;
        A->u = u;
        A->v = NULL;
    }
    else {
#if defined(PASTIX_DEBUG_LR)
        u = malloc( M * rkmax * sizeof(pastix_complex64_t) );
        v = malloc( N * rkmax * sizeof(pastix_complex64_t) );

        /* To avoid uninitialised values in valgrind. Lapacke doc (xgesvd) is not correct */
        memset(u, 0, M * rkmax * sizeof(pastix_complex64_t));
        memset(v, 0, N * rkmax * sizeof(pastix_complex64_t));
#else
        u = malloc( (M+N) * rkmax * sizeof(pastix_complex64_t));

        /* To avoid uninitialised values in valgrind. Lapacke doc (xgesvd) is not correct */
        memset(u, 0, (M+N) * rkmax * sizeof(pastix_complex64_t));

        v = u + M * rkmax;
#endif

        A->rk = 0;
        A->rkmax = rkmax;
        A->u = u;
        A->v = v;
    }

    return 0;
}

int
core_zlrfree( pastix_lrblock_t *A )
{
    if ( A->rk == -1 ) {
        free(A->u);
        A->u = NULL;
    }
    else {
        free(A->u);
#if defined(PASTIX_DEBUG_LR)
        free(A->v);
#endif
        A->u = NULL;
        A->v = NULL;
    }
    A->rk = 0;
    A->rkmax = 0;

    return 0;
}

int
core_zlrsze( int copy, pastix_int_t M, pastix_int_t N,
             pastix_lrblock_t *A, int newrk, int newrkmax )
{
    pastix_int_t minmn = pastix_imin( M, N );

    newrkmax = (newrkmax == -1) ? newrk : newrkmax;

    /**
     * It is not interesting to compress, so we alloc space to store the full matrix
     */
    if ( (newrk * 2) > minmn )
    {
        A->u = realloc( A->u, M * N * sizeof(pastix_complex64_t) );
#if defined(PASTIX_DEBUG_LR)
        free(A->v);
#endif
        A->v = NULL;
        A->rk = -1;
        A->rkmax = M;
        return -1;
    }
    /**
     * The rank is null, we free everything
     */
    else if (newrkmax == 0)
    {
        /**
         * The rank is nul, we free everything
         */
        free(A->u);
#if defined(PASTIX_DEBUG_LR)
        free(A->v);
#endif
        A->u = NULL;
        A->v = NULL;
        A->rkmax = newrkmax;
        A->rk = newrk;
    }
    /**
     * The rank is non null, we allocate the correct amount of space, and
     * compress the stored information if necessary
     */
    else {
        pastix_complex64_t *u, *v;
        int ret;

        if ( newrkmax != A->rkmax ) {
#if defined(PASTIX_DEBUG_LR)
            u = malloc( M * newrkmax * sizeof(pastix_complex64_t) );
            v = malloc( N * newrkmax * sizeof(pastix_complex64_t) );
#else
            u = malloc( (M+N) * newrkmax * sizeof(pastix_complex64_t) );
            v = u + M * newrkmax;
#endif
            if ( copy ) {
                ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', M, newrk,
                                           A->u, M, u, M );
                assert(ret == 0);
                ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', newrk, N,
                                           A->v, A->rkmax, v, newrkmax );
                assert(ret == 0);
            }
            free(A->u);
#if defined(PASTIX_DEBUG_LR)
            free(A->v);
#endif
            A->u = u;
            A->v = v;
        }
        A->rk = newrk;
        A->rkmax = newrkmax;

        (void)ret;
    }
    assert( A->rk <= A->rkmax);
    return 0;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_z_compress_LR - Compresses a dense block into a u v^T LR structure.
 *
 *******************************************************************************
 *
 * @param[in] fL
 *          Pointer to the dense structure of size dimb * dima
 *          Leading dimension is stride
 *
 * @param[out] u
 *          Pointer to the u factor of LR representation of size dimb * rank
 *          Leading dimension is ldu
 *
 * @param[out] v
 *          Pointer to the v factor of LR representation of size dima * rank
 *          Leading dimension is ldv
 *          Note that due to LAPACKE_zgesvd this block is stored transposed
 *
 *
 *******************************************************************************
 *
 * @return
 *          The rank of the compressed structure.
 *
 *******************************************************************************/
int
core_zge2lrx(double tol, pastix_int_t m, pastix_int_t n,
             const pastix_complex64_t *A, pastix_int_t lda,
             pastix_lrblock_t *Alr )
{
    pastix_complex64_t *u, *v, *zwork, *Acpy, ws;
    double             *rwork, *s, tolabs, tolrel;
    pastix_int_t        i, ret, ldu, ldv;
    pastix_int_t        minMN = pastix_imin( m, n );
    pastix_int_t        lwork = -1;
    pastix_int_t        zsize, rsize;

#if !defined(NDEBUG)
    if ( m < 0 ) {
        return -2;
    }
    if ( n < 0 ) {
        return -3;
    }
    if ( lda < m ) {
        return -5;
    }
    if ( (Alr->u == NULL) || (Alr->v == NULL) || (Alr->rkmax < minMN) ) {
        return -6;
    }
#endif

    u = Alr->u;
    v = Alr->v;
    ldu = m;
    ldv = Alr->rkmax;

    /**
     * Query the workspace needed for the gesvd
     */
#if defined(PASTIX_LR_CHECKNAN)
    ws = minMN;
#else
    ret = MYLAPACKE_zgesvd_work( LAPACK_COL_MAJOR, 'S', 'S',
                                 m, n, NULL, m,
                                 NULL, NULL, ldu, NULL, ldv,
                                 &ws, lwork, NULL );
#endif
    lwork = ws;
    zsize = ws;
    zsize += m * n; /* Copy of the matrix A */

    rsize = minMN;
#if defined(PRECISION_z) || defined(PRECISION_c)
    rsize += 5 * minMN;
#endif

    zwork = malloc( zsize * sizeof(pastix_complex64_t) + rsize * sizeof(double) );
    rwork = (double*)(zwork + zsize);

    Acpy = zwork + lwork;
    s    = rwork;

    /**
     * Backup the original matrix before to overwrite it with the SVD
     */
    ret = LAPACKE_zlacpy_work(LAPACK_COL_MAJOR, 'A', m, n,
                              A, lda, Acpy, m );
    assert( ret == 0 );

    ret = MYLAPACKE_zgesvd_work( LAPACK_COL_MAJOR, 'S', 'S',
                                 m, n, Acpy, m,
                                 s, u, ldu, v, ldv,
                                 zwork, lwork, rwork + minMN );
    assert(ret == 0);
    if( ret != 0 ){
        errorPrint("SVD Failed\n");
        EXIT(MOD_SOPALIN, INTERNAL_ERR);
    }

    tolrel = tol * s[0];
    tolabs = tol * tol;
    for (i=0; i<minMN; i++, v+=1){
        if ( (s[i] >= tolabs) &&
             (s[i] >= tolrel) )
        /* if (s[i] > tol) */
        {
            cblas_zdscal(n, s[i], v, ldv);
        }
        else {
            break;
        }
    }
    Alr->rk = i;

    free(zwork);
    return i;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zge2lr - Convert a full rank matrix in a low rank matrix.
 *
 *******************************************************************************
 *
 * @param[in] tol
 *          The tolerance used as a criterai to eliminate information from the
 *          full rank matrix
 *
 * @param[in] m
 *          Number of rows of the matrix A, and of the low rank matrix Alr.
 *
 * @param[in] n
 *          Number of columns of the matrix A, and of the low rank matrix Alr.
 *
 * @param[in] A
 *          The matrix of dimension lda-by-n that need to be compressed
 *
 * @param[in] lda
 *          The leading dimension of the matrix A. lda >= max(1, m)
 *
 * @param[out] Alr
 *          The low rank matrix structure that will store the low rank
 *          representation of A
 *
 *******************************************************************************/
int
core_zge2lr_SVD( double tol, pastix_int_t m, pastix_int_t n,
                 const pastix_complex64_t *A, pastix_int_t lda,
                 pastix_lrblock_t *Alr )
{
    int ret;
    /**
     * Allocate a temorary Low rank matrix
     */
    core_zlralloc( m, n, pastix_imin( m, n ), Alr );

    /**
     * Compress the dense matrix with the temporary space just allocated
     */
    ret = core_zge2lrx( tol, m, n, A, lda, Alr );

    if ( ret < 0 ) {
        core_zlrfree( Alr );
        return ret;
    }

    /**
     * Resize the space used by the low rank matrix
     */
    ret = core_zlrsze( 1, m, n, Alr, ret, -1 );

    /**
     * It was not interesting to compress, so we store the dense version in Alr
     */
    if (ret == -1) {
        ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', m, n,
                                   A, lda, Alr->u, Alr->rkmax );
        assert(ret == 0);
    }

    return 0;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zlr2ge - Convert a low rank matrix into a dense matrix.
 *
 *******************************************************************************
 *
 * @param[in] m
 *          Number of rows of the matrix A, and of the low rank matrix Alr.
 *
 * @param[in] n
 *          Number of columns of the matrix A, and of the low rank matrix Alr.
 *
 * @param[in] Alr
 *          The low rank matrix to be converted into a dense matrix
 *
 * @param[out] A
 *          The matrix of dimension lda-by-n in which to store the uncompressed
 *          version of Alr.
 *
 * @param[in] lda
 *          The leading dimension of the matrix A. lda >= max(1, m)
 *
 *******************************************************************************/
int
core_zlr2ge( pastix_int_t m, pastix_int_t n,
             const pastix_lrblock_t *Alr,
             pastix_complex64_t *A, pastix_int_t lda )
{
    int ret;

#if !defined(NDEBUG)
    if ( m < 0 ) {
        return -1;
    }
    if ( n < 0 ) {
        return -2;
    }
    if (Alr == NULL || Alr->rk > Alr->rkmax) {
        return -3;
    }
    if ( lda < m ) {
        return -5;
    }
    if ( Alr->rk == -1 ) {
        if (Alr->u == NULL || Alr->v != NULL || Alr->rkmax < m) {
            return -6;
        }
    }
    else if ( Alr->rk != 0){
        if (Alr->u == NULL || Alr->v == NULL) {
            return -6;
        }
    }
#endif

    if ( Alr->rk == -1 ) {
        ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', m, n,
                                   Alr->u, Alr->rkmax, A, lda );
        assert( ret == 0 );
    }
    else if ( Alr->rk == 0 ) {
        ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', m, n,
                                   0., 0., A, lda );
        assert( ret == 0 );
    }
    else {
        cblas_zgemm(CblasColMajor, CblasNoTrans, CblasNoTrans,
                    m, n, Alr->rk,
                    CBLAS_SADDR(zone),  Alr->u, m,
                                        Alr->v, Alr->rkmax,
                    CBLAS_SADDR(zzero), A, lda);
    }

    return 0;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_z_add_LR - Adds two LR structure u1 v1^T and (-u2) v2^T into u1 v1^T
 *
 *    u1v1^T + u2v2^T = (u1 u2) (v1 v2)^T
 *    Compute QR decomposition of (u1 u2) = Q1 R1
 *    Compute QR decomposition of (v1 v2) = Q2 R2
 *    Compute SVD of R1 R2^T = u \sigma v^T
 *    Final solution is (Q1 u \sigma^[1/2]) (Q2 v \sigma^[1/2])^T
 *
 *******************************************************************************
 *
 * @param[in, out] u1 v1
 *          LR structure where v1 is stored transposed
 *          u1 factor of size dim_u1 * rank_1 with ld_u1 as leading dimension
 *          v1 factor of size dim_v1 * rank_1 with ld_v1 as leading dimension
 *
 * @param[in] u2 v2
 *          Pointer to the u factor of LR representation of size dimb * rank
 *          Leading dimension is ldu
 *
 * @param[in] x2, y2
 *          Position where u2 v2 is added into u1 v1 (which is larger)
 *
 *
 *******************************************************************************
 *
 * @return
 *          The new rank of u1 v1^T or -1 if ranks are too large for recompression
 *
 *******************************************************************************/
int
core_zrradd( double tol, int transA1, pastix_complex64_t alpha,
             pastix_int_t M1, pastix_int_t N1, const pastix_lrblock_t *A,
             pastix_int_t M2, pastix_int_t N2,       pastix_lrblock_t *B,
             pastix_int_t offx, pastix_int_t offy)
{
    pastix_int_t rank, M, N, minU, minV;
    pastix_int_t i, ret, lwork, new_rank;
    pastix_int_t ldau, ldav, ldbu, ldbv;
    pastix_complex64_t *u1u2, *v1v2, *R, *u, *v;
    pastix_complex64_t *tmp, *zbuf, *tauU, *tauV;
    pastix_complex64_t  querysize;
    double *s, tolabs, tolrel;
    size_t wzsize, wdsize;

    rank = (A->rk == -1) ? pastix_imin(M1, N1) : A->rk;
    rank += B->rk;
    M = pastix_imax(M2, M1);
    N = pastix_imax(N2, N1);
    minU = pastix_imin(M, rank);
    minV = pastix_imin(N, rank);

    assert(M2 == M && N2 == N);
    assert(B->rk != -1);

    assert( A->rk <= A->rkmax);
    assert( B->rk <= B->rkmax);

    if ( ((M1 + offx) > M2) ||
         ((N1 + offy) > N2) )
    {
        errorPrint("Dimensions are not correct");
        assert(0 /* Incorrect dimensions */);
        return -1;
    }

    /**
     * A is rank null, nothing to do
     */
    if (A->rk == 0) {
        return rank;
    }

    ldau = (A->rk == -1) ? A->rkmax : M1;
    ldav = (transA1 == PastixNoTrans) ? A->rkmax : N1;
    ldbu = M;
    ldbv = B->rkmax;

    /**
     * Let's handle case where B is a null matrix
     *   B = alpha A
     */
    if (B->rk == 0) {
        if ( A->rk == -1 ) {
            /**
             * TODO: This case can be improved by compressing A, and then
             * copying it into B, however the criteria to keep A compressed or
             * not must be based on B dimension, and not on A ones
             */
            u = malloc( M * N * sizeof(pastix_complex64_t) );

            if ( M1 != M || N1 != N ) {
                LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', M, N,
                                     0., 0., u, M );
            }
            ret = core_zgeadd( PastixNoTrans, M1, N1,
                               alpha, A->u, ldau,
                               0., u + M * offy + offx, M );
            assert(ret == 0);

            core_zge2lr( tol, M, N, u, M, B );
            free(u);
        }
        else {
            core_zlralloc( M, N, A->rkmax, B );
            u = B->u;
            v = B->v;
            B->rk = A->rk;

            if ( M1 != M ) {
                LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', M, B->rk,
                                     0., 0., u, M );
            }
            if ( N1 != N ) {
                LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', B->rk, N,
                                     0., 0., v, B->rkmax );
            }

            ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', M1, A->rk,
                                       A->u, ldau,
                                       u + offx, M );
            assert(ret == 0);

            ret = core_zgeadd( transA1, A->rk, N1,
                               alpha, A->v, ldav,
                               0., v + B->rkmax * offy, B->rkmax );
            assert(ret == 0);
        }
        assert( B->rk <= B->rkmax);
        return 0;
    }

    /**
     * The rank is too big, let's try to compress
     */
    if ( rank > pastix_imin( M, N ) ) {
        assert(0);
    }

    /**
     * Let's compute the size of the workspace
     */
    /* u1u2 and v1v2 */
    wzsize = (M+N) * rank;
    /* tauU and tauV */
    wzsize += minU + minV;

    /* Storage of R, u and v */
    wzsize += 3 * rank * rank;

    /* QR/LQ workspace */
    lwork = pastix_imax( M, N ) * 32;

    /* Workspace needed for the gesvd */
#if defined(PASTIX_LR_CHECKNAN)
    querysize = rank;
#else
    ret = MYLAPACKE_zgesvd_work( LAPACK_COL_MAJOR, 'S', 'S',
                                 rank, rank, NULL, rank,
                                 NULL, NULL, rank, NULL, rank,
                                 &querysize, -1, NULL);
#endif
    lwork = pastix_imax( lwork, querysize );
    wzsize += lwork;

    wdsize = rank;
#if defined(PRECISION_z) || defined(PRECISION_c)
    wdsize += 5 * rank;
#endif

    zbuf = malloc( wzsize * sizeof(pastix_complex64_t) + wdsize * sizeof(double) );
    s    = (double*)(zbuf + wzsize);

    u1u2 = zbuf + lwork;
    tauU = u1u2 + M * rank;
    v1v2 = tauU + minU;
    tauV = v1v2 + N * rank;
    R    = tauV + minV;

    /**
     * Concatenate U2 and U1 in u1u2
     *  [ u2  0  ]
     *  [ u2  u1 ]
     *  [ u2  0  ]
     */
    //u1u2 = malloc( M * rank * sizeof(pastix_complex64_t));
    LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', M, B->rk,
                         B->u, ldbu, u1u2, M );

    tmp = u1u2 + B->rk * M;
    if ( A->rk == -1 ) {
        /**
         * A is full of rank M1, so A will be integrated into v1v2
         */
        if ( M1 < N1 ) {
            if (M1 != M2) {
                /* Set to 0 */
                memset(tmp, 0, M * M1 * sizeof(pastix_complex64_t));

                /* Set diagonal */
                tmp += offx;
                for (i=0; i<M1; i++, tmp += M+1) {
                    *tmp = 1.;
                }
            }
            else {
                assert( offx == 0 );
                ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', M, M1,
                                           0., 1., tmp, M );
                assert( ret == 0 );
            }
        }
        else {
            /**
             * A is full of rank N1, so A is integrated into u1u2
             */
            if (M1 != M) {
                memset(tmp, 0, M * N1 * sizeof(pastix_complex64_t));
            }
            ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', M1, N1,
                                       A->u, ldau, tmp + offx, M );
            assert(ret == 0);
        }
    }
    /**
     * A is low rank of rank A->rk
     */
    else {
        if (M1 != M) {
            memset(tmp, 0, M * A->rk * sizeof(pastix_complex64_t));
        }
        ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', M1, A->rk,
                                   A->u, ldau, tmp + offx, M );
        assert(ret == 0);
    }

    /**
     * Perform QR factorization on u1u2 = (Q1 R1)
     */
    //tauU = malloc( minU * sizeof(pastix_complex64_t));
    ret = LAPACKE_zgeqrf_work( LAPACK_COL_MAJOR, M, rank,
                               u1u2, M, tauU, zbuf, lwork );
    assert( ret == 0 );

    /**
     * Concatenate V2 and V1 in v1v2
     *  [ v2^h v2^h v2^h ]
     *  [ 0    v1^h 0    ]
     */
    //v1v2 = malloc( N * rank * sizeof(pastix_complex64_t));
    ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', B->rk, N,
                               B->v, ldbv, v1v2, rank );
    assert(ret == 0);

    tmp = v1v2 + B->rk;
    if ( A->rk == -1 ) {
        assert( transA1 == PastixNoTrans );
        /**
         * A is full of rank M1, so it is integrated into v1v2
         */
        if ( M1 < N1 ) {
            if (N1 != N) {
                ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', M1, N,
                                           0., 0., tmp, rank );
                assert( ret == 0 );
            }
            core_zgeadd( PastixNoTrans, M1, N1,
                         alpha, A->u, ldau,
                         0., tmp + offy * rank, rank );
        }
        /**
         * A is full of rank N1, so it has been integrated into u1u2
         */
        else {
            if (N1 != N2) {
                /* Set to 0 */
                ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', N1, N,
                                           0., 0., tmp, rank );
                assert(ret == 0);

                /* Set diagonal */
                tmp += offy * rank;
                for (i=0; i<N1; i++, tmp += rank+1) {
                    *tmp = alpha;
                }
            }
            else {
                assert( offy == 0 );
                ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', N1, N,
                                           0., alpha, tmp + offy * rank, rank );
                assert( ret == 0 );
            }
        }
    }
    /**
     * A is low rank of rank A->rk
     */
    else {
        if (N1 != N) {
            ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', A->rk, N,
                                       0., 0., tmp, rank );
            assert(ret == 0);
        }
        core_zgeadd( transA1, A->rk, N1,
                     alpha, A->v,              ldav,
                        0., tmp + offy * rank, rank );
    }

    /**
     * Perform LQ factorization on v1v2 = (L2 Q2)
     */
    //tauV = malloc( minV * sizeof(pastix_complex64_t));
    ret = LAPACKE_zgelqf_work( LAPACK_COL_MAJOR, rank, N,
                               v1v2, rank, tauV, zbuf, lwork );
    assert(ret == 0);
    /**
     * Compute R = alpha R1 L2
     */
    //R = malloc( 3 * rank * rank * sizeof(pastix_complex64_t));
    u = R + rank * rank;
    v = u + rank * rank;

    memset(R, 0, rank * rank * sizeof(pastix_complex64_t));

    ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'U', rank, rank,
                               u1u2, M, R, rank );
    assert(ret == 0);

    cblas_ztrmm(CblasColMajor,
                CblasRight, CblasLower,
                CblasNoTrans, CblasNonUnit,
                rank, rank, CBLAS_SADDR(zone),
                v1v2, rank, R, rank);

    /**
     * Compute svd(R) = u \sigma v^t
     */
    ret = MYLAPACKE_zgesvd_work( CblasColMajor, 'S', 'S',
                                 rank, rank, R, rank,
                                 s, u, rank, v, rank,
                                 zbuf, lwork, s + rank );
    assert(ret == 0);
    if (ret != 0) {
        errorPrint("LAPACKE_zgesvd FAILED");
        EXIT(MOD_SOPALIN, INTERNAL_ERR);
    }

    /**
     * Let's compute the new rank of the result
     */
    tmp = v;
    tolrel = tol * s[0];
    tolabs = tol * tol;
    for (i=0; i<rank; i++, tmp+=1){
        if ( (s[i] >= tolabs) &&
             (s[i] >= tolrel) )
        /* if (s[i] > tol) */
        {
            cblas_zdscal(rank, s[i], tmp, rank);
        }
        else {
            break;
        }
    }
    new_rank = i;

    /**
     * First case: The rank is too big, so we decide to uncompress the result
     */
    if ( new_rank*2 > pastix_imin( M, N ) ) {
        pastix_lrblock_t Bbackup = *B;

        core_zlralloc( M, N, -1, B );
        u = B->u;

        /* Uncompress B */
        cblas_zgemm(CblasColMajor, CblasNoTrans, CblasNoTrans,
                    M, N, Bbackup.rk,
                    CBLAS_SADDR(zone),  Bbackup.u, ldbu,
                                        Bbackup.v, ldbv,
                    CBLAS_SADDR(zzero), u, M );

        /* Add A into it */
        if ( A->rk == -1 ) {
            core_zgeadd( transA1, M1, N1,
                         alpha, A->u, ldau,
                         zone, u + offy * M + offx, M);
        }
        else {
            cblas_zgemm(CblasColMajor, CblasNoTrans, transA1,
                        M1, N1, A->rk,
                        CBLAS_SADDR(alpha), A->u, ldau,
                                            A->v, ldav,
                        CBLAS_SADDR(zone), u + offy * M + offx, M);
        }
        core_zlrfree(&Bbackup);
        free(zbuf);
        return 0;
    }
    else if ( new_rank == 0 ) {
        core_zlrfree(B);
        free(zbuf);
        return 0;
    }

    /**
     * We need to reallocate the buffer to store the new compressed version of B
     * because it wasn't big enough
     */
    ret = core_zlrsze( 0, M, N, B, new_rank, -1 );
    assert( ret != -1 );
    assert( B->rkmax >= new_rank );
    assert( B->rkmax >= B->rk    );

    ldbv = B->rkmax;

    /**
     * Let's now compute the final U = Q1 ([u] \sigma)
     *                                     [0]
     */
#if defined(PASTIX_LR_CHECKNAN)
    ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', M, new_rank,
                               0., 0., B->u, ldbu );
    assert(ret == 0);
    ret = LAPACKE_zlacpy( LAPACK_COL_MAJOR, 'A', rank, new_rank,
                          u, rank, B->u, ldbu );
    assert(ret == 0);
#else
    tmp = B->u;
    for (i=0; i<new_rank; i++, tmp+=ldbu, u+=rank) {
        memcpy(tmp, u,              rank * sizeof(pastix_complex64_t));
        memset(tmp + rank, 0, (M - rank) * sizeof(pastix_complex64_t));
    }
#endif

    ret = LAPACKE_zunmqr_work(LAPACK_COL_MAJOR, 'L', 'N',
                              M, new_rank, minU,
                              u1u2, M, tauU,
                              B->u, ldbu,
                              zbuf, lwork);
    assert( ret == 0 );

    /**
     * And the final V^T = [v^t 0 ] Q2
     */
    tmp = B->v;
    ret = LAPACKE_zlacpy_work( LAPACK_COL_MAJOR, 'A', new_rank, rank,
                               v, rank, B->v, ldbv );
    assert( ret == 0 );

    ret = LAPACKE_zlaset_work( LAPACK_COL_MAJOR, 'A', new_rank, N-rank,
                               0., 0., tmp + ldbv * rank, ldbv );
    assert( ret == 0 );

    ret = LAPACKE_zunmlq_work(LAPACK_COL_MAJOR, 'R', 'N',
                              new_rank, N, minV,
                              v1v2, rank, tauV,
                              B->v, ldbv,
                              zbuf, lwork);
    assert( ret == 0 );

    free(zbuf);
    //free(R);
    return new_rank;
}

int
core_zgradd( double tol, pastix_complex64_t alpha,
             pastix_int_t M1, pastix_int_t N1, pastix_complex64_t *A, pastix_int_t lda,
             pastix_int_t M2, pastix_int_t N2, pastix_lrblock_t *B,
             pastix_int_t offx, pastix_int_t offy)
{
    pastix_lrblock_t lrA;
    pastix_int_t rmax = pastix_imin( M2, N2 );
    pastix_int_t rank, ldub;

    assert( B->rk <= B->rkmax);

    if ( B->rk == -1 ) {
        pastix_complex64_t *tmp = B->u;
        ldub = B->rkmax;
        tmp += ldub * offy + offx;
        core_zgeadd( CblasNoTrans, M1, N1,
                     alpha, A,   lda,
                       1.0, tmp, ldub );
        return 0;
    }

    ldub = M2;

    /**
     * The rank is too big, we need to uncompress/compress B
     */
    if ( ((B->rk + M1) > rmax) &&
         ((B->rk + N1) > rmax) )
    {
        pastix_complex64_t *work = malloc( M2 * N2 * sizeof(pastix_complex64_t) );
        assert(B->rk > 0);

        cblas_zgemm( CblasColMajor, CblasNoTrans, CblasNoTrans,
                     M2, N2, B->rk,
                     CBLAS_SADDR(zone),  B->u, ldub,
                                         B->v, B->rkmax,
                     CBLAS_SADDR(zzero), work, M2 );

        core_zgeadd( PastixNoTrans, M1, N1,
                     alpha, A, lda,
                     1.,    work + M2 * offy + offx, M2 );

        core_zlrfree(B);
        core_zge2lr( tol, M2, N2, work, M2, B );
        rank = B->rk;
        free(work);
    }
    /**
     * We consider the A matrix as Id * A or A *Id
     */
    else {
        lrA.rk = -1;
        lrA.rkmax = lda;
        lrA.u = A;
        lrA.v = NULL;
        rank = core_zrradd( tol, PastixNoTrans, alpha,
                            M1, N1, &lrA,
                            M2, N2, B,
                            offx, offy );
    }

    assert( B->rk <= B->rkmax);
    return rank;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zlrm2 - Computes the product of two low rank matrices and returns the result in AB
 *
 *******************************************************************************/
int core_zlrm2( int transA, int transB,
                pastix_int_t M, pastix_int_t N, pastix_int_t K,
                const pastix_lrblock_t *A,
                const pastix_lrblock_t *B,
                pastix_lrblock_t *AB,
                pastix_complex64_t *work,
                pastix_int_t ldwork )
{
    pastix_int_t ldau, ldav, ldbu, ldbv;
    int transV = PastixNoTrans;

    assert( A->rk  <= A->rkmax);
    assert( B->rk  <= B->rkmax);
    assert(transA == PastixNoTrans);
    assert(transB != PastixNoTrans);

    /* Quick return if multiplication by 0 */
    if ( A->rk == 0 || B->rk == 0 ) {
        AB->rk = 0;
        AB->rkmax = 0;
        AB->u = NULL;
        AB->v = NULL;
        return transV;
    }

    ldau = (A->rk == -1) ? A->rkmax : M;
    ldav = A->rkmax;
    ldbu = (B->rk == -1) ? B->rkmax : N;
    ldbv = B->rkmax;

    if ( A->rk != -1 ) {
        /**
         * A and B are both low rank
         */
        if ( B->rk != -1 ) {
            /**
             * Let's compute A * B' = Au Av^h (Bu Bv^h)' with the smallest ws
             */
            if ( (A->rk * N) <= (B->rk * M) ) {
                /**
                 *    ABu = Au
                 *    ABv = (Av^h Bv^h') * Bu'
                 */
                assert( (A->rk * ( N + B->rk )) <= ldwork );
                AB->rk = A->rk;
                AB->rkmax = A->rk;
                AB->u = A->u;
                AB->v = work + A->rk * B->rk;

                cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                             A->rk, B->rk, K,
                             CBLAS_SADDR(zone),  A->v, ldav,
                                                 B->v, ldbv,
                             CBLAS_SADDR(zzero), work, A->rk );

                cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                             A->rk, N, B->rk,
                             CBLAS_SADDR(zone),  work,  A->rk,
                                                 B->u,  ldbu,
                             CBLAS_SADDR(zzero), AB->v, AB->rkmax );
            }
            else {
                /**
                 *    ABu = Au * (Av^h Bv^h')
                 *    ABv = Bu'
                 */
                assert( (B->rk * ( M + A->rk )) <= ldwork );
                AB->rk = B->rk;
                AB->rkmax = B->rk;
                AB->u = work + A->rk * B->rk;
                AB->v = B->u;

                cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                             A->rk, B->rk, K,
                             CBLAS_SADDR(zone),  A->v, ldav,
                                                 B->v, ldbv,
                             CBLAS_SADDR(zzero), work, A->rk );

                cblas_zgemm( CblasColMajor, CblasNoTrans, CblasNoTrans,
                             M, B->rk, A->rk,
                             CBLAS_SADDR(zone),  A->u,  ldau,
                                                 work,  A->rk,
                             CBLAS_SADDR(zzero), AB->u, M );

                transV = transB;
            }
        }
        /**
         * A is low rank and not B
         */
        else {
            /**
             * Let's compute A * B' = Au Av^h B' by computing only Av^h * B'
             *   ABu = Au
             *   ABv = Av^h B'
             */
            assert( (A->rk * N) <= ldwork );
            AB->rk = A->rk;
            AB->rkmax = A->rk;
            AB->u = A->u;
            AB->v = work;

            cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                         A->rk, N, K,
                         CBLAS_SADDR(zone),  A->v,  ldav,
                                             B->u,  ldbu,
                         CBLAS_SADDR(zzero), AB->v, AB->rkmax );
        }
    }
    else {
        /**
         * B is low rank and not A
         */
        if ( B->rk != -1 ) {
            /**
             * Let's compute A * B' = A * (Bu Bv^h)' by computing only A * Bv^h'
             *   ABu = A * Bv^h'
             *   ABv = Bu'
             */
            assert( (B->rk * M) <= ldwork );
            AB->rk = B->rk;
            AB->rkmax = B->rk;
            AB->u = work;
            AB->v = B->u;

            cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                         M, B->rk, K,
                         CBLAS_SADDR(zone),  A->u,  ldau,
                                             B->v,  ldbv,
                         CBLAS_SADDR(zzero), AB->u, M );

            transV = transB;
        }
        /**
         * A and B are both full rank
         */
        else {
            /**
             * A and B are both full
             *  Let's compute the product to add the full matrix
             * TODO: return low rank matrix:
             *             AB.u = A, AB.v = B' when K is small
             */
            /* if ( 2*K < pastix_imin( M, N ) ) { */
            /*     AB->rk = K; */
            /*     AB->rkmax = B->rkmax; */
            /*     AB->u = A->u; */
            /*     AB->v = B->u; */
            /*     transV = transB; */
            /* } */
            /* else { */
                assert( (M * N) <= ldwork );
                AB->rk = -1;
                AB->rkmax = M;
                AB->u = work;
                AB->v = NULL;

                cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                             M, N, K,
                             CBLAS_SADDR(zone),  A->u, ldau,
                                                 B->u, ldbu,
                             CBLAS_SADDR(zzero), work, M );
            /* } */
        }
    }
    assert( AB->rk <= AB->rkmax);
    return transV;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zlrm3 - A * B with two Low rank matrices
 *
 *******************************************************************************/
int core_zlrm3( double tol,
                int transA, int transB,
                pastix_int_t M, pastix_int_t N, pastix_int_t K,
                const pastix_lrblock_t *A,
                const pastix_lrblock_t *B,
                pastix_lrblock_t *AB )
{
    pastix_int_t ldau, ldav, ldbu, ldbv;
    int transV = PastixNoTrans;
    pastix_complex64_t *work2;
    pastix_lrblock_t rArB;

    assert( A->rk  <= A->rkmax);
    assert( B->rk  <= B->rkmax);
    assert(transA == PastixNoTrans);
    assert(transB != PastixNoTrans);

    /* Quick return if multiplication by 0 */
    if ( A->rk == 0 || B->rk == 0 ) {
        AB->rk = 0;
        AB->rkmax = 0;
        AB->u = NULL;
        AB->v = NULL;
        return transV;
    }

    ldau = (A->rk == -1) ? A->rkmax : M;
    ldav = A->rkmax;
    ldbu = (B->rk == -1) ? B->rkmax : N;
    ldbv = B->rkmax;

    work2 = malloc( A->rk * B->rk * sizeof(pastix_complex64_t));


    /**
     * Let's compute A * B' = Au Av^h (Bu Bv^h)' with the smallest ws
     */
    cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                 A->rk, B->rk, K,
                 CBLAS_SADDR(zone),  A->v, ldav,
                                     B->v, ldbv,
                 CBLAS_SADDR(zzero), work2, A->rk );

    /**
     * Try to compress (Av^h Bv^h')
     */
    core_zge2lr( tol, A->rk, B->rk, work2, A->rk, &rArB );

    /**
     * The rank of AB is not smaller than min(rankA, rankB)
     */
    if (rArB.rk == -1){
        if ( A->rk < B->rk ) {
            /**
             *    ABu = Au
             *    ABv = (Av^h Bv^h') * Bu'
             */
            pastix_complex64_t *work = malloc( A->rk * N * sizeof(pastix_complex64_t));

            //assert( (A->rk * ( N + B->rk )) <= lwork );
            AB->rk = A->rk;
            AB->rkmax = A->rk;
            AB->u = A->u;
            AB->v = work;

            cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                         A->rk, N, B->rk,
                         CBLAS_SADDR(zone),  work2,  A->rk,
                         B->u,  ldbu,
                         CBLAS_SADDR(zzero), AB->v, AB->rkmax );
        }
        else {
            /**
             *    ABu = Au * (Av^h Bv^h')
             *    ABv = Bu'
             */
            pastix_complex64_t *work = malloc( B->rk * M * sizeof(pastix_complex64_t));

            //assert( (B->rk * ( M + A->rk )) <= lwork );
            AB->rk = B->rk;
            AB->rkmax = B->rk;
            AB->u = work;
            AB->v = B->u;

            cblas_zgemm( CblasColMajor, CblasNoTrans, CblasNoTrans,
                         M, B->rk, A->rk,
                         CBLAS_SADDR(zone),  A->u,  ldau,
                         work2,  A->rk,
                         CBLAS_SADDR(zzero), AB->u, M );

            transV = transB;

            /* free(work); */
        }
    }
    else if (rArB.rk == 0){
        AB->rk    = 0;
        AB->rkmax = 0;
        AB->u = NULL;
        AB->v = NULL;
    }
    /**
     * The rank of AB is smaller than min(rankA, rankB)
     */
    else{
        pastix_complex64_t *work = malloc( (M + N) * rArB.rk * sizeof(pastix_complex64_t));

        AB->rk    = rArB.rk;
        AB->rkmax = rArB.rk;
        AB->u = work;
        AB->v = work + M * rArB.rk;

        cblas_zgemm( CblasColMajor, CblasNoTrans, CblasNoTrans,
                     M, rArB.rk, A->rk,
                     CBLAS_SADDR(zone),  A->u,   ldau,
                                         rArB.u, A->rk,
                     CBLAS_SADDR(zzero), AB->u,  M );

        cblas_zgemm( CblasColMajor, CblasNoTrans, transB,
                     rArB.rk, N, B->rk,
                     CBLAS_SADDR(zone),  rArB.v, rArB.rkmax,
                                         B->u, ldbu,
                     CBLAS_SADDR(zzero), AB->v, rArB.rk );

        /* free(work); */
    }
    core_zlrfree(&rArB);
    free(work2);
    return transV;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zlrmm - A * B + C with three Low rank matrices
 *
 *******************************************************************************/
int
core_zlrmm( double tol, int transA, int transB,
            pastix_int_t M, pastix_int_t N, pastix_int_t K,
            pastix_int_t Cm, pastix_int_t Cn,
            pastix_int_t offx, pastix_int_t offy,
            pastix_complex64_t alpha, const pastix_lrblock_t *A,
                                      const pastix_lrblock_t *B,
            pastix_complex64_t beta,        pastix_lrblock_t *C,
            pastix_complex64_t *work, pastix_int_t ldwork,
            SolverCblk *fcblk )
{
    pastix_complex64_t *tmp = NULL;
    pastix_lrblock_t AB;
    pastix_int_t ldabu, ldabv, ldcu, ldcv;
    pastix_int_t required = 0;
    int transV;
    int allocated = 0;

    assert(transA == PastixNoTrans);
    assert(transB != PastixNoTrans);
    assert( A->rk <= A->rkmax);
    assert( B->rk <= B->rkmax);
    assert( C->rk <= C->rkmax);

    /* Quick return if multiplication by 0 */
    if ( A->rk == 0 || B->rk == 0 ) {
        return 0;
    }

    if ( A->rk != -1 ) {
        if ( B->rk != -1 ) {
            required = 0;
            /* required = pastix_imin( A->rk * ( N + B->rk ), */
            /*                         B->rk * ( M + A->rk ) ); */
        }
        else {
            required = A->rk * N;
        }
    }
    else {
        if ( B->rk != -1 ) {
            required = B->rk * M;
        }
        else {
            required = M * N;
        }
    }

    if ( required <= ldwork ) {
        tmp = work;
    }
    else {
        tmp = malloc( required * sizeof(pastix_complex64_t));
        allocated = 1;
    }

    if (A->rk != -1 && B->rk != -1){
        /* Note that in this case tmp is not used anymore */
        /* For instance, AB.rk != -1 */
        transV = core_zlrm3( tol, transA, transB, M, N, K,
                             A, B, &AB );

        if (AB.rk == 0)
            return 0;
    }
    else{
        transV = core_zlrm2( transA, transB, M, N, K,
                             A, B, &AB, tmp, required );
    }

    pastix_cblk_lock( fcblk );

    ldabu = (AB.rk == -1) ? AB.rkmax : M;
    ldabv = (transV == PastixNoTrans) ? AB.rkmax : N;
    ldcu = (C->rk == -1) ? C->rkmax : Cm;
    ldcv = C->rkmax;

    /**
     * The destination matrix is full rank
     */
    if (C->rk == -1) {
        pastix_complex64_t *Cptr = C->u;
        Cptr += ldcu * offy + offx;

        if ( AB.rk == -1 ) {
            core_zgeadd( PastixNoTrans, M, N,
                         alpha, AB.u, ldabu,
                         beta,  Cptr, ldcu );
        }
        else {
            cblas_zgemm( CblasColMajor, CblasNoTrans, transV,
                         M, N, AB.rk,
                         CBLAS_SADDR(alpha), AB.u, ldabu,
                                             AB.v, ldabv,
                         CBLAS_SADDR(beta),  Cptr, ldcu );
        }
    }
     /**
     * The destination matrix is low rank
     */
    else {
        if ( AB.rk == -1 ) {
            assert(beta == 1.);
            core_zgradd( tol, alpha,
                         M, N, tmp, AB.rkmax,
                         Cm, Cn, C,
                         offx, offy );
        }
        else {
            if ( AB.rk + C->rk > pastix_imin(M, N) ) {
                pastix_complex64_t *work = malloc( Cm * Cn * sizeof(pastix_complex64_t) );

                /* Do not uncompress a null LR structure */
                if (C->rk > 0){
                    /* Uncompress C */
                    cblas_zgemm( CblasColMajor, CblasNoTrans, CblasNoTrans,
                                 Cm, Cn, C->rk,
                                 CBLAS_SADDR(beta),  C->u, ldcu,
                                                     C->v, ldcv,
                                 CBLAS_SADDR(zzero), work, Cm );
                }
                else{
                    memset(work, 0, Cm * Cn * sizeof(pastix_complex64_t) );
                }

                /* Add A*B */
                cblas_zgemm( CblasColMajor, CblasNoTrans, transV,
                             M, N, AB.rk,
                             CBLAS_SADDR(alpha), AB.u, ldabu,
                                                 AB.v, ldabv,
                             CBLAS_SADDR(zone), work + Cm * offy + offx, Cm );

                core_zlrfree(C);
                core_zge2lr( tol, Cm, Cn, work, Cm, C );
                free(work);
            }
            else {
                /* Need to handle correctly this case */
                core_zrradd( tol, transV, alpha,
                             M, N, &AB,
                             Cm, Cn, C,
                             offx, offy );
            }
        }
    }
    pastix_cblk_unlock( fcblk );

    if ( allocated ) {
        free(tmp);
    }

    assert( C->rk <= C->rkmax);
    return 0;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_kernel
 *
 * core_zlrmge - A * B + C with A, and B Low rank matrices, and C full rank
 *
 *******************************************************************************/
int
core_zlrmge( double tol, int transA, int transB,
             pastix_int_t M, pastix_int_t N, pastix_int_t K,
             pastix_complex64_t alpha, const pastix_lrblock_t *A,
                                       const pastix_lrblock_t *B,
             pastix_complex64_t beta, pastix_complex64_t *C, int ldc,
             pastix_complex64_t *work, pastix_int_t ldwork,
             SolverCblk *fcblk )
{
    pastix_lrblock_t lrC;

    lrC.rk = -1;
    lrC.rkmax = ldc;
    lrC.u = C;
    lrC.v = NULL;

    core_zlrmm( tol, transA, transB, M, N, K,
                M, N, 0, 0,
                alpha, A, B, beta, &lrC,
                work, ldwork,
                fcblk );

    return 0;
}

int core_zge2lr( double tol, pastix_int_t m, pastix_int_t n,
                 const pastix_complex64_t *A, pastix_int_t lda,
                 void *Alr ){
    if ( compress_method == SVD ){
        return core_zge2lr_SVD(tol, m, n, A, lda, Alr);
    }
    else{
        return core_zge2lr_RRQR(tol, m, n, A, lda, Alr);
    }
}
