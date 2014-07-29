/* File: multi-comm-step.c

   A simple example :
   creates two communicators,
   read the matrix, and run one instance of PaStiX on each communicator.

*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifndef FORCE_NOMPI
#include <mpi.h>
#endif
#include <complex.h>
/* to access functions from the libpastix, respect this order */
#include "pastix.h"
#include "read_matrix.h"
#include "get_options.h"

int main (int argc, char **argv)
{
#ifdef FORCE_NOMPI
  fprintf(stdout, "This example is not compatible with -DFORCE_MPI\n");
  return EXIT_FAILURE;
#else
  z_pastix_data_t    *pastix_data = NULL; /* Pointer to a storage structure needed by pastix           */
  pastix_int_t      ncol;               /* Size of the matrix                                        */
  pastix_int_t     *colptr      = NULL; /* Indexes of first element of each column in row and values */
  pastix_int_t     *rows        = NULL; /* Row of each element of the matrix                         */
  pastix_complex64_t   *values      = NULL; /* Value of each element of the matrix                       */
  pastix_complex64_t   *rhs         = NULL; /* right hand side                                           */
  pastix_complex64_t ***rhsiter     = NULL; /* right hand side (one by iteration)                        */
  pastix_int_t      iparm[IPARM_SIZE];  /* integer parameters for pastix                             */
  double            dparm[DPARM_SIZE];  /* floating parameters for pastix                            */
  pastix_int_t     *perm        = NULL; /* Permutation tabular                                       */
  pastix_int_t     *invp        = NULL; /* Reverse permutation tabular                               */
  char             *type        = NULL; /* type of the matrix                                        */
  char             *rhstype     = NULL; /* type of the right hand side                               */
  int               required;           /* MPI thread level required                                 */
  int               provided;           /* MPI thread level provided                                 */
  driver_type_t    *driver_type;        /* Matrix driver(s) requested by user                        */
  char            **filename;           /* Filename(s) given by user                                 */
  int               nbmatrices;         /* Number of matrices given by user                          */
  int               nbthread;           /* Number of thread wanted by user                           */
  int               verbosemode;        /* Level of verbose mode (0, 1, 2)                           */
  int               ordering;           /* Ordering to use                                           */
  int               incomplete;         /* Indicate if we want to use incomplete factorisation       */
  int               level_of_fill;      /* Level of fill for incomplete factorisation                */
  int               amalgamation;       /* Level of amalgamation for Kass                             */
  int               mpid;               /* Global MPI rank                                           */
  MPI_Comm          pastix_comm;        /* Communicator for pastix calls                             */
  int               ooc;                /* OOC limit (Mo/percent depending on compilation options)   */
  pastix_int_t      mat_type;
  long              i,j;
  int               nbcomm = 2;
  int               nfact  = 2;
  int               nsolv  = 2;

  /*******************************************/
  /*          MPI initialisation             */
  /*******************************************/
  required = MPI_THREAD_MULTIPLE;
  provided = -1;
  MPI_Init_thread(&argc, &argv, required, &provided);
  switch (provided)
    {
    case MPI_THREAD_SINGLE:
      printf("MPI_Init_thread level = MPI_THREAD_SINGLE\n");
      break;
    case MPI_THREAD_FUNNELED:
      printf("MPI_Init_thread level = MPI_THREAD_FUNNELED\n");
      break;
    case MPI_THREAD_SERIALIZED:
      printf("MPI_Init_thread level = MPI_THREAD_SERIALIZED\n");
      break;
    case MPI_THREAD_MULTIPLE:
      printf("MPI_Init_thread level = MPI_THREAD_MULTIPLE\n");
      break;
    default:
      printf("MPI_Init_thread level = ???\n");
    }

  /*******************************************/
  /*      Creation of two communicators      */
  /*******************************************/
  MPI_Comm_rank (MPI_COMM_WORLD, &mpid);
  MPI_Comm_split(MPI_COMM_WORLD, mpid%nbcomm, mpid, &pastix_comm);

  /*******************************************/
  /*    Get options from command line        */
  /*******************************************/
  if (EXIT_FAILURE == get_options(argc, argv,     &driver_type,
                                  &filename,      &nbmatrices,
                                  &nbthread,      &verbosemode,
                                  &ordering,      &incomplete,
                                  &level_of_fill, &amalgamation,
                                  &ooc,           &ncol))
    return EXIT_FAILURE;

  /*******************************************/
  /*      Read nbcomm Matrices               */
  /*******************************************/
  if (nbmatrices != nbcomm)
    {
      fprintf(stderr,"WARNING: should have %d matrix\n",nbcomm);
      while (nbmatrices < nbcomm)
        {
          fprintf(stderr,"\tSetting matrice %d to the same as matrix 1\n",
                  nbmatrices+1);
          driver_type[nbmatrices] = driver_type[0];
          if (filename[0] != NULL)
            {
              filename[nbmatrices] = (char *) malloc((strlen(filename[0])+1) *
                                                     sizeof(char));
              strcpy(filename[nbmatrices], filename[0]);
            }
          else
            {
              filename[nbmatrices]    = NULL;
            }
          nbmatrices++;
        }
    }

  read_matrix(filename[mpid%2], &ncol, &colptr, &rows, &values,
              &rhs, &type, &rhstype, driver_type[mpid%2], pastix_comm);

  for (i = 0; i < nbmatrices; i++)
    if (filename[i] != NULL)
      free(filename[i]);
  free(filename);
  free(driver_type);

  mat_type = API_SYM_NO;
  if (MTX_ISSYM(type)) mat_type = API_SYM_YES;
  if (MTX_ISHER(type)) mat_type = API_SYM_HER;

  /*******************************************/
  /*    Check Matrix format                  */
  /*******************************************/
  /*
   * Matrix needs :
   *    - to be in fortran numbering
   *    - to have only the lower triangular part in symmetric case
   *    - to have a graph with a symmetric structure in unsymmetric case
   */
  pastix_checkMatrix(pastix_comm, verbosemode,
                     mat_type, API_YES,
                     ncol, &colptr, &rows, &values, NULL, 1);

  /*******************************************/
  /* Initialize parameters to default values */
  /*******************************************/
  iparm[IPARM_MODIFY_PARAMETER] = API_NO;
  pastix(&pastix_data, pastix_comm,
         ncol, colptr, rows, values,
         perm, invp, rhs, 1, iparm, dparm);

  /*******************************************/
  /*       Customize some parameters         */
  /*******************************************/
  iparm[IPARM_THREAD_NBR] = nbthread;
  switch (mat_type)
  {
  case API_SYM_YES:
    iparm[IPARM_FACTORIZATION] = API_FACT_LDLT;
    break;
  case API_SYM_HER:
    iparm[IPARM_FACTORIZATION] = API_FACT_LDLH;
    break;
  default:
    iparm[IPARM_FACTORIZATION] = API_FACT_LU;
  }
  iparm[IPARM_MATRIX_VERIFICATION] = API_NO;
  iparm[IPARM_VERBOSE]             = verbosemode;
  iparm[IPARM_ORDERING]            = ordering;
  iparm[IPARM_INCOMPLETE]          = incomplete;
  iparm[IPARM_OOC_LIMIT]           = ooc;

  if (incomplete == 1)
    {
      dparm[DPARM_EPSILON_REFINEMENT] = 1e-7;
    }
  iparm[IPARM_LEVEL_OF_FILL]       = level_of_fill;
  iparm[IPARM_AMALGAMATION_LEVEL]  = amalgamation;
  iparm[IPARM_RHS_MAKING]          = API_RHS_1;
  /* reread parameters to set IPARM/DPARM */
  if (EXIT_FAILURE == get_idparm(argc, argv,
                                 iparm,          dparm))
    return EXIT_FAILURE;

  /*******************************************/
  /*           Call pastix                   */
  /*******************************************/
  perm = malloc(ncol*sizeof(pastix_int_t));
  invp = malloc(ncol*sizeof(pastix_int_t));

  /*******************************************/
  /*           Save the rhs                  */
  /*    (it will be replaced by solution)    */
  /*******************************************/
  rhsiter = (pastix_complex64_t ***) malloc(nfact*sizeof(pastix_complex64_t**));

  for (i = 0; i < nfact; i++)
    {
      rhsiter[i] = (pastix_complex64_t **)malloc(nsolv*sizeof(pastix_complex64_t*));
      for (j = 0; j < nsolv; j++)
        {
          rhsiter[i][j] = (pastix_complex64_t *)malloc(ncol*sizeof(pastix_complex64_t));
          memcpy(rhsiter[i][j], rhs, ncol*sizeof(pastix_complex64_t));
        }
    }

  /* Do nfact factorization */
  for (i = 0; i < nfact; i++)
    {
      /*******************************************/
      /*      Step 1 - Ordering / Scotch         */
      /*******************************************/
      iparm[IPARM_START_TASK] = API_TASK_ORDERING;
      iparm[IPARM_END_TASK]   = API_TASK_ORDERING;

      pastix(&pastix_data, pastix_comm,
             ncol, colptr, rows, values,
             perm, invp, rhs, 1, iparm, dparm);

      /*******************************************/
      /*      Step 2 - Symbolic factorization    */
      /*******************************************/
      iparm[IPARM_START_TASK] = API_TASK_SYMBFACT;
      iparm[IPARM_END_TASK]   = API_TASK_SYMBFACT;

      pastix(&pastix_data, pastix_comm,
             ncol, colptr, rows, values,
             perm, invp, rhs, 1, iparm, dparm);

      /*******************************************/
      /* Step 3 - Mapping and Compute scheduling */
      /*******************************************/
      iparm[IPARM_START_TASK] = API_TASK_ANALYSE;
      iparm[IPARM_END_TASK]   = API_TASK_ANALYSE;

      pastix(&pastix_data, pastix_comm,
             ncol, colptr, rows, values,
             perm, invp, rhs, 1, iparm, dparm);

      /*******************************************/
      /*     Step 4 - Numerical Factorisation    */
      /*******************************************/
      if (mpid == 0)
        fprintf(stdout, "\t> Factorisation number %ld <\n", (long)(i+1));
      iparm[IPARM_START_TASK] = API_TASK_NUMFACT;
      iparm[IPARM_END_TASK]   = API_TASK_NUMFACT;

      pastix(&pastix_data, pastix_comm,
             ncol, colptr, rows, values,
             perm, invp, rhs, 1, iparm, dparm);

      /* Do two solve */
      for (j = 0; j < nsolv; j++)
        {

          /*******************************************/
          /*     Step 5 - Solve and Refinnement      */
          /*******************************************/
          iparm[IPARM_START_TASK] = API_TASK_SOLVE;
          iparm[IPARM_END_TASK]   = API_TASK_REFINE;

          if (mpid == 0)
            fprintf(stdout, "\t>> Solve step number %ld  <<\n", (long)(j+1));

          pastix(&pastix_data, pastix_comm,
                 ncol, colptr, rows, values,
                 perm, invp, rhs, 1, iparm, dparm);
        }


      /*******************************************/
      /*     Step 6 - Clean structures           */
      /*******************************************/
      iparm[IPARM_START_TASK] = API_TASK_CLEAN;
      iparm[IPARM_END_TASK]   = API_TASK_CLEAN;

      pastix(&pastix_data, pastix_comm,
             ncol, colptr, rows, values,
             perm, invp, rhs, 1, iparm, dparm);
    }


  MPI_Finalize();

  free(colptr);
  free(rows);
  free(values);
  free(rhs);
  free(perm);
  free(invp);
  free(type);
  free(rhstype);

  return EXIT_SUCCESS;
#endif
}
