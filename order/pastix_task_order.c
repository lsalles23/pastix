/**
 *
 * @file pastix_task_order.c
 *
 *  PaStiX order routines
 *  PaStiX is a software package provided by Inria Bordeaux - Sud-Ouest,
 *  LaBRI, University of Bordeaux 1 and IPB.
 *
 * Contains the main routine to compute ordering of a given graph.
 *
 * @version 5.1.0
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
 * @ingroup pastix_ordering
 *
 * pastix_task_order - Computes the ordering of the given graph in parameters.
 *
 * The graph given by the user is used to generate a graph that can be used by
 * ordering tools and symbol factorization. This graph is stored in the
 * pastix_data->graph to be pass over to the symbolic factorization. If it exists
 * before to call this routine, then the current structure is cleaned and a new
 * one is created. From this structure, an ordering is computed by the ordering
 * tool chosen by IPARM_ORDERING and stored in pastix_data->ordemesh. At the end
 * the full ordering stucture: pemutation, inverse permutation, partition, and
 * partion tree is generated such that it can be used by any symbolic
 * factorization algorithm. It is however recommended to use Kass to perform
 * assembly if (PT-)Scotch was not used as the ordering algorithm.
 * The user can get back the permutation generated by providing allocated perm
 * and/or invp arrays where the results is stored after computation.
 *
 * This routine is affected by the following parameters:
 *   IPARM_VERBOSE, IPARM_ORDERING, IPARM_SCHUR, IPARM_ISOLATE_ZEROS,
 *   IPARM_IO_STRATEGY
 *
 *******************************************************************************
 *
 * @param[in,out] pastix_data
 *          The pastix_data structure that describes the solver instance.
 *          On exit, the field ordemesh is initialize with the result of the
 *          ordering.
 *          - IPARM_ORDERING will determine which ordering tool is used.
 *          - IPARM_SCHUR enables the extraction of the schur complement from the
 *          user's graph to isolate it at the end of the matrix. The list of
 *          vertices to isolated must be provided through the
 *          pastix_setSchurUnknownList() function.
 *          - IPARM_ISOLATE_ZEROS enables the extraction of the diagonal elements
 *          from the user's graph to isolate it at the end of the matrix. The
 *          list of vertices to isolated must be provided through the
 *          pastix_setZerosUnknownList() function.
 *          - IPARM_IO_STRATEGY will enable the results to be written on files
 *          if set to API_IO_SAVE, or the results to be directly loaded from
 *          file if set to APÏ_IO_LOAD without going through an ordering
 *          library.
 *
 * @param[in] spm
 *          The sparse matrix given by the user on which the ordering will be
 *          computed.
 *
 * @param[in,out] perm
 *          Array of size n.
 *          On entry, the permutation array if IPARM_ORDERING parameter set to
 *          API_ORDER_PERSONAL. Not read otherwise.
 *          On exit, if perm != NULL and IPARM_ORDERING parameter is not set to
 *          API_ORDER_PERSONAL, contains the permutation array
 *          generated. Otherwise, it is not referenced.
 *
 * @param[in,out] invp
 *          Array of size n.
 *          On entry, the inverse permutation array if IPARM_ORDERING parameter
 *          set to API_ORDER_PERSONAL. Not read otherwise.
 *          On exit, if invp != NULL and IPARM_ORDERING parameter is not set to
 *          API_ORDER_PERSONAL, contains the inverse permutation array
 *          generated. Otherwise, it is not referenced.
 *
 *******************************************************************************
 *
 * @return
 *          \retval PASTIX_SUCCESS on successful exit
 *          \retval PASTIX_ERR_BADPARAMETER if one parameter is incorrect.
 *          \retval PASTIX_ERR_OUTOFMEMORY if one allocation failed.
 *          \retval PASTIX_ERR_INTEGER_TYPE if Scotch integer type is not the
 *                  same size as PaStiX ones.
 *          \retval PASTIX_ERR_INTERNAL if an error occurs internally to Scotch.
 *
 *******************************************************************************/
