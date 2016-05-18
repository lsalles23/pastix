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


void order_grid2D_wide(pastix_int_t *rangtab,
                       pastix_int_t *peritab,
                       pastix_int_t *cblknbr,
                       pastix_int_t x0,
                       pastix_int_t xn,
                       pastix_int_t y0,
                       pastix_int_t yn,
                       pastix_int_t max_number,
                       pastix_int_t lda,
                       pastix_int_t *current_rangtab){

    pastix_int_t nx = xn-x0;
    pastix_int_t ny = yn-y0;
    /* printf("Treating a subgraph of size %ld %ld\n", nx, ny); */

    /* The subgraph is small enough */
    if (nx <= 4 && ny <= 4){
        cblknbr[0] ++;
        /* printf("Treating cblk %ld\n", cblknbr[0]); */
        pastix_int_t i, j;
        pastix_int_t current = 0;
        for (i=x0; i<xn; i++){
            for (j=y0; j<yn; j++){
                pastix_int_t index = i + lda * j;
                peritab[index] = max_number - current;
                /* printf("Fitting 1a %ld with %ld\n", index, max_number - current); */
                current++;
            }
        }

        rangtab[current_rangtab[0]] = max_number;
        current_rangtab[0]++;
        return;
    }

    cblknbr[0] ++;
    /* printf("Treating cblk %ld\n", cblknbr[0]); */

    /* In which direction do we cut? 0 for x, 1 for y */
    pastix_int_t dir = 0;
    if (ny > nx)
        dir = 1;

    /* If we cut in direction x */
    if (dir == 0){

        rangtab[current_rangtab[0]] = max_number;
        current_rangtab[0]++;

        pastix_int_t i;
        for (i=0; i<ny; i++){
            pastix_int_t index = x0 + nx/2 + lda*(y0+i) - 1;
            peritab[index] = max_number - i;
            /* printf("Fitting 1a %ld with %ld\n", index, max_number - i); */
        }
        for (i=0; i<ny; i++){
            pastix_int_t index = x0 + nx/2 + lda*(y0+i);
            peritab[index] = max_number - ny - i;
            /* printf("Fitting 1b %ld with %ld\n", index, max_number - ny - i); */
        }

        order_grid2D_wide(rangtab, peritab, cblknbr,
                          x0, xn-nx/2-1, y0, yn, max_number - 2*ny,
                          lda, current_rangtab);

        order_grid2D_wide(rangtab, peritab, cblknbr,
                          x0+nx/2+1, xn, y0, yn, max_number - 2*ny - ny*(nx/2-1),
                     lda, current_rangtab);

    }

    /* If we cut in direction y */
    else{
        rangtab[current_rangtab[0]] = max_number;
        current_rangtab[0]++;

        pastix_int_t i;
        for (i=0; i<nx; i++){
            pastix_int_t index = lda*(y0+ny/2-1) + x0 + i;
            peritab[index] = max_number - i;
            /* printf("Fitting 2a %ld with %ld\n", index, max_number - i); */
        }
        for (i=0; i<nx; i++){
            pastix_int_t index = lda*(y0+ny/2) + x0 + i;
            peritab[index] = max_number - nx - i;
            /* printf("Fitting 2b %ld with %ld\n", index, max_number - nx - i); */
        }

        /* printf("Parameters %ld %ld %ld %ld\n", x0, xn, y0, yn/2-1); */
        order_grid2D_wide(rangtab, peritab, cblknbr,
                          x0, xn, y0, yn-ny/2-1, max_number - 2*nx,
                          lda, current_rangtab);

        order_grid2D_wide(rangtab, peritab, cblknbr,
                          x0, xn, y0+ny/2+1, yn, max_number - 2*nx - nx*(ny/2-1),
                          lda, current_rangtab);
    }
}

