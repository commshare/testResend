SET(NACLSRC tweetnacl.c)

ADD_LIBRARY(tweetnacl OBJECT ${NACLSRC})
SET_TARGET_PROPERTIES(tweetnacl PROPERTIES COMPILE_FLAGS "-fPIC")