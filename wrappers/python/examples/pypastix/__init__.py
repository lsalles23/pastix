"""
PyPaStiX
========

"""
import ctypes
import ctypes.util

# Load the PASTIX library
libpastix_name = ctypes.util.find_library('pastix')
if libpastix_name == None:
    raise EnvironmentError("Could not find shared library: pastix."
                           "The path to libpastix.so should be in "
                           "$LIBRARY_PATH")
try:
    libpastix = ctypes.cdll.LoadLibrary(libpastix_name)
except:
    raise EnvironmentError("Could not load shared library: pastix."
                           "The path to libpastix.so should be in "
                           "$LD_LIBRARY_PATH or $DYLD_LIBRARY_PATH on MacOS");

# Load the SPM library
libspm_name = ctypes.util.find_library('pastix_spm')
if libspm_name == None:
    raise EnvironmentError("Could not find shared library: pastix_spm."
                           "The path to libpastix_spm.so should be in "
                           "$LIBRARY_PATH")

try:
    libspm = ctypes.cdll.LoadLibrary(libspm_name)
except:
    raise EnvironmentError("Could not load shared library: pastix_spm."
                           "The path to libpastix_spm.so should be in "
                           "$LD_LIBRARY_PATH or $DYLD_LIBRARY_PATH on MacOS");

__all__ = [ 'libpastix', 'libspm' ]

from .enum   import *
from .spm    import *
from .pastix import *
from .solver import *
