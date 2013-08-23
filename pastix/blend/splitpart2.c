#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include "common.h"
#include "dof.h"
#include "cost.h"
#include "symbol.h"
#include "elimin.h"
#include "extrastruct.h"
#include "extendVector.h"
#include "cand.h"
#include "queue.h"
#include "bulles.h"
#include "blendctrl.h"
#include "ftgt.h"
#include "csc.h"
#include "updown.h"
#include "solver.h"
#include "simu.h"
#include "costfunc.h"
#include "partbuild.h"
#include "splitpart.h"


typedef struct ExtraCblk_s {
    pastix_int_t  cblknbr;
    pastix_int_t  addcblk;         /*+ Number of cblk created                       +*/
    pastix_int_t *sptcblk;         /*+ Index for splitted cblk in the cblktab       +*/
    pastix_int_t *sptcbnb;         /*+ Number of splitted cblk for a cblk           +*/
    pastix_int_t  curcblk;         /*+ Cursor for cblktab                           +*/
    pastix_int_t  sizcblk;         /*+ Size of allocated cblktab                    +*/
    SymbolCblk   *cblktab;         /*+ Array of column blocks [+1,based]            +*/
} ExtraCblk_t;

static inline void
extraCblkInit( pastix_int_t cblknbr,
               ExtraCblk_t *extracblk )
{
    extracblk->cblknbr = cblknbr;
    extracblk->addcblk = 0;
    extracblk->sptcblk = NULL;
    extracblk->sptcbnb = NULL;
    extracblk->curcblk = -1;
    extracblk->sizcblk = 0;
    extracblk->cblktab = NULL;
    return;
}

static inline void
extraCblkAlloc( ExtraCblk_t *extracblk )
{
    pastix_int_t i;

    MALLOC_INTERN( extracblk->sptcblk, extracblk->cblknbr, pastix_int_t);
    MALLOC_INTERN( extracblk->sptcbnb, extracblk->cblknbr, pastix_int_t);

    /*
     * Unsplitted cblk will keep sptcblk to -1 and sptcbnb to 1
     * Splitted cblk will have sptcbnb to the number of cblk replacing the original one,
     * and sptcblk will contain the cblktab index of the first generated cblk
     */
    for(i=0; i<extracblk->cblknbr;i++)
    {
        extracblk->sptcblk[i] = -1;
        extracblk->sptcbnb[i] =  1;
    }

    /* We choose an arbitrary size for initial allocation of bloktab and cblktab (5%) */
    extracblk->sizcblk = (extracblk->cblknbr + 20 ) / 20;
    MALLOC_INTERN( extracblk->cblktab, extracblk->cblknbr, SymbolCblk );

    return;
}

static inline void
extraCblkExit( ExtraCblk_t *extracblk )
{
    if ( extracblk->sizcblk > 0 ) {
        memFree_null( extracblk->sptcblk );
        memFree_null( extracblk->sptcbnb );
        memFree_null( extracblk->cblktab );
    }
    extracblk->addcblk = 0;
    extracblk->curcblk = -1;
    extracblk->sizcblk = 0;
    return;
}

static inline pastix_int_t
extraCblkInc( ExtraCblk_t *extracblk )
{
    /* First cblk added */
    if ( extracblk->sizcblk == 0 ) {
        extraCblkAlloc( extracblk );
    }

    extracblk->curcblk++;

    /* Check that we have enough space and make it bigger if required */
    if( extracblk->curcblk >= extracblk->sizcblk )
    {
        pastix_int_t extrasize = (extracblk->cblknbr + 20 ) / 20;
        SymbolCblk *tmp;

        assert( extracblk->curcblk == extracblk->sizcblk);
        tmp = extracblk->cblktab;
        extracblk->sizcblk += extrasize;

        /* Add 5% of original cblknbr to the cblktab */
        MALLOC_INTERN( extracblk->cblktab, extracblk->cblknbr, SymbolCblk );
        memcpy(extracblk->cblktab, tmp, sizeof(SymbolCblk)*extracblk->curcblk);

        memFree_null(tmp);
    }

    return extracblk->curcblk;
}

static inline void
extraCblkAdd( ExtraCblk_t *extracblk,
              pastix_int_t fcolnum,
              pastix_int_t lcolnum )
{
    pastix_int_t curcblk = extraCblkInc( extracblk );

    extracblk->cblktab[ curcblk ].fcolnum = fcolnum;
    extracblk->cblktab[ curcblk ].lcolnum = lcolnum;
    extracblk->cblktab[ curcblk ].bloknum = -1;
}

