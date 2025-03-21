
SET(ENET_SEARCH_PATHS
    /usr/local/
    /usr
    /opt
)

FIND_PATH(ENET_INCLUDE_DIR enet/enet.h
    HINTS ${ENET_ROOT}
    PATH_SUFFIXES include include/enet
    PATHS ${ENET_SEARCH_PATHS}
)
FIND_LIBRARY(ENET_LIBRARY enet
    HINTS ${ENET_ROOT}
    PATH_SUFFIXES lib64 lib bin
    PATHS ${ENET_SEARCH_PATHS}
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments
FIND_PACKAGE_HANDLE_STANDARD_ARGS(enet
    REQUIRED_VARS ENET_LIBRARY ENET_INCLUDE_DIR
)

mark_as_advanced(ENET_INCLUDE_DIR ENET_LIBRARY ENET_SEARCH_PATHS)
