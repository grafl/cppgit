# find git2 library
#
#  GIT2_INCLUDE_DIRS - where is the git2.h located
#  GIT2_LIBRARIES    - list of libraries when using libgit2.
#  GIT2_FOUND        - true if libgit2 is found

# GIT2_INCLUDE_PATH
find_path(GIT2_INCLUDE_PATH NAMES git2.h HINTS ${GIT2_INCLUDE_DIR})
# GIT2_LIBRARY
find_library(GIT2_LIBRARY NAMES git2 HINTS ${GIT2_LIBRARY_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libgit2 REQUIRED_VARS GIT2_LIBRARY GIT2_INCLUDE_PATH)

if (libgit2_FOUND)
    set(GIT2_INCLUDE_DIR  ${GIT2_INCLUDE_PATH})
    set(GIT2_INCLUDE_DIRS ${GIT2_INCLUDE_PATH})
    set(GIT2_LIBRARIES    ${GIT2_LIBRARY})
endif()

mark_as_advanced(GIT2_INCLUDE_PATH GIT2_LIBRARY)
