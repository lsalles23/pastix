/**
 * @file z_spm_norm.c
 *
 *  PaStiX spm computational routines.
 *
 *  PaStiX is a software package provided by Inria Bordeaux - Sud-Ouest,
 *  LaBRI, University of Bordeaux 1 and IPB.
 *
 * @version 1.0.0
 * @author Mathieu Faverge
 * @author Pierre Ramet
 * @author Xavier Lacoste
 * @author Theophile Terraz
 * @date 2015-06-01
 * @precisions normal z -> c d s
 *
 **/
#include "common.h"
#include "csc.h"
#include "z_spm.h"

/**
 *******************************************************************************
 *
 * @ingroup spm_internal
 *
 * frobenius_update - Update the couple (scale, sumsq) with one element when
 * computing the Froebnius norm.
 *
 * The frobenius norm is equal to scale * sqrt( sumsq ), this method allows to
 * avoid overflow in the sum square computation.
 *
 *******************************************************************************
 *
 * @param[in,out] scale
 *           On entry, the former scale
 *           On exit, the update scale to take into account the value
 *
 * @param[in,out] sumsq
 *           On entry, the former sumsq
 *           On exit, the update sumsq to take into account the value
 *
 * @param[in] value
 *          The value to integrate into the couple (scale, sumsq)
 *
 *******************************************************************************/
static inline void
frobenius_update( int nb, double *scale, double *sumsq, double *value )
{
    double absval = fabs(*value);
    double ratio;
    if ( absval != 0. ){
        if ( (*scale) < absval ) {
            ratio = (*scale) / absval;
            *sumsq = (double)nb + (*sumsq) * ratio * ratio;
            *scale = absval;
        } else {
            ratio = absval / (*scale);
            *sumsq = (*sumsq) + (double)nb * ratio * ratio;
        }
    }
}

/**
 *******************************************************************************
 *
 * @ingroup spm_internal
 *
 * z_spmFrobeniusNorm - Compute the Frobenius norm of the non distributed given
 * spm structure.
 *
 *  ||A|| = sqrt( sum( a_ij ^ 2 ) )
 *
 *******************************************************************************
 *
 * @param[in] spm
 *           The spm from which the norm need to be computed.
 *
 *******************************************************************************
 *
 * @return
 *           The computed norm
 *
 *******************************************************************************/
double
z_spmFrobeniusNorm( const pastix_csc_t *spm )
{
    pastix_int_t i, j, baseval;
    double *valptr = (double*)spm->values;
    double scale = 1.;
    double sumsq = 0.;

    if (spm->mtxtype == PastixGeneral) {
        for(i=0; i <spm->nnz; i++, valptr++) {
            frobenius_update( 1, &scale, &sumsq, valptr );

#if defined(PRECISION_z) || defined(PRECISION_c)
            valptr++;
            frobenius_update( 1, &scale, &sumsq, valptr );
#endif
        }
    }
    else {
        pastix_int_t *colptr = spm->colptr;
        pastix_int_t *rowptr = spm->rowptr;
        int nb;
        baseval = spmFindBase( spm );

        switch( spm->fmttype ){
        case PastixCSC:
            for(i=0; i<spm->n; i++, colptr++) {
                for(j=colptr[0]; j<colptr[1]; j++, rowptr++, valptr++) {
                    nb = ( i == (*rowptr-baseval) ) ? 1 : 2;
                    frobenius_update( nb, &scale, &sumsq, valptr );

#if defined(PRECISION_z) || defined(PRECISION_c)
                    valptr++;
                    frobenius_update( nb, &scale, &sumsq, valptr );
#endif
                }
            }
            break;
        case PastixCSR:
            for(i=0; i<spm->n; i++, rowptr++) {
                for(j=rowptr[0]; j<rowptr[1]; j++, colptr++, valptr++) {
                    nb = ( i == (*colptr-baseval) ) ? 1 : 2;
                    frobenius_update( nb, &scale, &sumsq, valptr );

#if defined(PRECISION_z) || defined(PRECISION_c)
                    valptr++;
                    frobenius_update( nb, &scale, &sumsq, valptr );
#endif
                }
            }
            break;
        case PastixIJV:
            for(i=0; i <spm->nnz; i++, valptr++, colptr++, rowptr++) {
                nb = ( *rowptr == *colptr ) ? 1 : 2;
                frobenius_update( nb, &scale, &sumsq, valptr );

#if defined(PRECISION_z) || defined(PRECISION_c)
                valptr++;
                frobenius_update( nb, &scale, &sumsq, valptr );
#endif
            }
            break;
        default:
            fprintf(stderr, "z_spmFrobeniusNorm: Unknown Format\n");
        }
    }

    return scale * sqrt( sumsq );
}

