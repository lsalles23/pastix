/**
 *
 * @file pastix_subtask_order.c
 *
 * PaStiX ordering task.
 * Contains wrappers to build a good ordering for sparse direct solvers.
 * Affected by the compilation time options:
 *    - HAVE_SCOTCH: Enable Scotch graph partitioning library.
 *    - HAVE_PTSCOTCH: Enable PT-Scotch graph partitioning library.
 *    - HAVE_METIS: Enable Metis graph partitioning library.
 *
 * @copyright 2015-2017 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 * @version 6.0.0
 * @author Xavier Lacoste
 * @author Pierre Ramet
 * @author Mathieu Faverge
 * @date 2013-06-24
 *
 **/
#include "common.h"
#include "spm.h"
#include "graph.h"
#include "order.h"

/**
 *******************************************************************************
 *
 * @ingroup pastix_analyze
 *
 * @brief Computes the ordering of the given graph in parameters.
 *
 * The graph given by the user is used to generate a graph that can be used by
 * ordering tools and symbolic factorization. This graph is stored in the
 * pastix_data->graph to be pass over to the symbolic factorization. If it exists
 * before to call this routine, then the current structure is cleaned and a new
 * one is created. From this structure, an ordering is computed by the ordering
 * tool chosen by IPARM_ORDERING and stored in pastix_data->ordemesh. At the end
 * the full ordering stucture: pemutation, inverse permutation, partition, and
 * partion tree is generated such that it can be used by any symbolic
 * factorization algorithm. It is however recommended to use Kass to perform
 * assembly if (PT-)Scotch was not used as the ordering algorithm.
 * The user can get back the permutation generated by providing allocated ordering
 * structure where the results is stored after computation.
 *
 * This routine is affected by the following parameters:
 *   IPARM_VERBOSE, IPARM_ORDERING, IPARM_IO_STRATEGY
 *
 *******************************************************************************
 *
 * @param[inout] pastix_data
 *          The pastix_data structure that describes the solver instance.
 *          On exit, the field ordemesh is initialize with the result of the
 *          ordering.
 *          - IPARM_ORDERING will determine which ordering tool is used.
 *          - IPARM_IO_STRATEGY will enable the results to be written on files
 *          if set to PastixIOSave, or the results to be directly loaded from
 *          file if set to PastixIOLoad without going through an ordering
 *          library.
 *          - If the function pastix_setSchurUnknownList() function has been
 *          previously called to set the list of vertices to isolate in the
 *          schur complement, those vertices are isolated at the end of the
 *          matrix in a dedicated supernode..
 *          - If the function pastix_setZerosUnknownList() function has been
 *          previously called to set the list of diagonal elements that may
 *          cause problem during the factorization, those vertices are isolated
 *          at the end of the matrix in a dedicated supernode..
 *
 * @param[in] spm
 *          The sparse matrix given by the user on which the ordering will be
 *          computed.
 *
 *
 * @param[inout] myorder
 *          On entry, the permutation provide by the user if IPARM_ORDERING
 *          parameter set to PatixOrderPersonal. Not read otherwise.
 *          On exit, if the stucture attributs != NULL and IPARM_ORDERING parameter
 *          is not set to PastixOrderPersonal, contains the permutation generated.
 *          Otherwise, it is not referenced.
 *
 *******************************************************************************
 *
 * @retval PASTIX_SUCCESS on successful exit,
 * @retval PASTIX_ERR_BADPARAMETER if one parameter is incorrect,
 * @retval PASTIX_ERR_OUTOFMEMORY if one allocation failed,
 * @retval PASTIX_ERR_INTEGER_TYPE if Scotch integer type is not the
 *         same size as PaStiX ones,
 * @retval PASTIX_ERR_INTERNAL if an error occurs internally to Scotch.
 *
 *******************************************************************************/
