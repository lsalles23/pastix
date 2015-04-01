###
#
# @copyright (c) 2009-2014 The University of Tennessee and The University
#                          of Tennessee Research Foundation.
#                          All rights reserved.
# @copyright (c) 2012-2014 Inria. All rights reserved.
# @copyright (c) 2012-2014 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria, Univ. Bordeaux. All rights reserved.
#
###
#
# - Find HWLOC include dirs and libraries
# Use this module by invoking find_package with the form:
#  find_package(HWLOC
#               [REQUIRED]) # Fail with error if hwloc is not found
# This module finds headers and hwloc library.
# Results are reported in variables:
#  HWLOC_FOUND           - True if headers and requested libraries were found
#  HWLOC_INCLUDE_DIRS    - hwloc include directories
#  HWLOC_LIBRARY_DIRS    - Link directories for hwloc libraries
#  HWLOC_LIBRARIES       - hwloc component libraries to be linked
# The user can give specific paths where to find the libraries adding cmake
# options at configure (ex: cmake path/to/project -DHWLOC_DIR=path/to/hwloc):
#  HWLOC_DIR             - Where to find the base directory of hwloc
#  HWLOC_INCDIR          - Where to find the header files
#  HWLOC_LIBDIR          - Where to find the library files

#=============================================================================
# Copyright 2012-2013 Inria
# Copyright 2012-2013 Emmanuel Agullo
# Copyright 2012-2013 Mathieu Faverge
# Copyright 2012      Cedric Castagnede
# Copyright 2013      Florent Pruvost
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file MORSE-Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of Morse, substitute the full
#  License text for the above reference.)


if (NOT HWLOC_FOUND)
    set(HWLOC_DIR "" CACHE PATH "Root directory of HWLOC library")
    if (NOT HWLOC_FIND_QUIETLY)
        message(STATUS "A cache variable, namely HWLOC_DIR, has been set to specify the install directory of HWLOC")
    endif()
endif()

# Optionally use pkg-config to detect include/library dirs (if pkg-config is available)
# -------------------------------------------------------------------------------------
include(FindPkgConfig)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_EXECUTABLE)

    pkg_search_module(HWLOC hwloc)
    if (NOT HWLOC_FIND_QUIETLY)
        if (HWLOC_FOUND AND HWLOC_LIBRARIES)
            message(STATUS "Looking for HWLOC - found using PkgConfig")
            #if(NOT HWLOC_INCLUDE_DIRS)
            #    message("${Magenta}HWLOC_INCLUDE_DIRS is empty using PkgConfig."
            #        "Perhaps the path to hwloc headers is already present in your"
            #        "C(PLUS)_INCLUDE_PATH environment variable.${ColourReset}")
            #endif()
        else()
            message("${Magenta}Looking for HWLOC - not found using PkgConfig."
                "Perhaps you should add the directory containing hwloc.pc to"
                "the PKG_CONFIG_PATH environment variable.${ColourReset}")
        endif()
    endif()

endif(PKG_CONFIG_EXECUTABLE)

