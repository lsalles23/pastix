"""
 @file pastix.py

 PaStiX python wrapper

 @copyright 2017      Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
                      Univ. Bordeaux. All rights reserved.

 @version 6.0.0
 @author Pierre Ramet
 @author Mathieu Faverge
 @date 2017-05-04

"""
from ctypes import *
import numpy as np
from . import libpastix, __getnrhs
from .enum import *

def initParam( iparm, dparm ):
    libpastix.pastixInitParam.argtypes = [POINTER(pastix_c_int), POINTER(c_double)]
    libpastix.pastixInitParam( iparm.ctypes.data_as(POINTER(pastix_c_int)),
                                      dparm.ctypes.data_as(POINTER(c_double)) )

def init( iparm, dparm ):
    pastix_data = c_void_p();
    libpastix.pastixInit.argtypes = [POINTER(c_void_p), c_int, POINTER(pastix_c_int), POINTER(c_double)]
    libpastix.pastixInit( pointer( pastix_data ), c_int(0),
                          iparm.ctypes.data_as(POINTER(pastix_c_int)),
                          dparm.ctypes.data_as(POINTER(c_double)) )

    return pastix_data

def finalize( pastix_data, iparm, dparm ):
    libpastix.pastixFinalize.argtypes = [POINTER(c_void_p), c_int, POINTER(pastix_c_int), POINTER(c_double)]
    libpastix.pastixFinalize( pointer( pastix_data ), c_int(0),
                                     iparm.ctypes.data_as(POINTER(pastix_c_int)),
                                     dparm.ctypes.data_as(POINTER(c_double)) )

#
# Pastix Tasks
#
def analyze( pastix_data, spm ):
    libpastix.pastix_task_analyze.argtypes = [c_void_p, POINTER(spm.c_spm)]
    libpastix.pastix_task_analyze( pastix_data, spm.id_ptr )

def numfact( pastix_data, spm ):
    libpastix.pastix_task_numfact.argtypes = [c_void_p, POINTER(spm.c_spm)]
    libpastix.pastix_task_numfact( pastix_data, spm.id_ptr )

def solve( pastix_data, spm, x, nrhs=-1 ):

    n    = spm.spm_c.n
    nrhs = __getnrhs( nrhs, x )

    spm._spm__checkVector( n, nrhs, x )

    libpastix.pastix_task_solve.argtypes = [c_void_p, POINTER(spm.c_spm), pastix_c_int, c_void_p, pastix_c_int]
    libpastix.pastix_task_solve( pastix_data, spm.id_ptr, nrhs,
                                 x.ctypes.data_as(c_void_p), x.shape[0] )

def refine( pastix_data, b, x, nrhs=-1 ):

    nrhs = __getnrhs( nrhs, x )

    if b.dtype != x.dtype:
        raise TypeError( "b and x must use the same arithmetic" )

    libpastix.pastix_task_refine.argtypes = [c_void_p, c_void_p, pastix_c_int, c_void_p]
    libpastix.pastix_task_refine( pastix_data,
                                  x.ctypes.data_as(c_void_p), nrhs,
                                  b.ctypes.data_as(c_void_p) )

#
# Pastix Schur
#
def setSchurUnknownList( pastix_data, list ):
    n = list.shape[0]

    if pastix_np_int != list.dtype:
        raise TypeError( "list must use the same integer type as the pastix library" )

    libpastix.pastix_setSchurUnknownList.argtypes = [c_void_p, pastix_c_int, POINTER(pastix_c_int)]
    libpastix.pastix_setSchurUnknownList( pastix_data, n,
                                          list.ctypes.data_as(POINTER(pastix_c_int)) )

def getSchur( pastix_data, S ):
    libpastix.pastix_getSchur.argtypes = [c_void_p, c_void_p, pastix_c_int ]
    libpastix.pastix_getSchur( pastix_data,
                                      S.ctypes.data_as(c_void_p),
                                      S.shape[0] )

#
# Pastix Schur
#
def subtask_applyorder( pastix_data, dir, b ):
    flttype = pastix_coeftype.get( b.dtype )

    m = b.shape[0]
    if b.ndim == 1:
        n = 1
    else:
        n = b.shape[1]
    ldb = m

    libpastix.pastix_subtask_applyorder.argtypes = [c_void_p, c_int, c_int, pastix_c_int, pastix_c_int, c_void_p, pastix_c_int]
    libpastix.pastix_subtask_applyorder( pastix_data, flttype, dir, m, n,
                                                b.ctypes.data_as(c_void_p), ldb )

def subtask_trsm( pastix_data, side, uplo, trans, diag, b, nrhs=-1 ):
    flttype = pastix_coeftype.get( b.dtype )

    n    = b.shape[0]
    nrhs = __getnrhs( nrhs, b )
    ldb  = n

    libpastix.pastix_subtask_trsm.argtypes = [c_void_p, c_int, c_int, c_int, c_int, c_int, pastix_c_int, c_void_p, pastix_c_int]
    libpastix.pastix_subtask_trsm( pastix_data, flttype, side, uplo, trans, diag, nrhs,
                                   b.ctypes.data_as(c_void_p), ldb )

def subtask_diag( pastix_data, b ):
    flttype = pastix_coeftype.get( b.dtype )

    ldb  = b.shape[0]
    nrhs = __getnrhs( nrhs, b )

    libpastix.pastix_subtask_diag.argtypes = [c_void_p, c_int, pastix_c_int, c_void_p, pastix_c_int]
    libpastix.pastix_subtask_diag( pastix_data, flttype, nrhs,
                                   b.ctypes.data_as(c_void_p), ldb )