/*
 Function: splitOnProcs

 Parameters:
 symbmtx    - Symbolic matrix
 extracblk  -
 extracost  -
 ctrl       -
 dofptr     -
 cblknum    -
 procnbr    -
 */
void splitOnProcs2( const BlendCtrl    *ctrl,
                    const SymbolMatrix *symbmtx,
                    ExtraCblk_t        *extracblk,
                    Cand               *candtab)
{
    pastix_int_t i, cblknum;

    for(cblknum = 0; cblknum<symbmtx->cblknbr; cblknum++)
    {
        pastix_int_t blas_min_col;
        pastix_int_t blas_max_col;
        pastix_int_t candnbr;
        pastix_int_t pas;
        pastix_int_t nseq;
        pastix_int_t width;

        candnbr = candtab[ cblknum ].lcandnum
            -     candtab[ cblknum ].fcandnum + 1;

        /* Compute minimun broadness for splitting this cblk */
        if(candnbr > ctrl->ratiolimit)
        {
            blas_min_col = ctrl->blblokmin;
            blas_max_col = ctrl->blblokmax;
            }
        else
        {
            blas_min_col = ctrl->blcolmin;
            blas_max_col = ctrl->blcolmax;
        }

        /*
         * Compute the number of cblk to be generated by split,
         * for instance we choose to split at the maximum
         */
        width = symbmtx->cblktab[cblknum].lcolnum
            -   symbmtx->cblktab[cblknum].fcolnum + 1;

        if(candnbr == 1)
        {
            /*** Need to split big supernode because
             the diagonal block factorization is written
             in BLAS1 (due to the pivoting in LDLt and LU) ***/
            /*
             * if the column block size is small enough there is no need to
             * split it.
             */
            if( width <= blas_max_col)
                continue;

            pas  = blas_max_col;
            nseq = width / pas;
        }
        else
        {
            pastix_int_t abs = ctrl->abs;
            if(candnbr > ctrl->ratiolimit)
            {
                abs *= 2; /* Increase abs for 2D */
            }

            /***  If option adaptative block size is set then compute the size of a column block ***/
            if(abs > 0)
            {
                pas = width / (abs * candnbr);

                pas = MAX(pas, blas_min_col);
                pas = MIN(pas, blas_max_col);

                nseq = width / pas;
            }
            else
            {
                nseq = (int)ceil( (double)width / (double)blas_min_col );
            }
        }

        if (nseq <= 1)
            continue;
        /* /\* No parallelism available above 4 splitted cblk *\/ */
        if(nseq < 4)
            continue;

        /* Adapt the step to the segments number */
        pas = (int)( (double)width / (double)nseq );
        assert( pas > 0 );

        /* Create the new cblk */
        {
            pastix_int_t fcolnum = symbmtx->cblktab[cblknum].fcolnum;

            for(i=0;i<nseq-1;i++)
            {
                extraCblkAdd( extracblk,
                              fcolnum + pas * i,
                              fcolnum + pas * (i+1) -1 );
            }

            extraCblkAdd( extracblk,
                          fcolnum + pas * (nseq-1),
                          symbmtx->cblktab[cblknum].lcolnum );
        }

        /*
         * Mark the cblk as being splitted
         */
        extracblk->addcblk += nseq - 1;
        extracblk->sptcblk[cblknum] = extracblk->curcblk - nseq + 1;
        extracblk->sptcbnb[cblknum] = nseq;
    }
}

