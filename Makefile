CC = clang
WASI_CC = ${WASI_SDK_DIR}/bin/clang
JS = node

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

demo_wasi: ${WASI_SDK_DIR}
ifndef WASI_SDK_DIR
	@echo "You must provide WASI_SDK_DIR"
	@exit 1
else
	${WASI_CC} ${SOURCES} -o demo/public/u.wasm
endif

test: u
	./u ${UFLAGS} demo/src/example.u
	./u ${UFLAGS} tests/test.u
	${JS} tests/test.mjs
