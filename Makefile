CC = clang
WASI_CC = ${WASI_SDK_DIR}/bin/clang

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
	${CC} ${SOURCES} -o $@ -ggdb

demo: 
	${WASI_CC} ${SOURCES} -o demo/public/u.wasm

test: u
	cd tests && ../u ${UFLAGS} test.u && node test.mjs 