void
extraCblkMerge( const BlendCtrl *ctrl,
                SymbolMatrix *newsymb,
                Cand        **candtab,
                ExtraCblk_t  *extracblk )
{
    pastix_int_t  i, j, k, l;
    pastix_int_t  curbloknum, curcblknum;
    pastix_int_t  lastcblksplit;
    pastix_int_t  addblok = 0;
    pastix_int_t *newnum      = NULL;
    pastix_int_t *extranewnum = NULL;
    pastix_int_t  facing_splitted_cnt = 0;

    SymbolMatrix *oldsymb;
    Cand         *oldcand = *candtab;
    Cand         *newcand;

    SymbolCblk *curcblk;
    SymbolBlok *curblok;

    /* No splitted cblk: partition remains the same */
    if( extracblk->addcblk == 0 ) {
        return;
    }

    /* Backup the old symbol */
    MALLOC_INTERN(oldsymb, 1, SymbolMatrix);
    memcpy( oldsymb, newsymb, sizeof(SymbolMatrix) );

    /* Allocate new cblktab */
    newsymb->cblknbr = oldsymb->cblknbr + extracblk->addcblk;
    MALLOC_INTERN(newsymb->cblktab, newsymb->cblknbr+1, SymbolCblk);

    /* Allocate new candtab */
    MALLOC_INTERN(newcand, newsymb->cblknbr, Cand);

    /*
     * We use the sptcbnb array to get the new numbering of the former cblk
     * in the new symbolic matrix
     * newnum[i+1] becomes the new number of the first cblk generated from the
     * split of former cblk number i.
     */
    MALLOC_INTERN(newnum,      oldsymb->cblknbr+1,   pastix_int_t);
    MALLOC_INTERN(extranewnum, extracblk->curcblk+1, pastix_int_t);
    newnum[0] = 0;
    memcpy(newnum+1, extracblk->sptcbnb, (oldsymb->cblknbr) * sizeof(pastix_int_t));

    /* Compute number of blocks that will be generated,
     * and copy main information of cblktab and candtab */
    lastcblksplit = -1;
    for(i=0; i<oldsymb->cblknbr; i++)
    {
        pastix_int_t fbloknum = oldsymb->cblktab[i  ].bloknum;
        pastix_int_t lbloknum = oldsymb->cblktab[i+1].bloknum;
        pastix_int_t sptcbnbw = extracblk->sptcbnb[i];

        /*
         * First we compute the number of extra blocks that will be generated
         */

        /* Diagonal block */
        addblok += (((sptcbnbw+1) * sptcbnbw) / 2) - 1;
        for(j=fbloknum+1; j<lbloknum; j++)
        {
            pastix_int_t fcblknum = oldsymb->bloktab[j].cblknum;
            pastix_int_t sptfcbnb = extracblk->sptcbnb[fcblknum];
	    pastix_int_t sptcbnbh = 0;

            /* If facing cblk is splitted */
            if ( sptfcbnb > 1 )
            {
                SymbolCblk  *newfcblk =  &(extracblk->cblktab[ extracblk->sptcblk[fcblknum] ]);
                pastix_int_t frownum  = oldsymb->bloktab[j].frownum;
                pastix_int_t lrownum  = oldsymb->bloktab[j].lrownum;

                /* Compute how many times the block is splitted horizontally */
                for(k = 0; k < sptfcbnb; k++, newfcblk++)
                {
                    /* This block doesn't face this new cblk */
                    if ( frownum > newfcblk->lcolnum )
                        continue;

                    /* No more facing cblk will be found */
                    if ( lrownum < newfcblk->fcolnum )
                        break;

                    assert( frownum <= lrownum );
                    sptcbnbh++;
                    frownum = newfcblk->lcolnum+1;
                }
            }
	    else
	      sptcbnbh = 1;

            /*
             * The number of extra blocks is the number of times the block
             * is psplitted horizontally times the number of time the cblk
             * is splitted vertically minu itself
             */
            addblok += sptcbnbw * sptcbnbh - 1;
        }

        /*
         * Second, we create newnum/extranewnum arrays and copy information into
         * cblktab and candtab
         */
        {
            /* This cblk is splitted, we generate new cblktab from extra */
            pastix_int_t newcblknum = newnum[i];
            if (sptcbnbw > 1) {
                pastix_int_t nbcblk2copy = (i - lastcblksplit - 1);
                pastix_int_t sptcblk = extracblk->sptcblk[i];

                /* Copy the previous unchanged cblks from oldsymb */
                if ( nbcblk2copy > 0 ) {
                    lastcblksplit++;
                    memcpy( newsymb->cblktab + newnum[ lastcblksplit ],
                            oldsymb->cblktab + lastcblksplit,
                            nbcblk2copy * sizeof(SymbolCblk) );

                    memcpy( newcand + newnum[ lastcblksplit ],
                            oldcand + lastcblksplit,
                            nbcblk2copy * sizeof(Cand) );
                }

                /* Copy the new cblk from extracblk */
                assert( (sptcblk >= 0) && (sptcblk <= extracblk->curcblk) );
                memcpy( newsymb->cblktab   + newcblknum,
                        extracblk->cblktab + sptcblk,
                        sptcbnbw * sizeof(SymbolCblk) );

                /* Initialize extranewnum and duplicate the cand for each new cblk */
                for(j=0; j<sptcbnbw; j++, sptcblk++) {
                    extranewnum[sptcblk] = newcblknum+j;

                    assert( (extranewnum[sptcblk] >= 0) &&
                            (extranewnum[sptcblk] <  newsymb->cblknbr) );

                    memcpy( newcand + extranewnum[ sptcblk ],
                            oldcand + i, sizeof(Cand) );
                }

                lastcblksplit = i;
            }
            /* Update newnum of following cblk (newnum is allocated with one extra space) */
            newnum[i+1] += newcblknum;
        }
    }

    /* Copy last unsplitted block */
    if ( lastcblksplit < (oldsymb->cblknbr-1) )
    {
        pastix_int_t nbcblk2copy = oldsymb->cblknbr - lastcblksplit - 1;
        lastcblksplit++;
        memcpy( newsymb->cblktab + newnum[ lastcblksplit ],
                oldsymb->cblktab + lastcblksplit,
                nbcblk2copy * sizeof(SymbolCblk) );

        memcpy( newcand + newnum[ lastcblksplit ],
                oldcand + lastcblksplit,
                nbcblk2copy * sizeof(Cand) );
    }
    memFree_null(oldcand);

    pastix_print( ctrl->clustnum, 0,
                  "Number of column blocks modified by splitting: %ld\n"
                  "Number of column blocks created by splitting : %ld\n"
                  "Number of blocks creating by splitting       : %ld\n"
                  "Oldsymbol bloknbr = %ld, cblknbr = %ld\n"
                  "Newsymbol bloknbr = %ld, cblknbr = %ld\n",
                  (long int)(extracblk->curcblk + 1),
                  (long int)(extracblk->addcblk),
                  (long int)(addblok),
		  oldsymb->bloknbr, oldsymb->cblknbr,
		  newsymb->bloknbr + addblok, newsymb->cblknbr );

    /* Allocate new bloktab */
    newsymb->bloknbr = oldsymb->bloknbr + addblok;
    MALLOC_INTERN(newsymb->bloktab, newsymb->bloknbr,   SymbolBlok);

    /* Fill in the new symbolic matrix resulting from the splitting of the former one */
    curbloknum = 0;
    curcblknum = 0;
    curcblk = newsymb->cblktab;
    curblok = newsymb->bloktab;
    for(i=0; i<oldsymb->cblknbr; i++)
    {
        pastix_int_t fbloknum = oldsymb->cblktab[i  ].bloknum;
        pastix_int_t lbloknum = oldsymb->cblktab[i+1].bloknum;
        pastix_int_t sptcbnbw = extracblk->sptcbnb[i];

        /* For each new column created by the cblk split */
        for(j=0; j<sptcbnbw; j++, curcblknum++, curcblk++)
        {
            /* Store diagonal bloknum */
            curcblk->bloknum = curbloknum;

            /* Create odb due to the splitting of the diag blok */
            for(k=0; k<(sptcbnbw-j); k++, curbloknum++, curblok++)
            {
                curblok->frownum = curcblk[k].fcolnum;
                curblok->lrownum = curcblk[k].lcolnum;
                curblok->cblknum = curcblknum + k;
                curblok->levfval = 0;
            }
            /* Next cblk will have one block less on the diagonal */

            /* Create other off diagonal blocks */
            for(k=fbloknum+1; k<lbloknum; k++)
            {
                pastix_int_t fcblknum = oldsymb->bloktab[k].cblknum;
                pastix_int_t frownum  = oldsymb->bloktab[k].frownum;
                pastix_int_t lrownum  = oldsymb->bloktab[k].lrownum;
                pastix_int_t sptfcblk = extracblk->sptcblk[fcblknum];
                pastix_int_t sptfcbnb = extracblk->sptcbnb[fcblknum];

                /* If facing cblk is splitted */
                if ( sptfcbnb > 1 )
                {
                    pastix_int_t newfcblknum = extranewnum[ sptfcblk ];
                    SymbolCblk  *newfcblk =  &(extracblk->cblktab[ sptfcblk ]);

                    /* Create new blocks facing this cblk */
                    for(l=0; l<sptfcbnb; l++, newfcblk++)
                    {
                        /* This block doesn't face this new cblk */
                        if ( frownum > newfcblk->lcolnum )
                            continue;

                        /* No more facing cblk will be found */
                        if ( lrownum < newfcblk->fcolnum )
                            break;

                        assert( frownum <= lrownum );
                        assert( frownum >= newfcblk->fcolnum );

                        curblok->frownum = frownum;
                        curblok->lrownum = pastix_imin( lrownum, newfcblk->lcolnum );
                        curblok->cblknum = newfcblknum + l;
                        curblok->levfval = 0;
                        curblok++; curbloknum++;

                        frownum = newfcblk->lcolnum+1;
                        facing_splitted_cnt++;
                    }
                    /* Remove one for previously existing block */
                    facing_splitted_cnt--;
                }
                else
                {
                    curblok->frownum = frownum;
                    curblok->lrownum = lrownum;
                    curblok->cblknum = newnum[fcblknum];
                    curblok->levfval = 0;
                    curblok++; curbloknum++;
                }
            }
        }
    }

    assert(curcblknum == newsymb->cblknbr);
    assert(curbloknum == newsymb->bloknbr);
    assert((curcblk - newsymb->cblktab) == newsymb->cblknbr);
    assert((curblok - newsymb->bloktab) == newsymb->bloknbr);

    /* Free old versions and temporary buffer */
    symbolExit(oldsymb);
    memFree_null(oldsymb);
    memFree_null(newnum);
    memFree_null(extranewnum);

    /* Virtual cblk to avoid side effect in the loops on cblk bloks */
    newsymb->cblktab[newsymb->cblknbr].fcolnum = newsymb->cblktab[newsymb->cblknbr-1].lcolnum+1;
    newsymb->cblktab[newsymb->cblknbr].lcolnum = newsymb->cblktab[newsymb->cblknbr-1].lcolnum+1;
    newsymb->cblktab[newsymb->cblknbr].bloknum = curbloknum;

    if (ctrl->iparm[IPARM_VERBOSE] > API_VERBOSE_YES)
    {
        double        block_height_sum = 0.0;
        double        cblk_width_sum = 0.0;
        for (j = 0; j < newsymb->cblknbr; j++)
        {
            cblk_width_sum += (double)(newsymb->cblktab[j].lcolnum - newsymb->cblktab[j].fcolnum + 1);

            for (i = newsymb->cblktab[j].bloknum+1; i < newsymb->cblktab[j+1].bloknum; i++)
            {
                block_height_sum += (double)(newsymb->bloktab[i].lrownum - newsymb->bloktab[i].frownum + 1);
            }
        }
        fprintf(stdout, "Average cblk size : %g\n", cblk_width_sum/newsymb->cblknbr);
        fprintf(stdout, "Average extra diagonal block height : %g\n", block_height_sum/(newsymb->bloknbr-newsymb->cblknbr));
        fprintf(stdout, "Number of blocks created due to facing block splitting : %d\n", (int)facing_splitted_cnt);
    }

    symbolCheck(newsymb);

    *candtab = newcand;
    return;
}




