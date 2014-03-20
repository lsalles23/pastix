
/*
**  The defines and includes.
*/

#define SOLVER_IO

#include "common.h"
#include "symbol.h"
#include "ftgt.h"
#include "queue.h"
#include "bulles.h"
#include "csc.h"
#include "updown.h"
#include "solver.h"
#include "solverRealloc.h"
#include "solver_io.h"



/*****************************************/
/* The solver matrix handling routines.  */
/*                                       */
/*****************************************/

/*+ This routine reads the given
*** solver matrix structure from
*** the given stream. The solver matrix
*** structure must have been
*** previously initialized.
*** It returns:
*** - 0   : on success.
*** - !0  : on error.
+*/

pastix_int_t solverLoad(SolverMatrix *solvptr, FILE *stream)
{
    pastix_int_t i,j;
    pastix_int_t clustnbr, clustnum;
    SolverCblk   *cblkptr;
    SolverCblk   *cblktnd;
    SolverBlok   *blokptr;
    SolverBlok   *bloktnd;
    FanInTarget  *ftgtptr;
    FanInTarget  *ftgttnd;
    Task         *taskptr;
    Task         *tasknd;

    pastix_int_t                 versval;
    pastix_int_t                 baseval;
    pastix_int_t                 nodenbr;
    pastix_int_t                 cblknbr;
    pastix_int_t                 cblknum;
    pastix_int_t                 bloknbr;
    pastix_int_t                 bloknum;

    solverInit(solvptr);

    /** Load the symbol matrix **/
    if ((intLoad (stream, &versval) +               /* Read header */
         intLoad (stream, &cblknbr) +
         intLoad (stream, &bloknbr) +
         intLoad (stream, &nodenbr) +
         intLoad (stream, &baseval) != 5) ||
        (versval < 0)                     ||        /* Version should be 0 or 1 */
        (versval > 1)                     ||
        (bloknbr < cblknbr)               ||
        (nodenbr < cblknbr)) {
      errorPrint ("solverLoad: bad input (1)");
      return     (1);
    }
    MALLOC_INTERN(solvptr->cblktab, cblknbr + 1, SolverCblk);
    MALLOC_INTERN(solvptr->bloktab, bloknbr,     SolverBlok);
    if (solvptr->cblktab == NULL || solvptr->bloktab == NULL) {
      errorPrint ("solverLoad: out of memory");
      solverExit (solvptr);
      solverInit (solvptr);
      return     (1);
    }
    solvptr->baseval = baseval;
    solvptr->cblknbr = cblknbr;
    solvptr->bloknbr = bloknbr;
    solvptr->nodenbr = nodenbr;

    for (cblknum = 0; cblknum < cblknbr; cblknum ++) {
      if ((intLoad (stream, &solvptr->cblktab[cblknum].fcolnum) + /* Read column blocks */
           intLoad (stream, &solvptr->cblktab[cblknum].lcolnum) +
           intLoad (stream, &solvptr->cblktab[cblknum].bloknum) != 3) ||
          (solvptr->cblktab[cblknum].fcolnum > solvptr->cblktab[cblknum].lcolnum)) {
        errorPrint ("solverLoad: bad input (2)");
        /* solverExit (solvptr); */
        /* solverInit (solvptr); */
        return     (1);
      }
    }
    solvptr->cblktab[cblknbr].fcolnum =             /* Set last column block */
      solvptr->cblktab[cblknbr].lcolnum = nodenbr + baseval;
    solvptr->cblktab[cblknbr].bloknum = bloknbr + baseval;

    for (bloknum = 0; bloknum < bloknbr; bloknum ++) {
      if ((intLoad (stream, &solvptr->bloktab[bloknum].frownum) + /* Read column blocks */
           intLoad (stream, &solvptr->bloktab[bloknum].lrownum) +
           intLoad (stream, &solvptr->bloktab[bloknum].cblknum) != 3) ||
          (solvptr->bloktab[bloknum].frownum > solvptr->bloktab[bloknum].lrownum)) {
        errorPrint ("solverLoad: bad input (3)");
        solverExit (solvptr);
        solverInit (solvptr);
        return     (1);
    }

    solvptr->bloktab[bloknum].levfval = 0;        /* Assume version 0 */
    if ((versval > 0) &&
        ((intLoad (stream, &solvptr->bloktab[bloknum].levfval) != 1) ||
         (solvptr->bloktab[bloknum].levfval < 0))) {
      errorPrint ("solverLoad: bad input (4)");
      solverExit (solvptr);
      solverInit (solvptr);
      return     (1);
    }
  }


    if(  intLoad (stream, &solvptr->coefnbr) +
         intLoad (stream, &solvptr->ftgtnbr) +
         intLoad (stream, &solvptr->coefmax) +
         intLoad (stream, &solvptr->bpftmax) +
         intLoad (stream, &solvptr->cpftmax) +
         intLoad (stream, &solvptr->nbftmax) +
         intLoad (stream, &solvptr->arftmax) +
         intLoad (stream, &clustnum) +
         intLoad (stream, &clustnbr) +
         intLoad (stream, &solvptr->indnbr) +
         intLoad (stream, &solvptr->tasknbr) +
         intLoad (stream, &solvptr->procnbr) +
         intLoad (stream, &solvptr->thrdnbr) +
         intLoad (stream, &solvptr->gridldim) + intLoad (stream, &solvptr->gridcdim)
         != 13)
        {
            errorPrint ("solverLoad: bad input (1)");
            return     (1);
        }

    solvptr->clustnbr = (pastix_int_t)clustnbr;
    solvptr->clustnum = (pastix_int_t)clustnum;


    if (((solvptr->cblktab = (SolverCblk *) memAlloc ((solvptr->cblknbr + 1) * sizeof (SolverCblk)))  == NULL) ||
        ((solvptr->bloktab = (SolverBlok *) memAlloc (solvptr->bloknbr       * sizeof (SolverBlok)))  == NULL) ||
        ((solvptr->ftgttab = (FanInTarget *)memAlloc (solvptr->ftgtnbr               * sizeof (FanInTarget))) == NULL) ||
        ((solvptr->indtab  = (pastix_int_t *)        memAlloc (solvptr->indnbr                * sizeof (pastix_int_t)))         == NULL) ||
        ((solvptr->tasktab = (Task *)       memAlloc ((solvptr->tasknbr+1)           * sizeof (Task)))        == NULL) ||
        ((solvptr->ttsknbr = (pastix_int_t *)        memAlloc ((solvptr->thrdnbr)             * sizeof (pastix_int_t)))         == NULL) ||
        ((solvptr->ttsktab = (pastix_int_t **)       memAlloc ((solvptr->thrdnbr)             * sizeof (pastix_int_t *)))       == NULL)
        ) {
        errorPrint ("solverLoad: out of memory (1)");
        if (solvptr->cblktab != NULL)
          memFree_null (solvptr->cblktab);
        if (solvptr->bloktab != NULL)
          memFree_null (solvptr->bloktab);
        if (solvptr->ftgttab != NULL)
          memFree_null (solvptr->ftgttab);
        if (solvptr->indtab != NULL)
          memFree_null (solvptr->indtab);
        if (solvptr->tasktab != NULL)
          memFree_null (solvptr->tasktab);
        return (1);
    }

    for (cblkptr = solvptr->cblktab,                /* Read column block data */
           cblktnd = cblkptr + solvptr->cblknbr;
         cblkptr < cblktnd; cblkptr ++){
      if (intLoad (stream, &cblkptr->stride) +
          intLoad (stream, &cblkptr->color) +
          intLoad (stream, &cblkptr->procdiag) +
          intLoad (stream, &cblkptr->cblkdiag) != 4)
          {
            errorPrint ("solverlLoad: bad input (2)");
            solverExit (solvptr);
            return     (1);
          }
#ifdef SOLVER_DEBUG
      /*intLoad (stream, &cblkptr->color);*/
#endif

      }

        for (blokptr = solvptr->bloktab,                /* Read block data */
             bloktnd = blokptr + solvptr->bloknbr;
             blokptr < bloktnd; blokptr ++) {
          if (intLoad (stream, &blokptr->coefind) != 1)
            {
              errorPrint ("solverLoad: bad input (3)");
              solverExit (solvptr);
              return     (1);
            }

        }


    for (ftgtptr = solvptr->ftgttab,                /* Read fan in target data */
           ftgttnd = ftgtptr + solvptr->ftgtnbr;
         ftgtptr < ftgttnd; ftgtptr ++)
      {
        for(i=0;i<MAXINFO;i++)
          intLoad (stream, &(ftgtptr->infotab[i]));
        ftgtptr->coeftab = NULL;
      }

   for(i=0;i<solvptr->indnbr;i++)                   /** Read indtab **/
     intLoad(stream, &(solvptr->indtab[i]));


   for (taskptr = solvptr->tasktab,                /** Read Task data **/
            tasknd = taskptr + solvptr->tasknbr +1;
        (taskptr < tasknd); taskptr ++)
   {
       pastix_int_t temp;

       intLoad(stream, &(taskptr->taskid));
       intLoad(stream, &(taskptr->prionum));
       intLoad(stream, &(taskptr->cblknum));
       intLoad(stream, &(taskptr->bloknum));
       {
           /* volatile pb alpha */
           intLoad(stream, &temp);
           taskptr->ftgtcnt = temp;
           intLoad(stream, &temp);
           taskptr->ctrbcnt = temp;
       }
       intLoad(stream, &(taskptr->indnum));
   }

   for(i=0;i<solvptr->thrdnbr;i++)                 /** Read task by thread data **/
     {
       intLoad(stream, &(solvptr->ttsknbr[i]));
       MALLOC_INTERN(solvptr->ttsktab[i], solvptr->ttsknbr[i], pastix_int_t);
       if (solvptr->ttsktab[i] == NULL)
         {
           errorPrint ("solverLoad: out of memory (1)");
           return 1;
         }
       for (j=0;j<solvptr->ttsknbr[i];j++)
         {
           intLoad(stream, &(solvptr->ttsktab[i][j]));
         }
     }

   for(i=0;i<solvptr->procnbr;i++)                 /** Read proc -> cluster **/
     {
       intLoad(stream, &(solvptr->proc2clust[i]));
     }

   if(  intLoad (stream, &(solvptr->updovct.sm2xmax)) + /** Read updown **/
        intLoad (stream, &(solvptr->updovct.sm2xsze)) +
        intLoad (stream, &(solvptr->updovct.sm2xnbr)) +
        intLoad (stream, &(solvptr->updovct.gcblk2listnbr)) +
        intLoad (stream, &(solvptr->updovct.listptrnbr)) +
        intLoad (stream, &(solvptr->updovct.listnbr)) +
        intLoad (stream, &(solvptr->updovct.loc2globnbr)) +
        intLoad (stream, &(solvptr->updovct.gcblknbr)) +
        intLoad (stream, &(solvptr->updovct.gnodenbr))
        != 9)
     {
       errorPrint ("solverLoad: bad input (1)");
       return     (1);
     }

   MALLOC_INTERN(solvptr->updovct.cblktab,    solvptr->cblknbr,       UpDownCblk);
   MALLOC_INTERN(solvptr->updovct.gcblk2list, solvptr->updovct.gcblk2listnbr, pastix_int_t);
   MALLOC_INTERN(solvptr->updovct.listptr,    solvptr->updovct.listptrnbr,    pastix_int_t);
   MALLOC_INTERN(solvptr->updovct.listcblk,   solvptr->updovct.listnbr,       pastix_int_t);
   MALLOC_INTERN(solvptr->updovct.listblok,   solvptr->updovct.listnbr,       pastix_int_t);
   MALLOC_INTERN(solvptr->updovct.loc2glob,   solvptr->updovct.loc2globnbr,   pastix_int_t);
   MALLOC_INTERN(solvptr->updovct.lblk2gcblk, solvptr->bloknbr,       pastix_int_t);

   for (i=0;i<solvptr->cblknbr;i++)
     {
       intLoad(stream, &(solvptr->updovct.cblktab[i].sm2xind));
       intLoad(stream, &(solvptr->updovct.cblktab[i].browprocnbr));
       intLoad(stream, &(solvptr->updovct.cblktab[i].msgnbr));
       {
         pastix_int_t msgcnt = solvptr->updovct.cblktab[i].msgcnt;
         intLoad(stream, &msgcnt);
       }
       intLoad(stream, &(solvptr->updovct.cblktab[i].ctrbnbr));
       {
         pastix_int_t ctrbcnt = solvptr->updovct.cblktab[i].ctrbcnt;
         intLoad(stream, &ctrbcnt);
       }

       MALLOC_INTERN(solvptr->updovct.cblktab[i].browproctab,
                     solvptr->updovct.cblktab[i].browprocnbr,
                     pastix_int_t);
       MALLOC_INTERN(solvptr->updovct.cblktab[i].browcblktab,
                     solvptr->updovct.cblktab[i].browprocnbr,
                     pastix_int_t);

       for (j=0;j<solvptr->updovct.cblktab[i].browprocnbr;j++)
         {
           intLoad(stream, &(solvptr->updovct.cblktab[i].browproctab[j]));
           intLoad(stream, &(solvptr->updovct.cblktab[i].browcblktab[j]));
         }
     }

   for (i=0;i<solvptr->updovct.gcblk2listnbr;i++)
     {
       intLoad(stream, &(solvptr->updovct.gcblk2list[i]));
     }

   for (i=0;i<solvptr->updovct.listptrnbr;i++)
     {
       intLoad(stream, &(solvptr->updovct.listptr[i]));
     }

   for (i=0;i<solvptr->updovct.listnbr;i++)
     {
       intLoad(stream, &(solvptr->updovct.listcblk[i]));
       intLoad(stream, &(solvptr->updovct.listblok[i]));
     }

   for (i=0;i<solvptr->updovct.loc2globnbr;i++)
     {
       intLoad(stream, &(solvptr->updovct.loc2glob[i]));
     }

   for (i=0;i<solvptr->bloknbr;i++)
     {
       intLoad(stream, &(solvptr->updovct.lblk2gcblk[i]));
     }

   return (0);
}


