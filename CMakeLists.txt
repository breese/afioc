###############################################################################
#
# Copyright (C) 2013 Bjorn Reese <breese@users.sourceforge.net>
#
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)
#
###############################################################################

# Debug build
#   cmake -DCMAKE_BUILD_TYPE=Debug .

###############################################################################

cmake_minimum_required(VERSION 2.8)
project(afio-callback)

set(AFIOC_BUILD_DIR ${CMAKE_BINARY_DIR}/stage)
set(LIBRARY_OUTPUT_PATH ${AFIOC_BUILD_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${AFIOC_BUILD_DIR}/bin)

add_definitions(-g -Wall)
add_definitions(-Wno-unused-local-typedefs -Wno-unused-variable -Wno-unused-but-set-variable)
add_definitions(-std=c++11)
#add_definitions(-fmax-errors=1) # gcc
#add_definitions(-ferror-limit=1) # clang

###############################################################################
# Boost package
###############################################################################

if (${BOOST_ROOT})
  #set(Boost_DEBUG ON)
  set(Boost_NO_BOOST_CMAKE ON)
  set(Boost_NO_SYSTEM_PATHS ON)
endif()
find_package(Boost 1.55.0 COMPONENTS filesystem thread chrono system)
if (NOT ${Boost_FOUND})
  message(FATAL_ERROR "Boost not found (or too old)")
endif()
include_directories(BEFORE ${Boost_INCLUDE_DIR})
set(EXTRA_LIBS ${EXTRA_LIBS} ${Boost_LIBRARIES})

###############################################################################
# Afio-callback package
###############################################################################

include_directories(BEFORE .)

add_library(afioc STATIC
  libs/afio/src/file_handle.cpp
)

target_link_libraries(afioc ${EXTRA_LIBS})

###############################################################################
# Examples
###############################################################################

add_executable(example1
  libs/afio/example/example1.cpp
)
target_link_libraries(example1 afioc)

add_executable(copy_file
  libs/afio/example/copy_file.cpp
)
target_link_libraries(copy_file afioc)