void order_grid3D_wide(pastix_int_t *rangtab,
                       pastix_int_t *peritab,
                       pastix_int_t *cblknbr,
                       pastix_int_t x0,
                       pastix_int_t xn,
                       pastix_int_t y0,
                       pastix_int_t yn,
                       pastix_int_t z0,
                       pastix_int_t zn,
                       pastix_int_t max_number,
                       pastix_int_t lda,
                       pastix_int_t *current_rangtab,
                       pastix_int_t *treetab,
                       pastix_int_t current_treetab){

    pastix_int_t nx = xn-x0;
    pastix_int_t ny = yn-y0;
    pastix_int_t nz = zn-z0;
    /* printf("Treating a subgraph of size %ld %ld %ld\n", nx, ny, nz); */

    /* The subgraph is small enough */
    if (nx <= 4 && ny <= 4 && nz <= 4){
        cblknbr[0] ++;
        /* printf("Treating cblk %ld\n", cblknbr[0]); */
        pastix_int_t i, j, k;
        pastix_int_t current = 0;
        for (i=x0; i<xn; i++){
            for (j=y0; j<yn; j++){
                for (k=z0; k<zn; k++){
                    pastix_int_t index = i + lda * j + lda*lda * k;
                    peritab[index] = max_number - current;
                    current++;
                }
            }
        }

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number;
        current_rangtab[0]++;
        return;
    }

    cblknbr[0] ++;
    /* printf("Treating cblk %ld\n", cblknbr[0]); */

    /* In which direction do we cut? 0 for x, 1 for y */
    pastix_int_t dir = 0;
    if (ny > nx)
        dir = 1;
    if (nz > nx && nz > ny)
        dir = 2;

    /* If we cut in direction x */
    if (dir == 0){

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number;
        current_rangtab[0]++;

        pastix_int_t i, j;
        pastix_int_t current = 0;
        for (i=0; i<ny; i++){
            for (j=0; j<nz; j++){
                pastix_int_t index = x0 + nx/2 + lda*(y0+i) - 1 + lda*lda*(z0+j);
                peritab[index] = max_number - current;
                current++;
            }
        }
        for (i=0; i<ny; i++){
            for (j=0; j<nz; j++){
                pastix_int_t index = x0 + nx/2 + lda*(y0+i) + lda*lda*(z0+j);
                peritab[index] = max_number - current;
                current++;
            }
        }

        order_grid3D_wide(rangtab, peritab, cblknbr,
                          x0, xn-nx/2-1, y0, yn, z0, zn, max_number - 2*ny*nz,
                          lda, current_rangtab,
                          treetab, current_treetab+1);

        order_grid3D_wide(rangtab, peritab, cblknbr,
                          x0+nx/2+1, xn, y0, yn, z0, zn, max_number - nx*ny*nz /2 - ny*nz,
                          lda, current_rangtab,
                          treetab, current_treetab+1);

    }

    /* If we cut in direction y */
    else if (dir == 1){

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number;
        current_rangtab[0]++;

        pastix_int_t i, j;
        pastix_int_t current = 0;
        for (i=0; i<nx; i++){
            for (j=0; j<nz; j++){
                pastix_int_t index = lda*(y0+ny/2-1) + x0 + i  + lda*lda*(z0+j);
                peritab[index] = max_number - current;
                current++;
            }
        }
        for (i=0; i<nx; i++){
            for (j=0; j<nz; j++){
                pastix_int_t index = lda*(y0+ny/2) + x0 + i + lda*lda*(z0+j);
                peritab[index] = max_number - current;
                current++;
            }
        }

        order_grid3D_wide(rangtab, peritab, cblknbr,
                          x0, xn, y0, yn-ny/2-1, z0, zn, max_number - 2*nx*nz,
                          lda, current_rangtab,
                          treetab, current_treetab+1);

        order_grid3D_wide(rangtab, peritab, cblknbr,
                          x0, xn, y0+ny/2+1, yn, z0, zn, max_number - nx*ny*nz /2 - nx*nz,
                          lda, current_rangtab,
                          treetab, current_treetab+1);
    }

    /* If we cut in direction y */
    else{

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number;
        current_rangtab[0]++;

        pastix_int_t i, j;
        pastix_int_t current = 0;
        for (i=0; i<nx; i++){
            for (j=0; j<ny; j++){
                pastix_int_t index = lda*lda*(z0+nz/2-1) + x0 + i + lda * (y0+j);
                peritab[index] = max_number - current;
                current++;
            }
        }
        for (i=0; i<nx; i++){
            for (j=0; j<ny; j++){
                pastix_int_t index = lda*lda*(z0+nz/2) + x0 + i + lda * (y0+j);
                peritab[index] = max_number - current;
                current++;
            }
        }

        order_grid3D_wide(rangtab, peritab, cblknbr,
                          x0, xn, y0, yn, z0, zn-nz/2-1, max_number - 2*nx*ny,
                          lda, current_rangtab,
                          treetab, current_treetab+1);

        order_grid3D_wide(rangtab, peritab, cblknbr,
                          x0, xn, y0, yn, z0+nz/2+1, zn, max_number - nx*ny*nz /2 - nx*ny,
                          lda, current_rangtab,
                          treetab, current_treetab+1);
    }
}

