/**
 *
 *  PaStiX is a software package provided by Inria Bordeaux - Sud-Ouest,
 *  LaBRI, University of Bordeaux 1 and IPB.
 *
 * @version 5.2.0
 * @author Mathieu Faverge
 * @author Pierre Ramet
 * @author Xavier Lacoste
 * @date 2011-11-11
 * @precisions normal z -> c d s
 *
 **/
#ifndef Z_PASTIX_DATA_H_
#define Z_PASTIX_DATA_H_

#include "sopalin_define.h"
#include "dof.h"
#include "z_ftgt.h"
#include "symbol.h"
#include "../csc/z_csc.h"
#include "z_updown.h"
#include "queue.h"
#include "bulles.h"
#include "z_solver.h"
/* #include "assembly.h" */
/* #include "param_blend.h" */
/* #include "order.h" */
/* #include "fax.h" */
/* #include "kass.h" */
/* #include "blend.h" */
/* #include "solverRealloc.h" */
/* #include "z_sopalin_thread.h" */
/* #include "stack.h" */
/* #include "z_sopalin3d.h" */
/* #include "z_sopalin_init.h" */
/* #include "z_sopalin_option.h" */
/* #include "z_csc_intern_updown.h" */
/* #include "z_csc_intern_build.h" */
/* #include "coefinit.h" */

/*
 * Steps of the z_pastix z_solver
 */
#define STEP_INIT     (1 << 0)
#define STEP_ORDERING (1 << 1)
#define STEP_SYMBFACT (1 << 2)
#define STEP_ANALYSE  (1 << 3)
#define STEP_NUMFACT  (1 << 4)
#define STEP_SOLVE    (1 << 5)
#define STEP_REFINE   (1 << 6)

/*
  struct: z_SopalinParam_

  Parameters for factorisation, updown and reffinement.
 */
typedef struct z_SopalinParam_ {
  z_CscMatrix      *cscmtx;          /*+ Compress Sparse Column matrix                    +*/
  double          epsilonraff;     /*+ epsilon to stop reffinement                      +*/
  double          rberror;         /*+ ||r||/||b||                                      +*/
  double          espilondiag;     /*+ epsilon critere for diag control                 +*/
  pastix_complex64_t *b;               /*+ b vector (RHS and solution)                      +*/
  pastix_complex64_t *transcsc;        /*+ transpose csc                                    +*/
  pastix_int_t    itermax;         /*+ max number of iteration                          +*/
  pastix_int_t    diagchange;      /*+ number of change of diag                         +*/
  pastix_int_t    gmresim;         /*+ Krylov subspace size for GMRES                   +*/
  pastix_int_t    fakefact;        /*+ Flag indicating if we want fake factorisation    +*/
  pastix_int_t    usenocsc;        /*+ Flag indicating if we want to use the intern CSC +*/
  int             factotype;       /*+ Type of factorization                            +*/
  int             symmetric;       /*+ Symmetric                                        +*/
  MPI_Comm        pastix_comm;     /*+ MPI communicator                                 +*/
  int             type_comm;       /*+ Communication mode                               +*/
  int             nbthrdcomm;      /*+ Communication's thread number                    +*/
  pastix_int_t   *iparm;           /*+ In/Out integer parameters                        +*/
  double         *dparm;           /*+ In/Out float parameters                          +*/
  int            *bindtab;         /*+ Define where to bin threads                      +*/
  int             stopthrd;        /*+ Boolean for communication thread controlling     +*/
  int             schur;           /*+ If API_YES won't compute last diag               +*/
  pastix_int_t    n;               /*+ size of the matrix                               +*/
  pastix_int_t    gN;
} z_SopalinParam;

/*
   struct: z_pastix_data_t

   Structure used to store datas for a step by step execution.
*/

struct z_pastix_data_s {
    pastix_int_t    *iparm;              /*< Store integer parameters (input/output)                             +*/
    double          *dparm;              /*< Store floating parameters (input/output)                            +*/