int
pastix_subtask_order(       pastix_data_t *pastix_data,
                      const pastix_spm_t  *spm,
                            Order         *myorder )
{
    pastix_int_t    n;
    pastix_int_t    schur_n;
    pastix_int_t   *schur_colptr;
    pastix_int_t   *schur_rows;
    pastix_int_t   *schur_perm;
    pastix_int_t    zeros_n;
    pastix_int_t   *zeros_colptr;
    pastix_int_t   *zeros_rows;
    pastix_int_t   *zeros_perm;
    pastix_int_t   *iparm;
    pastix_graph_t  subgraph;
    pastix_graph_t *graph;
    Order          *ordemesh;
    Clock           timer;
    int             procnum;
    int             retval = PASTIX_SUCCESS;
    int             retval_rcv;
    int             do_schur = 1;
    int             do_zeros = 1;

    /*
     * Check parameters
     */
    if (pastix_data == NULL) {
        errorPrint("pastix_subtask_order: wrong pastix_data parameter");
        return PASTIX_ERR_BADPARAMETER;
    }
    if (spm == NULL) {
        errorPrint("pastix_subtask_order: wrong spm parameter");
        return PASTIX_ERR_BADPARAMETER;
    }
    iparm = pastix_data->iparm;
    n = spm->n;

    if ( !(pastix_data->steps & STEP_INIT) ) {
        errorPrint("pastix_subtask_order: pastixInit() has to be called before calling this function");
        return PASTIX_ERR_BADPARAMETER;
    }

    /*
     * Backup flttype from the spm into iparm[IPARM_FLOAT] for later use
     */
    iparm[IPARM_FLOAT] = spm->flttype;

    if (pastix_data->schur_n > 0)
    {
        /*
         * If ordering is set to PastixOrderPersonal or PastixOrderLoad, we
         * consider that the schur complement is already isolated at the end of
         * permutation array
         */
        if ((iparm[IPARM_ORDERING] == PastixOrderPersonal) ||
            (iparm[IPARM_ORDERING] == PastixOrderLoad)) {
            do_schur = 0;
        }
    } else {
        do_schur = 0;
    }
    if (pastix_data->zeros_n > 0)
    {
        /*
         * If ordering is set to PastixOrderPersonal or PastixOrderLoad, we
         * consider that the zeros on diagonal are already isolated at the end of
         * permutation array
         */
        if ((iparm[IPARM_ORDERING] == PastixOrderPersonal) ||
            (iparm[IPARM_ORDERING] == PastixOrderLoad) ) {
            do_zeros = 0;
        }
    } else {
        do_zeros = 0;
    }

    /*
     * Clean ordering if it exists
     */
    if (pastix_data->ordemesh != NULL) {
        orderExit(pastix_data->ordemesh);
    } else {
        MALLOC_INTERN( pastix_data->ordemesh, 1, Order );
    }

    ordemesh = pastix_data->ordemesh;
    procnum  = pastix_data->procnum;
    orderAlloc( ordemesh, 0, 0 );

    if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
        pastix_print(procnum, 0, "%s", OUT_STEP_ORDER);

    /*
     * Prepare a copy of user's SPM
     * Copy the given spm in pastix_data structure and performs basic required
     * operations as symmetrizing the graph and removing the diagonal
     * coefficients
     */
    graphPrepare( pastix_data, spm, &(pastix_data->graph) );
    graph = pastix_data->graph;

    /*
     * Isolate Shur elements
     */
    if ( do_schur )
    {
        assert( graph->loc2glob == NULL );
        assert( pastix_data->schur_list != NULL );
        graphIsolate(graph->n,
                     graph->colptr,
                     graph->rows,
                     pastix_data->schur_n,
                     pastix_data->schur_list,
                     &schur_colptr,
                     &schur_rows,
                     &schur_perm,
                     NULL);

        schur_n = graph->n - pastix_data->schur_n;
    } else {
        schur_n      = graph->n;
        schur_colptr = graph->colptr;
        schur_rows   = graph->rows;
    }

    /*
     * Isolate diagonal elements close to 0.
     */
    if ( do_zeros )
    {
        assert( graph->loc2glob == NULL );
        assert( pastix_data->zeros_list != NULL );
        graphIsolate(schur_n,
                     schur_colptr,
                     schur_rows,
                     pastix_data->zeros_n,
                     pastix_data->zeros_list,
                     &zeros_colptr,
                     &zeros_rows,
                     &zeros_perm,
                     NULL);

        zeros_n = schur_n - pastix_data->zeros_n;
    } else {
        zeros_n      = schur_n;
        zeros_colptr = schur_colptr;
        zeros_rows   = schur_rows;
    }

    if (iparm[IPARM_VERBOSE] > PastixVerboseYes)
        pastix_print(procnum, 0, "%s", OUT_ORDER_INIT);

    clockStart(timer);

    subgraph.gN       = graph->gN;
    subgraph.n        = zeros_n;
    subgraph.colptr   = zeros_colptr;
    subgraph.rows     = zeros_rows;
    subgraph.loc2glob = graph->loc2glob;


    /* Select the ordering method chosen by the user */
    switch (iparm[IPARM_ORDERING]) {
        /*
         * Scotch Ordering
         */
    case PastixOrderScotch:
        if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
            pastix_print(procnum, 0, OUT_ORDER_METHOD, "Scotch" );
#if defined(HAVE_SCOTCH)
        retval = orderComputeScotch( pastix_data, &subgraph );
#else
        errorPrint("pastix_subtask_order: Ordering with Scotch requires to enable -DPASTIX_ORDERING_SCOTCH option");
        retval = PASTIX_ERR_BADPARAMETER;
#endif
        break;

        /*
         * PT-Scotch Ordering
         */
    case PastixOrderPtScotch:
        if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
            pastix_print(procnum, 0, OUT_ORDER_METHOD, "PT-Scotch" );
#if defined(HAVE_PTSCOTCH)
        retval = orderComputePTScotch( pastix_data, &subgraph );
#else
        errorPrint("pastix_subtask_order: Ordering with PT-Scotch requires to enable -DPASTIX_ORDERING_PTSCOTCH option");
        retval = PASTIX_ERR_BADPARAMETER;
#endif
        break;

        /*
         *  METIS ordering
         */
    case PastixOrderMetis:
        if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
            pastix_print(procnum, 0, OUT_ORDER_METHOD, "Metis" );
#if defined(HAVE_METIS)
        retval = orderComputeMetis( pastix_data, &subgraph );
        assert( ordemesh->rangtab == NULL );
#else
        errorPrint("pastix_subtask_order: Ordering with Metis requires -DHAVE_METIS flag at compile time");
        retval = PASTIX_ERR_BADPARAMETER;
#endif
        break;

        /*
         * Personal Ordering
         */
    case PastixOrderPersonal:
        {
            pastix_int_t i, n;

            n = spm->gN;
            /* Personal ordering have to be global ordering */
            assert( spm->gN == spm->n );

            orderAlloc(ordemesh, n, 0);

            /* Rebase the Personal ordering to 0 */
            if ( myorder != NULL ) {
                assert( myorder != NULL );
                assert( myorder->vertnbr == n );
                orderBase(myorder, 0);
            }

            if ( (myorder == NULL) || (myorder->permtab == NULL) ) {
                if ( (myorder == NULL) || (myorder->peritab == NULL) ) {
                    if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
                        pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (identity)" );
                    for(i=0; i<n; i++) {
                        ordemesh->permtab[i] = i;
                        ordemesh->peritab[i] = i;
                    }
                }
                else {
                    if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
                        pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (from myorder->peritab)" );
                    /* generate permtab from myorder->peritab */
                    for(i=0;i<n;i++)
                        ordemesh->permtab[myorder->peritab[i]] = i;
                    memcpy(ordemesh->peritab, myorder->peritab, n*sizeof(pastix_int_t));
                }
            }
            else {
                if (myorder->peritab == NULL) {
                    if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
                        pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (from myorder->permtab)" );
                    /* generate peritab from myorder->permtab */
                    for(i=0;i<n;i++)
                        ordemesh->peritab[myorder->permtab[i]] = i;
                    memcpy(ordemesh->permtab, myorder->permtab, n*sizeof(pastix_int_t));
                }
                else {
                    if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
                        pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (myorder->permtab/peritab)" );
                    memcpy(ordemesh->permtab, myorder->permtab, n*sizeof(pastix_int_t));
                    memcpy(ordemesh->peritab, myorder->peritab, n*sizeof(pastix_int_t));
                }
            }

            /* Destroy the rangtab */
            ordemesh->cblknbr = 0;
            memFree_null( ordemesh->rangtab );
            /* Destroy the treetab */
            memFree_null( ordemesh->treetab );

            /* If treetab is provided, user must also provide rangtab */
            if ( myorder != NULL ) {
                assert( !( (myorder->rangtab == NULL) && (myorder->treetab != NULL) ) );
                if (myorder->rangtab != NULL )
                {
                    ordemesh->cblknbr = myorder->cblknbr;
                    MALLOC_INTERN(ordemesh->rangtab, myorder->cblknbr+1, pastix_int_t);
                    memcpy(ordemesh->rangtab, myorder->rangtab, (myorder->cblknbr+1)*sizeof(pastix_int_t));
                }
                if (myorder->treetab != NULL )
                {
                    MALLOC_INTERN(ordemesh->treetab, myorder->cblknbr, pastix_int_t);
                    memcpy(ordemesh->treetab, myorder->treetab, myorder->cblknbr*sizeof(pastix_int_t));
                }
            }
        }
        break;

        /*
         * Load ordering
         */
    case PastixOrderLoad:
        if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
            pastix_print(procnum, 0, OUT_ORDER_METHOD, "Load" );
        retval = orderLoad( ordemesh, NULL );
        break;

    default:
        errorPrint( "pastix_subtask_order: Ordering not available (iparm[IPARM_ORDERING]=%d)\n",
                    iparm[IPARM_ORDERING] );
        retval = PASTIX_ERR_BADPARAMETER;
        break;
    }

    if (retval != PASTIX_SUCCESS )
        return retval;

    /* Rebase the ordering to 0 (for orderFindSupernodes) */
    orderBase( ordemesh, 0 );

    /*
     * If the rangtab or the treetab are not initialized, let's find it ourself
     */
    if (( ordemesh->rangtab == NULL ) ||
        ( ordemesh->treetab == NULL ) )
    {
        /* TODO: if rangtab is provided, treetab could be easily calculated */
        graphBase( &subgraph, 0 );
        orderFindSupernodes( &subgraph, ordemesh );
    }

    orderApplyLevelOrder( ordemesh, iparm[IPARM_DISTRIBUTION_LEVEL] );

    /*
     * Add the isolated elements to the ordering structure
     */
    if ( do_zeros )
    {
        orderAddIsolate( ordemesh, schur_n, zeros_perm );

        if ( zeros_colptr != schur_colptr ) { memFree_null( zeros_colptr ); }
        if ( zeros_rows   != schur_rows   ) { memFree_null( zeros_rows   ); }
        if ( zeros_perm   != NULL         ) { memFree_null( zeros_perm   ); }
    }

    /*
     * Add the isolated elements to the ordering structure
     */
    if ( do_schur )
    {
        orderAddIsolate( ordemesh, n, schur_perm );

        if ( schur_colptr != graph->colptr ) { memFree_null( schur_colptr ); }
        if ( schur_rows   != graph->rows   ) { memFree_null( schur_rows   ); }
        if ( schur_perm   != NULL          ) { memFree_null( schur_perm   ); }
    }

    /* Reduce the error code */
    MPI_Allreduce(&retval, &retval_rcv, 1, MPI_INT, MPI_MAX,
                  pastix_data->pastix_comm);
    if (retval_rcv != PASTIX_SUCCESS)
        return retval_rcv;

    clockStop(timer);
    if (iparm[IPARM_VERBOSE] > PastixVerboseNot)
        pastix_print(procnum, 0, OUT_ORDER_TIME, clockVal(timer));

    /* Save i/o strategy */
    if ( iparm[IPARM_IO_STRATEGY] & PastixIOSave ) {
        if (procnum == 0) {
            retval = orderSave( ordemesh, NULL );
        }
        /* TODO: synchro of retval */
        if (retval != PASTIX_SUCCESS)
            return retval;
    }

    /*
     * Return the ordering to user if structure is not NULL
     * Remark: No need to copy back for personal
     */
    if (iparm[IPARM_ORDERING] != PastixOrderPersonal) {
        if (spm->loc2glob == NULL) {
            if (myorder != NULL)
            {
                retval = orderCopy( myorder, ordemesh );
                if (retval != PASTIX_SUCCESS )
                    return retval;
            }
        }
        else {
            int baseval = graph->colptr[0];
            /* Should be 0-based ? */
            assert (baseval == 0);

            if (myorder->permtab != NULL) {
                pastix_int_t *permtab = ordemesh->permtab - baseval;
                pastix_int_t i;

                for(i=0; i<n; i++) {
                    myorder->permtab[i] = permtab[spm->loc2glob[i]];
                }
            }
            if (myorder->peritab != NULL) {
                pastix_int_t *peritab = ordemesh->peritab - baseval;
                pastix_int_t i;

                for(i=0; i<n; i++) {
                    myorder->peritab[i] = peritab[spm->loc2glob[i]];
                }
            }
            /* TODO: Copy also rangtab and treetab ? */
        }
    }

#if !defined(NDEBUG)
    orderCheck( ordemesh );
#endif

    /* Backup the spm pointer for further information */
    pastix_data->csc = spm;

    /* Invalidate following steps, and add order step to the ones performed */
    pastix_data->steps &= ~( STEP_SYMBFACT |
                             STEP_ANALYSE  |
                             STEP_NUMFACT  |
                             STEP_SOLVE    |
                             STEP_REFINE   );
    pastix_data->steps |= STEP_ORDERING;

    return PASTIX_SUCCESS;
}