pastix_int_t solverSave(const SolverMatrix * solvptr, FILE *stream)
{
   pastix_int_t i;
   pastix_int_t j;
   SolverCblk   *cblkptr;
   SolverCblk   *cblktnd;
   SolverBlok   *blokptr;
   SolverBlok   *bloktnd;
   FanInTarget  *ftgtptr;
   FanInTarget  *ftgttnd;
   Task         *taskptr;
   Task         *tasknd;

   pastix_int_t          o;


   /** Save the solver matrix **/
   {
     const SolverCblk *  cblktnd;
     const SolverCblk *  cblkptr;
     const SolverBlok *  bloktnd;
     const SolverBlok *  blokptr;
     int                 o;

     o = (fprintf (stream, "1\n%ld\t%ld\t%ld\t%ld\n", /* Write file header */
                   (long) solvptr->cblknbr,
                   (long) solvptr->bloknbr,
                   (long) solvptr->nodenbr,
                   (long) solvptr->baseval) == EOF);
     for (cblkptr = solvptr->cblktab, cblktnd = cblkptr + solvptr->cblknbr;
          (cblkptr < cblktnd) && (o == 0); cblkptr ++) {
       o = (fprintf (stream, "%ld\t%ld\t%ld\n",
                     (long) cblkptr->fcolnum,
                     (long) cblkptr->lcolnum,
                     (long) cblkptr->bloknum) == EOF);
     }
     for (blokptr = solvptr->bloktab, bloktnd = blokptr + solvptr->bloknbr;
          (blokptr < bloktnd) && (o == 0); blokptr ++) {
       o = (fprintf (stream, "%ld\t%ld\t%ld\t%ld\n",
                     (long) blokptr->frownum,
                     (long) blokptr->lrownum,
                     (long) blokptr->cblknum,
                     (long) blokptr->levfval) == EOF);
     }

   }



   /*fprintf(stream, "File header\n");*/
   o = (fprintf (stream, "\n%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n", /* Write file header */
                 (long) solvptr->coefnbr,
                 (long) solvptr->ftgtnbr,
                 (long) solvptr->coefmax,
                 (long) solvptr->bpftmax,
                 (long) solvptr->cpftmax,
                 (long) solvptr->nbftmax,
                 (long) solvptr->arftmax,
                 (long) solvptr->clustnum,
                 (long) solvptr->clustnbr,
                 (long) solvptr->indnbr,
                 (long) solvptr->tasknbr,
                 (long) solvptr->procnbr,
                 (long) solvptr->thrdnbr,
                 (long) solvptr->gridldim,
                 (long) solvptr->gridcdim
                 ) == EOF);
   /* write cblk data */
   /*fprintf(stream, "cblk data\n");*/
   for (cblkptr = solvptr->cblktab, cblktnd = cblkptr + solvptr->cblknbr;
        (cblkptr < cblktnd) && (o == 0); cblkptr ++)
     {
       o = (fprintf (stream, "%ld\t%ld\t%ld\t%ld\n",
                     (long) cblkptr->stride,
                     (long) cblkptr->color,
                     (long) cblkptr->procdiag,
                     (long) cblkptr->cblkdiag) == EOF);
#ifdef SOLVER_DEBUG
       /*fprintf(stream, "%ld\n", (long)cblkptr->color);*/
#endif
     }


   /* write blok data */
   /*fprintf(stream, "blok data\n");*/
   for (blokptr = solvptr->bloktab, bloktnd = blokptr + solvptr->bloknbr;
        (blokptr < bloktnd) && (o == 0); blokptr ++) {
     o = (fprintf (stream, "%ld\n",(long) blokptr->coefind) == EOF);

   }

   /*fprintf(stream, "fan in target data\n");*/
   for (ftgtptr = solvptr->ftgttab,                /* Write fan in target data */
        ftgttnd = ftgtptr + solvptr->ftgtnbr;
        (ftgtptr < ftgttnd) && (o==0); ftgtptr ++) {
     for(i=0;i<MAXINFO;i++)
       o = (fprintf(stream, "%ld\t", (long)ftgtptr->infotab[i]) == EOF);
     fprintf(stream, "\n");
     fprintf(stream, "\n");
   }

   /*fprintf(stream, "indtab data\n");*/
   for(i=0;i<solvptr->indnbr;i++)                   /** Write indtab **/
     fprintf(stream, "%ld\t", (long)solvptr->indtab[i]);
   fprintf(stream, "\n");
   fprintf(stream, "\n");

   /*fprintf(stream, "task data\n");*/
   for (taskptr = solvptr->tasktab,                /* Write Task data */
            tasknd = taskptr + solvptr->tasknbr+1;
        (taskptr < tasknd) && (o==0); taskptr ++)
   {
       fprintf(stream, "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
               (long)taskptr->taskid, (long)taskptr->prionum, (long)taskptr->cblknum, (long)taskptr->bloknum,
               (long)taskptr->ftgtcnt, (long)taskptr->ctrbcnt, (long)taskptr->indnum);
       fprintf(stream, "\n");
       fprintf(stream, "\n");
   }

   /*fprintf(stream, "ttsktab\n");*/
   for (i=0; i<solvptr->thrdnbr; i++) /* Write ttsktab */
     {
       fprintf(stream, "%ld\n", (long)solvptr->ttsknbr[i]);
       for (j=0; j<solvptr->ttsknbr[i]; j++)
         {
           fprintf(stream, "%ld\n", (long)solvptr->ttsktab[i][j]);
         }
     }

   /*fprintf(stream, "proc2clust\n");*/
   for (i=0; i<solvptr->procnbr; i++) /* Write proc2clust */
     {
       fprintf(stream, "%ld\n", (long)solvptr->proc2clust[i]);
     }

   /*fprintf(stream, "updo\n");*/
   fprintf(stream, "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
           (long) solvptr->updovct.sm2xmax, (long) solvptr->updovct.sm2xsze, (long) solvptr->updovct.sm2xnbr,
           (long) solvptr->updovct.gcblk2listnbr, (long) solvptr->updovct.listptrnbr, (long) solvptr->updovct.listnbr,
           (long) solvptr->updovct.loc2globnbr, (long) solvptr->updovct.gcblknbr, (long) solvptr->updovct.gnodenbr);

   /*fprintf(stream, "updown cblk\n");*/
   for (i=0; i<solvptr->cblknbr; i++)
     {
       fprintf(stream, "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
               (long) solvptr->updovct.cblktab[i].sm2xind, (long) solvptr->updovct.cblktab[i].browprocnbr, (long) solvptr->updovct.cblktab[i].msgnbr,
               (long) solvptr->updovct.cblktab[i].msgcnt, (long) solvptr->updovct.cblktab[i].ctrbnbr, (long) solvptr->updovct.cblktab[i].ctrbcnt);

       for (j=0; j<solvptr->updovct.cblktab[i].browprocnbr; j++)
         {
           fprintf(stream, "%ld\t%ld\t\n",
                   (long) solvptr->updovct.cblktab[i].browproctab[j],
                   (long) solvptr->updovct.cblktab[i].browcblktab[j]);
         }
     }

   /*fprintf(stream, "updown gcblk2list\n");*/
   for (i=0; i<solvptr->updovct.gcblk2listnbr; i++)
     {
       fprintf(stream, "%ld\n", (long) solvptr->updovct.gcblk2list[i]);
     }

   /*fprintf(stream, "updown listptr\n");*/
   for (i=0; i<solvptr->updovct.listptrnbr; i++)
     {
       fprintf(stream, "%ld\n", (long) solvptr->updovct.listptr[i]);
     }

   /*fprintf(stream, "updown listcblk & listblok\n");*/
   for (i=0; i<solvptr->updovct.listnbr; i++)
     {
       fprintf(stream, "%ld\t%ld\n", (long) solvptr->updovct.listcblk[i], (long) solvptr->updovct.listblok[i]);
     }

   /*fprintf(stream, "updown loc2globcblk\n");*/
   for (i=0; i<solvptr->updovct.loc2globnbr; i++)
     {
       fprintf(stream, "%ld\n", (long) solvptr->updovct.loc2glob[i]);
     }

   /*fprintf(stream, "updown lblk2gcblk\n");*/
   for (i=0; i<solvptr->bloknbr; i++)
     {
       fprintf(stream, "%ld\n", (long) solvptr->updovct.lblk2gcblk[i]);
     }


   return o;
 }
