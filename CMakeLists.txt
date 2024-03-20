CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT( DEPTESTOMATIC C CXX )
SET( PACKAGE "libzypp-testsuite-tools" )

# VERSION
INCLUDE( ${DEPTESTOMATIC_SOURCE_DIR}/VERSION.cmake )
SET( VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
IF ( ZYPP_SOURCE )

  IF ( NOT ZYPP_BUILDDIR )
    message ( FATAL_ERROR "Define ZYPP_BUILDDIR to specify the binary location" )
  ENDIF()

  #we force the cache entries to make sure FindZypp.cmake does not reset them or they are not set already by a previous build
  SET( ZYPP_INCLUDE_DIR "${ZYPP_SOURCE}" CACHE STRING "" FORCE )
  SET( ZYPP_LIBRARY "${ZYPP_BUILDDIR}/zypp/libzypp.so" CACHE STRING "" FORCE )

  SET( CMAKE_MODULE_PATH
    ${ZYPP_SOURCE}/cmake/modules/
    ${DEPTESTOMATIC_SOURCE_DIR}/cmake/modules
    ${CMAKE_MODULE_PATH}
  )
ELSE()
  SET( CMAKE_MODULE_PATH
    ${ZYPP_PREFIX}/share/cmake/Modules
    ${DEPTESTOMATIC_SOURCE_DIR}/cmake/modules
    ${CMAKE_MODULE_PATH}
  )
ENDIF()

FIND_PACKAGE( Zypp REQUIRED )

INCLUDE(ZyppCommon)
GENERATE_PACKAGING(${PACKAGE} ${VERSION})

SET( CMAKE_CXX_STANDARD ${ZYPPCOMMON_CXX_STANDARD} )

SET( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -O3" )
SET( CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   -s -O3" )

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -g -Wall -Woverloaded-virtual -Wnon-virtual-dtor -fstack-protector -fvisibility-inlines-hidden" )
SET( CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fpermissive -g -Wall -fstack-protector" )

INCLUDE_DIRECTORIES(
  ${LIBXML2_INCLUDE_DIRS}
  ${ZYPP_INCLUDE_DIR}
  ${DEPTESTOMATIC_SOURCE_DIR}/src
)

SET( HEADERS
  src/KeyRingCallbacks.h
)

SET( SOURCES
  src/deptestomatic.cc
)

ADD_EXECUTABLE( deptestomatic ${HEADERS} ${SOURCES}  )

TARGET_LINK_LIBRARIES( deptestomatic solv ${ZYPP_LIBRARY} )

INSTALL(
  TARGETS deptestomatic
  RUNTIME DESTINATION ${INSTALL_PREFIX}/lib/zypp/testsuite/bin
)
