CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT( DEPTESTOMATIC C CXX )
SET( PACKAGE "deptestomatic" )

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

INCLUDE(ZyppCommon)
SET( CMAKE_CXX_STANDARD ${ZYPPCOMMON_CXX_STANDARD} )

FIND_PACKAGE( EXPAT REQUIRED )
FIND_PACKAGE( LibXml2 REQUIRED )
FIND_PACKAGE( Zypp REQUIRED )
IF( ZYPP_FOUND )
  INCLUDE_DIRECTORIES(${ZYPP_INCLUDE_DIR})
ELSE( ZYPP_FOUND )
  MESSAGE( FATAL_ERROR "libzypp not found" )
ENDIF( ZYPP_FOUND )

INCLUDE_DIRECTORIES( ${DEPTESTOMATIC_SOURCE_DIR}/src ${LIBXML2_INCLUDE_DIRS} ${EXPAT_INCLUDE_DIRS} )

SET( HEADERS 
  src/KeyRingCallbacks.h
  src/XmlNode.h
)

SET( SOURCES 
  src/deptestomatic.cc
  src/XmlNode.cc
)

ADD_EXECUTABLE( deptestomatic ${HEADERS} ${SOURCES}  )
TARGET_LINK_LIBRARIES( deptestomatic ${ZYPP_LIBRARY} ${LIBXML2_LIBRARIES} ${EXPAT_LIBRARIES} )