    pastix_int_t     steps;              /*< Bitmask of the steps performed or not                               +*/
    pastix_graph_t  *csc;
    pastix_int_t     gN;                 /*< Global number of columns without DoF                                +*/
    pastix_int_t     n2;                 /*< Local number of columns without DoF                                 +*/
    Order           *ordemesh;           /*< Ordering structure                                                  +*/
    SymbolMatrix    *symbmtx;            /*< Symbol Matrix                                                       +*/

    pastix_int_t     schur_n;            /*< Number of entries for the Schur complement                          +*/
    pastix_int_t    *schur_list;         /*< List of entries for the schur complement                            +*/
    pastix_int_t     zeros_n;            /*< Number of diagonal entries considered as zeros                      +*/
    pastix_int_t    *zeros_list;         /*< List of diagonal entries considered as zeros                        +*/

    z_SolverMatrix   solvmatr;           /*+ Matrix informations                                                 +*/
    z_CscMatrix        cscmtx;             /*+ Compress Sparse Column matrix                                       +*/
    z_SopalinParam     sopar;              /*+ Sopalin parameters                                                  +*/
#ifdef PASTIX_DISTRIBUTED
#ifdef WITH_SCOTCH
    pastix_int_t    *PTS_permtab;
    pastix_int_t    *PTS_peritab;
#endif /* WITH_SCOTCH */
    pastix_int_t    *glob2loc;           /*+ local column number of global column, or -(owner+1) is not local    +*/
    pastix_int_t     ncol_int;           /*+ Number of local columns in internal CSCD                            +*/
    pastix_int_t    *l2g_int;            /*+ Local to global column numbers in internal CSCD                     +*/
    int              malrhsd_int;        /*+ Indicates if internal distributed rhs has been allocated            +*/
    int              mal_l2g_int;
    pastix_complex64_t  *b_int;              /*+ Local part of the right-hand-side                                   +*/
#endif /* PASTIX_DISTRIBUTED */
    int              malcsc;             /*+ boolean indicating if solvmatr->cscmtx has beek allocated           +*/
    int              malsmx;             /*+ boolean indicating if solvmatr->updovct.sm2xtab has been allocated  +*/
    int              malslv;             /*+ boolean indicating if solvmatr has been allocated                   +*/
    int              malcof;             /*+ boolean indicating if coeficients tabular(s) has(ve) been allocated +*/
    MPI_Comm         pastix_comm;        /*+ PaStiX MPI communicator                                             +*/
    MPI_Comm         intra_node_comm;    /*+ PaStiX intra node MPI communicator                                  +*/
    MPI_Comm         inter_node_comm;    /*+ PaStiX inter node MPI communicator                                  +*/
    int              procnbr;            /*+ Number of MPI tasks                                                 +*/
    int              procnum;            /*+ Local MPI rank                                                      +*/
    int              intra_node_procnbr; /*+ Number of MPI tasks in node_comm                                    +*/
    int              intra_node_procnum; /*+ Local MPI rank in node_comm                                         +*/
    int              inter_node_procnbr; /*+ Number of MPI tasks in node_comm                                    +*/
    int              inter_node_procnum; /*+ Local MPI rank in node_comm                                         +*/
    int             *bindtab;            /*+ Tabular giving for each thread a CPU to bind it too                 +*/
    pastix_complex64_t  *schur_tab;
    pastix_int_t     schur_tab_set;
    int              cscInternFilled;
    int              scaling;            /*+ Indicates if the matrix has been scaled                             +*/
    pastix_complex64_t  *scalerowtab;        /*+ Describes how the matrix has been scaled                            +*/
    pastix_complex64_t  *iscalerowtab;
    pastix_complex64_t  *scalecoltab;
    pastix_complex64_t  *iscalecoltab;
#ifdef WITH_SEM_BARRIER
    sem_t           *sem_barrier;        /*+ Semaphore used for AUTOSPLIT_COMM barrier                           +*/
#endif
    pastix_int_t     pastix_id;          /*+ Id of the z_pastix instance (PID of first MPI task)                   +*/
};

#endif /* Z_PASTIX_DATA_H_ */