CC = clang

SOURCES += src/u.c
SOURCES += src/lex.c
SOURCES += src/parse.c
SOURCES += src/mod_vis.c
SOURCES += src/codegen.c

HEADERS += src/lex.h
HEADERS += src/parse.h
HEADERS += src/mod_vis.h
HEADERS += src/codegen.h
HEADERS += src/da.h

u: ${SOURCES} ${HEADERS}
	${CC} ${SOURCES} -o $@

test: u
	cd tests && ../u test.u && node test.mjs 
