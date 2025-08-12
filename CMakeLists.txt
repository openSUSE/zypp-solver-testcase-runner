CMAKE_MINIMUM_REQUIRED(VERSION 3.17)
PROJECT( DEPTESTOMATIC C CXX )
SET( PACKAGE "libzypp-testsuite-tools" )

# VERSION
INCLUDE( ${DEPTESTOMATIC_SOURCE_DIR}/VERSION.cmake )
SET( VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
if ( ZYPP_STACK_BUILD )
  SET( ZYPPER_TARGET_PREFIX "zypp_solver_testcaserunner")
  SET( CMAKE_MODULE_PATH
    ${zypp-stack_SOURCE_DIR}/libzypp/cmake/modules/
    ${CMAKE_MODULE_PATH}
  )
  include_directories(${ZYPP_STACK_INCLUDE_DIRS})

  set( ZYPP_FOUND 1 )
  set( ZYPP_INCLUDE_DIR ${zypp-stack_SOURCE_DIR}/libzypp )
  set( ZYPP_LIBRARY zypp )
  set( ZYPP_TUI_LIBRARY zypp-tui )
  set( LIBRARY_DEPS libsolv ${ZYPP_LIBRARY} )

else()
  SET( CMAKE_MODULE_PATH
      ${CMAKE_INSTALL_PREFIX}/${LIB}/cmake/Zypp/
      ${CMAKE_INSTALL_PREFIX}/lib64/cmake/Zypp/
      ${CMAKE_INSTALL_PREFIX}/lib/cmake/Zypp/
      ${CMAKE_MODULE_PATH}
  )
  FIND_PACKAGE( Zypp REQUIRED )
  set( LIBRARY_DEPS solv ${ZYPP_LIBRARY} )
ENDIF()

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

TARGET_LINK_LIBRARIES( deptestomatic ${LIBRARY_DEPS} )

INSTALL(
  TARGETS deptestomatic
  RUNTIME DESTINATION ${INSTALL_PREFIX}/lib/zypp/testsuite/bin
)