int
pastix_task_order(      pastix_data_t *pastix_data,
                  const pastix_spm_t  *spm,
                        pastix_int_t  *perm,
                        pastix_int_t  *invp)
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
        errorPrint("pastix_task_order: wrong pastix_data parameter");
        return PASTIX_ERR_BADPARAMETER;
    }
    if (spm == NULL) {
        errorPrint("pastix_task_order: wrong spm parameter");
        return PASTIX_ERR_BADPARAMETER;
    }
    iparm = pastix_data->iparm;
    n = spm->n;

    if ( !(pastix_data->steps & STEP_INIT) ) {
        errorPrint("pastix_task_order: pastix_task_init() has to be called before calling this function");
        return PASTIX_ERR_BADPARAMETER;
    }

    if ((iparm[IPARM_SCHUR] == API_YES) &&
        (pastix_data->schur_n > 0))
    {
        /*
         * If ordering is set to API_ORDER_PERSONAL or API_ORDER_LOAD, we
         * consider that the schur complement is already isolated at the end of
         * permutation array
         */
        if ((iparm[IPARM_ORDERING] == API_ORDER_PERSONAL) ||
            (iparm[IPARM_ORDERING] == API_ORDER_LOAD)) {
            do_schur = 0;
        }
    } else {
        do_schur = 0;
    }
    if ((iparm[IPARM_ISOLATE_ZEROS] == API_YES) &&
        (pastix_data->zeros_n > 0) )
    {
        /*
         * If ordering is set to API_ORDER_PERSONAL or API_ORDER_LOAD, we
         * consider that the zeros on diagonal are already isolated at the end of
         * permutation array
         */
        if ((iparm[IPARM_ORDERING] == API_ORDER_PERSONAL) ||
            (iparm[IPARM_ORDERING] == API_ORDER_LOAD) ) {
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
    orderInit( ordemesh, 0, 0 );

    if (iparm[IPARM_VERBOSE] > API_VERBOSE_NO)
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

    if (iparm[IPARM_VERBOSE] > API_VERBOSE_YES)
        pastix_print(procnum, 0, "%s", OUT_ORDERINIT);

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
    case API_ORDER_SCOTCH:
        pastix_print(procnum, 0, OUT_ORDER_METHOD, "Scotch" );
#if defined(HAVE_SCOTCH)
        retval = orderComputeScotch( pastix_data, &subgraph );
#else
        errorPrint("pastix_task_order: Ordering with Scotch requires to enable -DPASTIX_ORDERING_SCOTCH option");
        retval = PASTIX_ERR_BADPARAMETER;
#endif
        break;

        /*
         * PT-Scotch Ordering
         */
    case API_ORDER_PTSCOTCH:
        pastix_print(procnum, 0, OUT_ORDER_METHOD, "PT-Scotch" );
#if defined(HAVE_PTSCOTCH)
        retval = orderComputePTScotch( pastix_data, &subgraph );
#else
        errorPrint("pastix_task_order: Ordering with PT-Scotch requires to enable -DPASTIX_ORDERING_PTSCOTCH option");
        retval = PASTIX_ERR_BADPARAMETER;
#endif
        break;

        /*
         *  METIS ordering
         */
    case API_ORDER_METIS:
        pastix_print(procnum, 0, OUT_ORDER_METHOD, "Metis" );
#if defined(HAVE_METIS)
        retval = orderComputeMetis( pastix_data, &subgraph );
        assert( ordemesh->rangtab == NULL );
#else
        errorPrint("pastix_task_order: Ordering with Metis requires -DHAVE_METIS flag at compile time");
        retval = PASTIX_ERR_BADPARAMETER;
#endif
        break;

        /*
         * Personal Ordering
         */
    case API_ORDER_PERSONAL:
        {
            pastix_int_t i, n;
            n = spm->gN;
            orderInit(ordemesh, n, 0);
            if (perm == NULL) {
                if (invp == NULL) {
                    pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (identity)" );
                    for(i=0; i<n; i++) {
                        ordemesh->permtab[i] = i;
                        ordemesh->peritab[i] = i;
                    }
                }
                else {
                    pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (from invp)" );
                    // TODO: generate perm from invp
                    assert(0);
                    memcpy(ordemesh->peritab, invp, n*sizeof(pastix_int_t));
                }
            }
            else {
                if (invp == NULL) {
                    pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (from perm)" );
                    // TODO: generate invp from perm
                    assert(0);
                    memcpy(ordemesh->permtab, perm, n*sizeof(pastix_int_t));
                }
                else {
                    pastix_print(procnum, 0, OUT_ORDER_METHOD, "Personal (perm/invp)" );
                    memcpy(ordemesh->permtab, perm, n*sizeof(pastix_int_t));
                    memcpy(ordemesh->peritab, invp, n*sizeof(pastix_int_t));
                }
            }
            /* Destroy the rangtab */
            memFree_null( ordemesh->rangtab );
            ordemesh->cblknbr = 0;
        }
        break;

        /*
         * Load ordering
         */
    case API_ORDER_LOAD:
        pastix_print(procnum, 0, OUT_ORDER_METHOD, "Load" );
        retval = orderLoad( ordemesh, NULL );
        break;

    default:
        errorPrint( "pastix_task_order: Ordering not available (iparm[IPARM_ORDERING]=%d)\n",
                    iparm[IPARM_ORDERING] );
        retval = PASTIX_ERR_BADPARAMETER;
        break;
    }

    if (retval != PASTIX_SUCCESS )
        return retval;

    /* Rebase the ordering to 0 (for orderFindSupernodes) */
    orderBase(ordemesh, 0);

    /*
     * If the rangtab or the treetab are not initialized, let's find it ourself
     */
    if (( ordemesh->rangtab == NULL ) ||
        ( ordemesh->treetab == NULL ) )
    {
        graphBase( &subgraph, 0 );
        orderFindSupernodes( &subgraph, ordemesh );
    }

    orderApplyLevelOrder( ordemesh );

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
    if (iparm[IPARM_VERBOSE] > API_VERBOSE_NOT)
        pastix_print(procnum, 0, TIME_COMPUTE_ORDERING, clockVal(timer));

    /* Save i/o strategy */
    if (PASTIX_MASK_ISTRUE(iparm[IPARM_IO_STRATEGY], API_IO_SAVE)) {
        if (procnum == 0) {
            retval = orderSave( ordemesh, NULL );
        }
        /* TODO: synchro of retval */
        if (retval != PASTIX_SUCCESS)
            return retval;
    }

    /*
     * Return the ordering to user if perm/invp are not NULL
     * Remark: No need to copy back for personal
     */
    if (iparm[IPARM_ORDERING] != API_ORDER_PERSONAL) {
        if (spm->loc2glob == NULL) {
            if (perm != NULL) memcpy(perm, ordemesh->permtab, n*sizeof(pastix_int_t));
            if (invp != NULL) memcpy(invp, ordemesh->peritab, n*sizeof(pastix_int_t));
        }
        else {
            int baseval = graph->colptr[0];

            if (perm != NULL) {
                pastix_int_t *permtab = ordemesh->permtab - baseval;
                pastix_int_t i;

                for(i=0; i<n; i++) {
                    perm[i] = permtab[spm->loc2glob[i]];
                }
            }
            if (invp != NULL) {
                pastix_int_t *peritab = ordemesh->peritab - baseval;
                pastix_int_t i;

                for(i=0; i<n; i++) {
                    invp[i] = peritab[spm->loc2glob[i]];
                }
            }
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

    iparm[IPARM_START_TASK]++;
    return PASTIX_SUCCESS;
}
