/**
 *
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
#include "bcsc.h"
#include "z_bcsc.h"
#include "z_raff_functions.h"
#include "solver.h"

/**
 *******************************************************************************
 *
 * @ingroup pastix_raff
 *
 * z_grad_smp - Refine the solution using static pivoting method.
 *
 *******************************************************************************
 *
 * @param[in] pastix_data
 *          The PaStiX data structure that describes the solver instance.
 * 
 * @param[in] x
 *          The solution vector.
 * 
 * @param[in] b
 *          The right hand side member (only one).
 *
 *******************************************************************************/
void z_pivot_smp (pastix_data_t *pastix_data, void *x, void *b)
{
  /* Choix du solveur */
  struct z_solver solveur = {NULL};
  z_Pastix_Solveur(&solveur);

  /* Variables */
  pastix_bcsc_t      * bcsc           = pastix_data->bcsc;
  pastix_int_t         n              = bcsc->gN;
  Clock                raff_clk;
  double               t0             = 0;
  double               t1             = 0;
  double               t2             = 0;
  double               t3             = 0;
  pastix_complex64_t * volatile  lub  = NULL;
  pastix_complex64_t * volatile  lur  = NULL;
  pastix_complex64_t * volatile  lur2 = NULL;
  double               tmp_berr       = 0.0;
  double               berr           = 0.0;
  double               lberr          = 0.0;
  double               rberror        = 0.0;
  int                  iter           = 0;
  int                  flag           = 1;
  pastix_int_t         raffnbr        = 0.0;
  pastix_int_t         itermax;
  double               epsilonraff;

  itermax     = solveur.Itermax(pastix_data);
  epsilonraff = solveur.Eps(pastix_data);

  if (pastix_data->iparm[IPARM_VERBOSE] > API_VERBOSE_NOT)
    {
        fprintf(stdout, OUT_ITERRAFF_PIVOT);
    }
  lub  = (pastix_complex64_t *)solveur.Malloc(n * sizeof(pastix_complex64_t));
  lur  = (pastix_complex64_t *)solveur.Malloc(n * sizeof(pastix_complex64_t));
  lur2 = (pastix_complex64_t *)solveur.Malloc(n * sizeof(pastix_complex64_t));

  solveur.B(b, lub, n);

  clockInit(raff_clk);clockStart(raff_clk);

  while(flag)
    {
      iter++;
      clockStop((raff_clk));
      t0 = clockGet();

      /* r=b-ax */
      solveur.bMAx(bcsc,lub,x,lur);
//       z_CscbMAx(sopalin_data, me, lur, lub, sopalin_data->sopar->cscmtx,
//               &(datacode->updovct), datacode, pastix_comm,
//               sopar->iparm[IPARM_TRANSPOSE_SOLVE]);


      /* r'=|A||x|+|b| */
      z_bcscAxpb(PastixNoTrans, bcsc, (void *)x, (void *)lub, (void *)lur2);
//       z_CscAxPb( sopalin_data, me, lur2, lub, sopalin_data->sopar->cscmtx,
//                &(datacode->updovct), datacode, pastix_comm,
//                sopar->iparm[IPARM_TRANSPOSE_SOLVE]);



      /* tmp_berr =  max_i(|lur_i|/|lur2_i|)*/
      tmp_berr = z_bcscBerr((void *)lur,(void *)lur2,n);
//       z_CscBerr(sopalin_data, me, lur, lur2, UPDOWN_SM2XSZE,
//               1, &tmp_berr , pastix_comm);

      berr = tmp_berr;
      if (lberr == 0)
        /* force le premier raffinement */
        lberr = 3*berr;

      print_debug(DBG_RAFF_PIVOT, "RAFF : berr lberr %6g %6g\n",
                  berr, lberr);

      /* Calcul de ||r|| et ||r||/||b|| */
      tmp_berr = z_bcscNormErr((void *)lur, (void *)lub, n);

      rberror = tmp_berr;
      print_debug(DBG_RAFF_PIVOT, "RAFF : rberror %6g\n", rberror);

      if ((raffnbr < itermax)
          && (berr > epsilonraff)
          && (berr <= (lberr/2)))
        {

          /* LU dx = r */
          /* lur2 <= updo_vect (ie X_i)
           * updo_vect <= lur (ie B-AX_i)
           */
          memcpy(lur2, x, n * sizeof( pastix_complex64_t ));
          memcpy(x, lur, n * sizeof( pastix_complex64_t ));

          clockStop((raff_clk));
          t1 = clockGet();

//           z_up_down_smp(arg);
          solveur.Precond(pastix_data, b, x);

          clockStop((raff_clk));
          t2 = clockGet();

          /* updo_vect <= updo_vect (ie PRECOND(B-AX_i)) + lur2 (ie X_i) */
          z_bcscAxpy( 1.0, (void*)lur2, n, x, 1 );

          /* lastberr = berr */
          lberr = berr;
          raffnbr++;
        }
      else
        {
          flag = 0;
        }

      clockStop((raff_clk));
      t3 = clockGet();

//       if (sopar->iparm[IPARM_VERBOSE] > API_VERBOSE_NOT)
//         {
//           double sst, rst = 0.0;
//           double stt, rtt;
//           double err, berr = sopalin_data->berr;
// 
//           stt = t3 - t0;
//           sst = t2-t1;
//           MyMPI_Reduce(&sst, &rst, 1, MPI_DOUBLE, MPI_MAX, 0, pastix_comm);
// 
//           MyMPI_Reduce(&berr, &err, 1, MPI_DOUBLE, MPI_MAX, 0, pastix_comm);
//           MyMPI_Reduce(&stt,  &rtt, 1, MPI_DOUBLE, MPI_MAX, 0, pastix_comm);
//           if (SOLV_PROCNUM == 0)
//             {
//               fprintf(stdout, OUT_ITERRAFF_ITER, (int)sopalin_data->raffnbr);
//               fprintf(stdout, OUT_ITERRAFF_TTS, rst);
//               fprintf(stdout, OUT_ITERRAFF_TTT, rtt);
//               fprintf(stdout, OUT_ITERRAFF_ERR, err);
//             }
//         }
      t0 = t3;
    }

  memFree_null(lub);
  memFree_null(lur);
  memFree_null(lur2);
  itermax = raffnbr;

//   if (sopar->iparm[IPARM_END_TASK] >= API_TASK_REFINE)
//     {
//         MUTEX_LOCK(&(sopalin_data->mutex_comm));
//         sopalin_data->step_comm = COMMSTEP_END;
//         print_debug(DBG_THCOMM, "%s:%d END\n", __FILE__, __LINE__);
//         MUTEX_UNLOCK(&(sopalin_data->mutex_comm));
//         pthread_cond_broadcast(&(sopalin_data->cond_comm));
//     }

  clockStop((raff_clk));
  print_debug(DBG_SOPALIN_RAFF, "%d : refinement time %lf\n", (int)me, clockGet());
  pastix_data->dparm[DPARM_RAFF_TIME] = clockGet();
}