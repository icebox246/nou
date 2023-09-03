#ifndef CODEGEN_H_
#define CODEGEN_H_

#include <stddef.h>
#include <stdint.h>

#include "da.h"
#include "parse.h"

typedef struct {
    da_list(uint8_t);
} ByteBuffer;

ByteBuffer codegen_module(Module* mod);

#endif
