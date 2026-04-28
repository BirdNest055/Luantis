# FindOpus.cmake — Locate the Opus audio codec library
#
# This module defines:
#  OPUS_FOUND        — System has the Opus library
#  OPUS_INCLUDE_DIRS — The Opus include directory
#  OPUS_LIBRARIES    — The libraries needed to use Opus
#
# Hints:
#  OPUS_ROOT         — Root directory of the Opus installation

find_path(OPUS_INCLUDE_DIR
	NAMES opus/opus.h
	HINTS ${OPUS_ROOT}
	PATH_SUFFIXES include
)

find_library(OPUS_LIBRARY
	NAMES opus
	HINTS ${OPUS_ROOT}
	PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus
	DEFAULT_MSG
	OPUS_LIBRARY
	OPUS_INCLUDE_DIR
)

if(OPUS_FOUND)
	set(OPUS_LIBRARIES ${OPUS_LIBRARY})
	set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDE_DIR})
	mark_as_advanced(OPUS_INCLUDE_DIR OPUS_LIBRARY)
endif()