void order_grid3D_classic(pastix_int_t *rangtab,
                          pastix_int_t *peritab,
                          pastix_int_t *cblknbr,
                          pastix_int_t x0,
                          pastix_int_t xn,
                          pastix_int_t y0,
                          pastix_int_t yn,
                          pastix_int_t z0,
                          pastix_int_t zn,
                          pastix_int_t *max_number,
                          pastix_int_t lda,
                          pastix_int_t *current_rangtab,
                          pastix_int_t *treetab,
                          pastix_int_t current_treetab){

    pastix_int_t nx = xn-x0;
    pastix_int_t ny = yn-y0;
    pastix_int_t nz = zn-z0;

    /* printf("Treating a subgraph of size %ld %ld %ld\n", nx, ny, nz); */

    /* The subgraph is small enough */
    /* if (nx <= 4 && ny <= 4 && nz <= 4){ */
    if (nx*ny*nz < 10){
        cblknbr[0] ++;
        printf("Treating small clique %ld\n", nx*ny*nz);
        pastix_int_t i, j, k;
        pastix_int_t current = 0;
        for (i=x0; i<xn; i++){
            for (j=y0; j<yn; j++){
                for (k=z0; k<zn; k++){
                    pastix_int_t index = i + lda * j + lda*lda * k;
                    peritab[index] = max_number[0] - current;
                    current++;
                }
            }
        }

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number[0];
        max_number[0] -= current;
        current_rangtab[0]++;
        return;
    }

    cblknbr[0] ++;

    /* In which direction do we cut? 0 for x, 1 for y */
    pastix_int_t dir = 0;
    if (ny > nx)
        dir = 1;
    if (nz > nx && nz > ny)
        dir = 2;

    /* If we cut in direction x */
    if (dir == 0){

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number[0];
        current_rangtab[0]++;

        pastix_int_t i, j;
        pastix_int_t current = 0;
        for (i=0; i<ny; i++){
            for (j=0; j<nz; j++){
                pastix_int_t index = x0 + nx/2 + lda*(y0+i) + lda*lda*(z0+j);
                peritab[index] = max_number[0] - current;
                current++;
            }
        }
        max_number[0] -= current;

        order_grid3D_classic(rangtab, peritab, cblknbr,
                             x0, x0 + nx/2, y0, yn, z0, zn, max_number,
                             lda, current_rangtab,
                             treetab, current_treetab+1);

        order_grid3D_classic(rangtab, peritab, cblknbr,
                             x0+nx/2+1, xn, y0, yn, z0, zn, max_number,
                             lda, current_rangtab,
                             treetab, current_treetab+1);

    }

    /* If we cut in direction y */
    else if (dir == 1){

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number[0];
        current_rangtab[0]++;

        pastix_int_t i, j;
        pastix_int_t current = 0;
        for (i=0; i<nx; i++){
            for (j=0; j<nz; j++){
                pastix_int_t index = lda*(y0+ny/2) + x0 + i  + lda*lda*(z0+j);
                peritab[index] = max_number[0] - current;
                current++;
            }
        }
        max_number[0] -= current;

        order_grid3D_classic(rangtab, peritab, cblknbr,
                             x0, xn, y0, y0+ny/2, z0, zn, max_number,
                             lda, current_rangtab,
                             treetab, current_treetab+1);

        order_grid3D_classic(rangtab, peritab, cblknbr,
                             x0, xn, y0+ny/2+1, yn, z0, zn, max_number,
                             lda, current_rangtab,
                             treetab, current_treetab+1);
    }

    /* If we cut in direction z */
    else{

        treetab[current_rangtab[0]] = current_treetab;
        rangtab[current_rangtab[0]] = max_number[0];
        current_rangtab[0]++;

        pastix_int_t i, j;
        pastix_int_t current = 0;
        for (i=0; i<nx; i++){
            for (j=0; j<ny; j++){
                pastix_int_t index = lda*lda*(z0+nz/2) + x0 + i + lda * (y0+j);
                peritab[index] = max_number[0] - current;
                current++;
            }
        }
        max_number[0] -= current;

        order_grid3D_classic(rangtab, peritab, cblknbr,
                             x0, xn, y0, yn, z0, z0+nz/2, max_number,
                             lda, current_rangtab,
                             treetab, current_treetab+1);

        order_grid3D_classic(rangtab, peritab, cblknbr,
                             x0, xn, y0, yn, z0+nz/2+1, zn, max_number,
                             lda, current_rangtab,
                             treetab, current_treetab+1);
    }
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_ordering
 * @ingroup pastix
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
 * @param[in] n
 *          The number of vertices.
 *
 * @param[in] colptr
 *          Array of size n+1
 *          The array of indirection to the rows array for each vertex.
 *          rows[ colptr[i] ] to rows[ colptr[i+1] are the edges of the
 *          ith vertex.
 *
 * @param[in] rows
 *          Array of size nnz = colptr[n] - colptr[0]. The array of edges.
 *          rows[ colptr[i]   - colptr[0] ] to
 *          rows[ colptr[i+1] - colptr[0] ] are the edges of the ith vertex.
 *
 * @param[in] loc2glob
 *          Array of size n
 *          Global numbering of each local vertex.
 *          NULL if centralized interface is used or if graph load is asked.
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
                  const pastix_csc_t  *csc,
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
    if (csc == NULL) {
        errorPrint("pastix_task_order: wrong csc parameter");
        return PASTIX_ERR_BADPARAMETER;
    }
    iparm = pastix_data->iparm;
    n = csc->n;

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
     * Prepare a copy of user's CSC
     * Copy the given csc in pastix_data structure and performs basic required
     * operations as symmetrizing the graph and removing the diagonal
     * coefficients
     */
    graphPrepare( pastix_data, csc, &(pastix_data->graph) );
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
            n = csc->gN;
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

    /* orderApplyLevelOrder( ordemesh ); */

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
        if (csc->loc2glob == NULL) {
            if (perm != NULL) memcpy(perm, ordemesh->permtab, n*sizeof(pastix_int_t));
            if (invp != NULL) memcpy(invp, ordemesh->peritab, n*sizeof(pastix_int_t));
        }
        else {
            int baseval = graph->colptr[0];

            if (perm != NULL) {
                pastix_int_t *permtab = ordemesh->permtab - baseval;
                pastix_int_t i;

                for(i=0; i<n; i++) {
                    perm[i] = permtab[csc->loc2glob[i]];
                }
            }
            if (invp != NULL) {
                pastix_int_t *peritab = ordemesh->peritab - baseval;
                pastix_int_t i;

                for(i=0; i<n; i++) {
                    invp[i] = peritab[csc->loc2glob[i]];
                }
            }
        }
    }

#if !defined(NDEBUG)
    orderCheck( ordemesh );
#endif

    /* Backup the csc pointer for further information */
    pastix_data->csc = csc;

    /* Invalidate following steps, and add order step to the ones performed */
    pastix_data->steps &= ~( STEP_SYMBFACT |
                             STEP_ANALYSE  |
                             STEP_NUMFACT  |
                             STEP_SOLVE    |
                             STEP_REFINE   );
    pastix_data->steps |= STEP_ORDERING;

    iparm[IPARM_START_TASK]++;


    /* Useful to perform reordering after splitting */
    /* if (rangtab_current != 0){ */
    /*     ordemesh->cblknbr = rangtab_current; */

    /*     free(ordemesh->rangtab); */
    /*     ordemesh->rangtab = malloc((rangtab_current+1)* sizeof(pastix_int_t)); */

    /*     pastix_int_t i; */
    /*     for (i=0; i<=rangtab_current; i++){ */
    /*         ordemesh->rangtab[i] = rangtab_new[i]; */
    /*     } */

    /*     memcpy(ordemesh->permtab, permtab_saved, n*sizeof(pastix_int_t)); */
    /*     memcpy(ordemesh->peritab, peritab_saved, n*sizeof(pastix_int_t)); */

    /* } */

    /* Manual ordering for regular grids */
    if (0){
        pastix_int_t sep = 47; //sqrt(n);
        pastix_int_t i = 2;

        /* Graphs for using wide separators */
        /* while (i != sep && i < sep+1){ */
        /*     printf("2*i+2 %ld\n", i); */
        /*     i = 2*i+2; */
        /* } */
        /* if (i != sep){ */
        /*     printf("The given graph size is not correct for manual ordering (2D/3D graph of size 2^n*4-2)\n"); */
        /*     exit(1); */
        /* } */

        /* Graphs for using classical separators */
        while (i != sep && i < sep+1){
            printf("2*i+1 %ld\n", i);
            i = 2*i+1;
        }
        if (i != sep){
            printf("The given graph size is not correct for manual ordering (2D/3D graph of size 2^n*4-2)\n");
            exit(1);
        }

        ordemesh->rangtab = malloc((n+1)* sizeof(pastix_int_t));
        ordemesh->treetab = malloc((n+1) * sizeof(pastix_int_t));
        ordemesh->cblknbr = 0;
        pastix_int_t current_rangtab = 0;

        if (sep * sep == n){
            order_grid2D_wide(ordemesh->rangtab, ordemesh->permtab, &ordemesh->cblknbr,
                              0, sep, 0, sep, n-1, sep, &current_rangtab);
        }
        else{
            /* order_grid3D_wide(ordemesh->rangtab, ordemesh->permtab, &ordemesh->cblknbr, */
            /*                   0, sep, 0, sep, 0, sep, n-1, sep, &current_rangtab, */
            /*                   ordemesh->treetab, 1); */
            pastix_int_t current_number = n-1;

            order_grid3D_classic(ordemesh->rangtab, ordemesh->permtab, &ordemesh->cblknbr,
                                 0, sep, 0, sep, 0, sep, &current_number, sep, &current_rangtab,
                                 ordemesh->treetab, 1);
        }

        for (i=0; i<n; i++){
            ordemesh->peritab[ordemesh->permtab[i]] = i;
        }

        pastix_int_t *saved_rangtab = malloc(n*sizeof(pastix_int_t));
        memcpy(saved_rangtab, ordemesh->rangtab, n*sizeof(pastix_int_t));
        pastix_int_t *saved_treetab = malloc(n*sizeof(pastix_int_t));
        memcpy(saved_treetab, ordemesh->treetab, n*sizeof(pastix_int_t));

        ordemesh->rangtab[0] = 0;
        for (i=0; i<ordemesh->cblknbr; i++){
            ordemesh->rangtab[i+1] = saved_rangtab[ordemesh->cblknbr - i - 1]+1;
            ordemesh->treetab[i]   = ordemesh->cblknbr-1; //saved_treetab[ordemesh->cblknbr - i - 1];
            /* printf("Rangtab %ld is %ld Treetab is %ld\n", */
            /*        i, ordemesh->rangtab[i], */
            /*        ordemesh->treetab[i]); */
        }
        ordemesh->treetab[ordemesh->cblknbr-1] = -1;

        printf("WE GOT %ld CBLK\n", ordemesh->cblknbr);
        printf("MANUAL ORDERING IS COMPUTED\n\n");
    }

    return PASTIX_SUCCESS;
}
