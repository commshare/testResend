PROJECT(librdns C)

SET(RDNS_VERSION_MAJOR 0)
SET(RDNS_VERSION_MINOR 1)
SET(RDNS_VERSION_PATCH 1)

IF(NOT SLAVE_BUILD)
	CMAKE_MINIMUM_REQUIRED(VERSION 2.8.8 FATAL_ERROR)
ELSE(NOT SLAVE_BUILD)
	CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0 FATAL_ERROR)
ENDIF(NOT SLAVE_BUILD)

OPTION(ENABLE_CURVE "Enable DNSCurve plugin" ON)

INCLUDE(CheckFunctionExists)
INCLUDE(CheckSymbolExists)

SET(LIBRDNSSRC			src/util.c
						src/logger.c
						src/compression.c
						src/punycode.c
						src/curve.c
						src/parse.c
						src/packet.c
						src/resolver.c)

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/src" 
					"${CMAKE_CURRENT_SOURCE_DIR}/include" 
					"${CMAKE_BINARY_DIR}/src"
					"${CMAKE_BINARY_DIR}/include"  
					"${CMAKE_CURRENT_SOURCE_DIR}/contrib/uthash"
					"${CMAKE_CURRENT_SOURCE_DIR}/contrib/libottery"
					"${CMAKE_CURRENT_SOURCE_DIR}/contrib/tweetnacl")

ADD_LIBRARY(rdns_core OBJECT ${LIBRDNSSRC})

ADD_SUBDIRECTORY(contrib/libottery)
ADD_LIBRARY(rdns STATIC $<TARGET_OBJECTS:rdns_core> $<TARGET_OBJECTS:ottery>)