if(NOT HWLOC_FOUND OR NOT HWLOC_LIBRARIES)

    if (NOT HWLOC_FIND_QUIETLY)
        message(STATUS "Looking for HWLOC - PkgConfig not used")
    endif()

    # Looking for include
    # -------------------

    # Add system include paths to search include
    # ------------------------------------------
    unset(_inc_env)
    if(WIN32)
        string(REPLACE ":" ";" _inc_env "$ENV{INCLUDE}")
    else()
        string(REPLACE ":" ";" _path_env "$ENV{INCLUDE}")
        list(APPEND _inc_env "${_path_env}")
        string(REPLACE ":" ";" _path_env "$ENV{C_INCLUDE_PATH}")
        list(APPEND _inc_env "${_path_env}")
        string(REPLACE ":" ";" _path_env "$ENV{CPATH}")
        list(APPEND _inc_env "${_path_env}")
        string(REPLACE ":" ";" _path_env "$ENV{INCLUDE_PATH}")
        list(APPEND _inc_env "${_path_env}")
    endif()
    list(APPEND _inc_env "${CMAKE_PLATFORM_IMPLICIT_INCLUDE_DIRECTORIES}")
    list(APPEND _inc_env "${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES}")
    list(REMOVE_DUPLICATES _inc_env)

    # set paths where to look for
    set(PATH_TO_LOOK_FOR "${_inc_env}")

    # Try to find the hwloc header in the given paths
    # -------------------------------------------------
    # call cmake macro to find the header path
    if(HWLOC_INCDIR)
        set(HWLOC_hwloc.h_DIRS "HWLOC_hwloc.h_DIRS-NOTFOUND")
        find_path(HWLOC_hwloc.h_DIRS
          NAMES hwloc.h
          HINTS ${HWLOC_INCDIR})
    else()
        if(HWLOC_DIR)
            set(HWLOC_hwloc.h_DIRS "HWLOC_hwloc.h_DIRS-NOTFOUND")
            find_path(HWLOC_hwloc.h_DIRS
              NAMES hwloc.h
              HINTS ${HWLOC_DIR}
              PATH_SUFFIXES include)
        else()
            set(HWLOC_hwloc.h_DIRS "HWLOC_hwloc.h_DIRS-NOTFOUND")
            find_path(HWLOC_hwloc.h_DIRS
                      NAMES hwloc.h
                      HINTS ${PATH_TO_LOOK_FOR})
        endif()
    endif()
    mark_as_advanced(HWLOC_hwloc.h_DIRS)

    # Add path to cmake variable
    # ------------------------------------
    if (HWLOC_hwloc.h_DIRS)
        set(HWLOC_INCLUDE_DIRS "${HWLOC_hwloc.h_DIRS}")
    else ()
        set(HWLOC_INCLUDE_DIRS "HWLOC_INCLUDE_DIRS-NOTFOUND")
        if(NOT HWLOC_FIND_QUIETLY)
            message(STATUS "Looking for hwloc -- hwloc.h not found")
        endif()
    endif ()

    if (HWLOC_INCLUDE_DIRS)
        list(REMOVE_DUPLICATES HWLOC_INCLUDE_DIRS)
    endif ()


    # Looking for lib
    # ---------------

    # Add system library paths to search lib
    # --------------------------------------
    unset(_lib_env)
    if(WIN32)
        string(REPLACE ":" ";" _lib_env "$ENV{LIB}")
    else()
        if(APPLE)
            string(REPLACE ":" ";" _lib_env "$ENV{DYLD_LIBRARY_PATH}")
        else()
            string(REPLACE ":" ";" _lib_env "$ENV{LD_LIBRARY_PATH}")
        endif()
        list(APPEND _lib_env "${CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES}")
        list(APPEND _lib_env "${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}")
    endif()
    list(REMOVE_DUPLICATES _lib_env)

    # set paths where to look for
    set(PATH_TO_LOOK_FOR "${_lib_env}")

    # Try to find the hwloc lib in the given paths
    # ----------------------------------------------

    # call cmake macro to find the lib path
    if(HWLOC_LIBDIR)
        set(HWLOC_hwloc_LIBRARY "HWLOC_hwloc_LIBRARY-NOTFOUND")
        find_library(HWLOC_hwloc_LIBRARY
            NAMES hwloc
            HINTS ${HWLOC_LIBDIR})
    else()
        if(HWLOC_DIR)
            set(HWLOC_hwloc_LIBRARY "HWLOC_hwloc_LIBRARY-NOTFOUND")
            find_library(HWLOC_hwloc_LIBRARY
                NAMES hwloc
                HINTS ${HWLOC_DIR}
                PATH_SUFFIXES lib lib32 lib64)
        else()
            set(HWLOC_hwloc_LIBRARY "HWLOC_hwloc_LIBRARY-NOTFOUND")
            find_library(HWLOC_hwloc_LIBRARY
                         NAMES hwloc
                         HINTS ${PATH_TO_LOOK_FOR})
        endif()
    endif()
    mark_as_advanced(HWLOC_hwloc_LIBRARY)

    # If found, add path to cmake variable
    # ------------------------------------
    if (HWLOC_hwloc_LIBRARY)
        get_filename_component(hwloc_lib_path ${HWLOC_hwloc_LIBRARY} PATH)
        # set cmake variables (respects naming convention)
        set(HWLOC_LIBRARIES    "${HWLOC_hwloc_LIBRARY}")
        set(HWLOC_LIBRARY_DIRS "${hwloc_lib_path}")
    else ()
        set(HWLOC_LIBRARIES    "HWLOC_LIBRARIES-NOTFOUND")
        set(HWLOC_LIBRARY_DIRS "HWLOC_LIBRARY_DIRS-NOTFOUND")
        if(NOT HWLOC_FIND_QUIETLY)
            message(STATUS "Looking for hwloc -- lib hwloc not found")
        endif()
    endif ()

    if (HWLOC_LIBRARY_DIRS)
        list(REMOVE_DUPLICATES HWLOC_LIBRARY_DIRS)
    endif ()

    if(HWLOC_LIBRARIES)
        # check a function to validate the find
        if (HWLOC_INCLUDE_DIRS)
            set(CMAKE_REQUIRED_INCLUDES  "${HWLOC_INCLUDE_DIRS}")
        endif()
        set(CMAKE_REQUIRED_LIBRARIES "${HWLOC_LIBRARIES}")
        if (HWLOC_LIBRARY_DIRS)
            set(CMAKE_REQUIRED_FLAGS     "-L${HWLOC_LIBRARY_DIRS}")
        endif()

        unset(HWLOC_WORKS CACHE)
        include(CheckFunctionExists)
        check_function_exists(hwloc_topology_init HWLOC_WORKS)
        mark_as_advanced(HWLOC_WORKS)

        if(NOT HWLOC_WORKS)
            if(NOT HWLOC_FIND_QUIETLY)
                message(STATUS "Looking for hwloc : test of hwloc_topology_init with hwloc library fails")
                message(STATUS "HWLOC_LIBRARIES: ${CMAKE_REQUIRED_LIBRARIES}")
                message(STATUS "HWLOC_LIBRARY_DIRS: ${CMAKE_REQUIRED_FLAGS}")
                message(STATUS "HWLOC_INCLUDE_DIRS: ${CMAKE_REQUIRED_INCLUDES}")
                message(STATUS "Check in CMakeFiles/CMakeError.log to figure out why it fails")
                message(STATUS "Looking for hwloc : set HWLOC_LIBRARIES to NOTFOUND")
            endif()
            set(HWLOC_LIBRARIES "HWLOC_LIBRARIES-NOTFOUND")
        endif()
        set(CMAKE_REQUIRED_INCLUDES)
        set(CMAKE_REQUIRED_FLAGS)
        set(CMAKE_REQUIRED_LIBRARIES)
    endif(HWLOC_LIBRARIES)

endif(NOT HWLOC_FOUND OR NOT HWLOC_LIBRARIES)


# check that HWLOC has been found
# -------------------------------
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HWLOC DEFAULT_MSG
                                  HWLOC_LIBRARIES)

if(HWLOC_FOUND)
  set(HWLOC_SAVE_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
  list(APPEND CMAKE_REQUIRED_INCLUDES ${HWLOC_INCLUDE_DIR})
  check_struct_has_member( "struct hwloc_obj" parent hwloc.h HAVE_HWLOC_PARENT_MEMBER )
  check_struct_has_member( "struct hwloc_cache_attr_s" size hwloc.h HAVE_HWLOC_CACHE_ATTR )
  check_c_source_compiles( "#include <hwloc.h>
    int main(void) { hwloc_obj_t o; o->type = HWLOC_OBJ_PU; return 0;}" HAVE_HWLOC_OBJ_PU)
  check_library_exists(${HWLOC_LIBRARIES} hwloc_bitmap_free "" HAVE_HWLOC_BITMAP)
  set(CMAKE_REQUIRED_INCLUDES ${HWLOC_SAVE_CMAKE_REQUIRED_INCLUDES})
endif()
