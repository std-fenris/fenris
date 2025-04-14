# - Find Crypto++ library
# This module tries to find the Crypto++ library
# Once done this will define
#
#  CRYPTOPP_FOUND - system has Crypto++
#  CRYPTOPP_INCLUDE_DIRS - Crypto++ include directories
#  CRYPTOPP_LIBRARIES - Libraries needed to use Crypto++
#
# The user can set CRYPTOPP_ROOT to the preferred installation prefix

# Find the include directory
find_path(CRYPTOPP_INCLUDE_DIR
    NAMES cryptopp/cryptlib.h
    PATHS
        ${CRYPTOPP_ROOT}/include
        /usr/include
        /usr/local/include
        /opt/local/include
    DOC "Crypto++ include directory"
)

# Find the library
find_library(CRYPTOPP_LIBRARY
    NAMES cryptopp
    PATHS
        ${CRYPTOPP_ROOT}/lib
        /usr/lib
        /usr/local/lib
        /opt/local/lib
    DOC "Crypto++ library"
)

# Handle the QUIETLY and REQUIRED arguments and set CRYPTOPP_FOUND to TRUE
# if all listed variables are found
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CryptoPP
    DEFAULT_MSG
    CRYPTOPP_LIBRARY
    CRYPTOPP_INCLUDE_DIR
)

# Set output variables
if(CRYPTOPP_FOUND)
    set(CRYPTOPP_LIBRARIES ${CRYPTOPP_LIBRARY})
    set(CRYPTOPP_INCLUDE_DIRS ${CRYPTOPP_INCLUDE_DIR})
else()
    set(CRYPTOPP_LIBRARIES)
    set(CRYPTOPP_INCLUDE_DIRS)
endif()

mark_as_advanced(CRYPTOPP_INCLUDE_DIR CRYPTOPP_LIBRARY)
