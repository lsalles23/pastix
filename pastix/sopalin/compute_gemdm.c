#define PastixNoTrans   111
#define PastixTrans     112
#define PastixConjTrans 113

int API_CALL(CORE_gemdm)(int transA, int transB,
                         pastix_int_t M, pastix_int_t N, pastix_int_t K,
                         pastix_float_t alpha, pastix_float_t *A, int LDA,
                         pastix_float_t *B, int LDB,
                         pastix_float_t beta, pastix_float_t *C, int LDC,
                         pastix_float_t *D, int incD,
                         pastix_float_t *WORK, int LWORK)
{
    static int iun = 1;
    pastix_int_t j; /*, Am, Bm;*/
    pastix_float_t delta;
    pastix_float_t *wD, *w;
    char *tA = ( transA == PastixNoTrans ) ? "N"
        : (( transA == PastixTrans ) ? "T" : "C" );
    char *tB = ( transB == PastixNoTrans ) ? "N"
        : (( transB == PastixTrans ) ? "T" : "C" );

    /* Am = (transA == PastixNoTrans ) ? M : K; */
    /* Bm = (transB == PastixNoTrans ) ? K : N; */

    /* /\* Check input arguments *\/ */
    /* if ((transA != PastixNoTrans) && (transA != PastixTrans) && (transA != PastixConjTrans)) { */
    /*     coreblas_error(1, "Illegal value of transA"); */
    /*     return -1; */
    /* } */
    /* if ((transB != PastixNoTrans) && (transB != PastixTrans) && (transB != PastixConjTrans)) { */
    /*     coreblas_error(2, "Illegal value of transB"); */
    /*     return -2; */
    /* } */
    /* if (M < 0) { */
    /*     coreblas_error(3, "Illegal value of M"); */
    /*     return -3; */
    /* } */
    /* if (N < 0) { */
    /*     coreblas_error(4, "Illegal value of N"); */
    /*     return -4; */
    /* } */
    /* if (K < 0) { */
    /*     coreblas_error(5, "Illegal value of K"); */
    /*     return -5; */
    /* } */
    /* if ((LDA < max(1,Am)) && (Am > 0)) { */
    /*     coreblas_error(8, "Illegal value of LDA"); */
    /*     return -8; */
    /* } */
    /* if ((LDB < max(1,Bm)) && (Bm > 0)) { */
    /*     coreblas_error(10, "Illegal value of LDB"); */
    /*     return -10; */
    /* } */
    /* if ((LDC < max(1,M)) && (M > 0)) { */
    /*     coreblas_error(13, "Illegal value of LDC"); */
    /*     return -13; */
    /* } */
    /* if ( incD < 0 ) { */
    /*     coreblas_error(15, "Illegal value of incD"); */
    /*     return -15; */
    /* } */
    if ( ( ( transA == PastixNoTrans ) && ( LWORK < (M+1)*K) ) ||
         ( ( transA != PastixNoTrans ) && ( LWORK < (N+1)*K) ) ){
        fprintf(stderr, "CORE_gemdm: Illegal value of LWORK\n");
        if (transA == PastixNoTrans )
            errorPrint("LWORK %d < (M=%d+1)*K=%d ", LWORK, M, K);
        if (transA == PastixNoTrans )
            errorPrint("LWORK %d < (N=%d+1)*K=%d ", LWORK, N, K);
        return -17;
    }

    /* Quick return */
    if (M == 0 || N == 0 ||
        ((alpha == 0.0 || K == 0) && beta == 1.0) ) {
        return 0;
    }

    if ( incD == 1 ) {
        wD = D;
    } else {
        wD = WORK;
        SOPALIN_COPY(K, D, incD, wD, iun);
    }
    w = WORK + K;

    /*
     * transA == PastixNoTrans
     */
    if ( transA == PastixNoTrans )
    {
        /* WORK = A * D */
      for (j=0; j<K; j++, wD++) {
            delta = *wD;
            SOPALIN_COPY(M, &A[LDA*j], iun, &w[M*j], iun);
            SOPALIN_SCAL(M, delta,          &w[M*j], iun);
        }

        /* C = alpha * WORK * op(B) + beta * C */
        SOPALIN_GEMM( "N", tB,
                      M, N, K,
                      alpha, w, M,
                             B, LDB,
                      beta,  C, LDC);
    }
    else
    {
        if ( transB == PastixNoTrans ) /* Worst case*/
        {
            /* WORK = (D * B)' */
            for (j=0; j<K; j++, wD++) {
                delta = *wD;
                SOPALIN_COPY(N, &B[j], LDB, &w[N*j], iun);
                SOPALIN_SCAL(N, delta,      &w[N*j], iun);
            }

            /* C = alpha * op(A) * WORK' + beta * C */
            SOPALIN_GEMM(tA, "T",
                         M, N, K,
                         alpha, A, LDA,
                                w, N,
                         beta,  C, LDC);
        }
        else
        {
#ifdef COMPLEX
            /* if ( transB == PastixConjTrans ) */
            /* { */
            /*     /\* WORK = D * B' *\/ */
            /*   for (j=0; j<K; j++, wD++) { */
            /*         delta = *wD; */
            /*         SOPALIN_COPY(N, &B[LDB*j], iun, &w[N*j], iun); */
            /*         LAPACKE_zlacgv_work(N,          &w[N*j], iun); */
            /*         SOPALIN_SCAL(N, delta,          &w[N*j], iun); */
            /*     } */
            /* } */
            /* else  */
#endif
            {
                /* WORK = D * B' */
              for (j=0; j<K; j++, wD++) {
                    delta = *wD;
                    SOPALIN_COPY(N, &B[LDB*j], iun, &w[N*j], iun);
                    SOPALIN_SCAL(N, delta,          &w[N*j], iun);
                }
            }

            /* C = alpha * op(A) * WORK + beta * C */
            SOPALIN_GEMM(tA, "N",
                         M, N, K,
                         alpha, A, LDA,
                                w, N,
                         beta,  C, LDC);
        }
    }
    return 0;
}