/*
 Function: splitPart

 Repartitioning of the initial symbolic factorization
 and processing of candidate processors group for
 each colum bloc

 Parameters:
 symbmtx - Symbolic matrix.
 ctrl    -
 dofptr  -
 */
void splitPart2( BlendCtrl    *ctrl,
                 SymbolMatrix *symbmtx )
{
    ExtraCblk_t extracblk;

    extraCblkInit( symbmtx->cblknbr, &extracblk );

    /* Stupid split */
    splitOnProcs2( ctrl, symbmtx, &extracblk, ctrl->candtab );

    /* Rebuild the symbolic matrix */
    {
        Clock timer;
        clockStart(timer);
        extraCblkMerge( ctrl, symbmtx, &(ctrl->candtab), &extracblk );
        clockStop(timer);

        pastix_print( 0, 0, "symbolMerge perform in %e s\n",
                      clockVal(timer) );
    }

    extraCblkExit(&extracblk);

    /*********************************/
    /* Restore the elimination tree **/
    /*********************************/
    {
        eTreeExit(ctrl->etree);
        MALLOC_INTERN(ctrl->etree, 1, EliminTree);
        eTreeInit(ctrl->etree);
        eTreeBuild(ctrl->etree, symbmtx);
    }

    /*********************************/
    /* Restore cost matrix          **/
    /*********************************/
    {
        costExit(ctrl->costmtx);
        MALLOC_INTERN( ctrl->costmtx, 1, CostMatrix);
        costInit(ctrl->costmtx);
        costMatrixBuild(ctrl->costmtx, symbmtx, NULL );
    }

    candBuild( ctrl->autolevel,
               ctrl->level2D,
               ctrl->ratiolimit,
               ctrl->candtab,
               ctrl->etree,
               symbmtx,
               ctrl->costmtx );
}