/**
 *******************************************************************************
 *
 * @ingroup spm_internal
 *
 * z_spmMaxNorm - Compute the Max norm of the non distributed given spm
 * structure.
 *
 *  ||A|| = max( abs(a_ij) )
 *
 *******************************************************************************
 *
 * @param[in] spm
 *           The spm from which the norm need to be computed.
 *
 *******************************************************************************
 *
 * @return
 *           The computed norm
 *
 *******************************************************************************/
double
z_spmMaxNorm( const pastix_csc_t *spm )
{
    pastix_int_t i;
    pastix_complex64_t *valptr = (pastix_complex64_t*)spm->values;
    double tmp, norm = 0.;

    for(i=0; i <spm->nnz; i++, valptr++) {
        tmp = cabs( *valptr );
        norm = norm > tmp ? norm : tmp;
    }

    return norm;
}

/**
 *******************************************************************************
 *
 * @ingroup spm_internal
 *
 * z_spmInfNorm - Compute the Infinite norm of the non distributed given spm
 * structure.
 *
 *  ||A|| =
 *
 *******************************************************************************
 *
 * @param[in] spm
 *           The spm from which the norm need to be computed.
 *
 *******************************************************************************
 *
 * @return
 *           The computed norm
 *
 *******************************************************************************/
double
z_spmInfNorm( const pastix_csc_t *spm )
{
    pastix_int_t i;
    pastix_complex64_t *valptr = (pastix_complex64_t*)spm->values;
    double tmp, norm = 0.;

    for(i=0; i <spm->nnz; i++, valptr++) {
        tmp = cabs( *valptr );
        norm = norm > tmp ? norm : tmp;
    }

    return norm;
}

/**
 *******************************************************************************
 *
 * @ingroup spm_internal
 *
 * z_spmOneNorm - Compute the One norm of the non distributed given spm
 * structure.
 *
 *  ||A|| =
 *
 *******************************************************************************
 *
 * @param[in] spm
 *           The spm from which the norm need to be computed.
 *
 *******************************************************************************
 *
 * @return
 *           The computed norm
 *
 *******************************************************************************/
double
z_spmOneNorm( const pastix_csc_t *spm )
{
    pastix_int_t i;
    pastix_complex64_t *valptr = (pastix_complex64_t*)spm->values;
    double tmp, norm = 0.;

    for(i=0; i <spm->nnz; i++, valptr++) {
        tmp = cabs( *valptr );
        norm = norm > tmp ? norm : tmp;
    }

    return norm;
}

/**
 *******************************************************************************
 *
 * @ingroup pastix_spm
 *
 * z_spmNorm - Compute the norm of an spm matrix
 *
 *******************************************************************************
 *
 * @param[in] type
 *          = PastixMaxNorm: Max norm
 *          = PastixOneNorm: One norm
 *          = PastixInfNorm: Infinity norm
 *          = PastixFrobeniusNorm: Frobenius norm
 *
 * @param[in] spm
 *          The spm structure describing the matrix.
 *
 *******************************************************************************
 *
 * @return
 *      \retval The norm of the matrix spm
 *
 *******************************************************************************/
double
z_spmNorm( int ntype,
           const pastix_csc_t *spm)
{
    double norm = 0.;

    if(spm == NULL)
    {
        return -1.;
    }

    switch( ntype ) {
    case PastixMaxNorm:
        norm = z_spmMaxNorm( spm );
        break;

    case PastixInfNorm:
        norm = z_spmInfNorm( spm );
        break;

    case PastixOneNorm:
        norm = z_spmOneNorm( spm );
        break;

    case PastixFrobeniusNorm:
        norm = z_spmFrobeniusNorm( spm );
        break;

    default:
        fprintf(stderr, "z_spmNorm: invalid norm type\n");
        return -1.;
    }

    return norm;
}
