import { readFileSync } from 'fs';

const module = await WebAssembly.instantiate(readFileSync('a.out'), {
    env: {
        log_int(n) {
            console.log(n);
            return n;
        }
    }
});

const {
    u_memory,

    add,
    add2,
    big,
    echo,
    nop,
    variables,
    expressions,
    expressions2,
    expressions3,
    functions,
    external_functions,
    boolean_values,
    boolean_values2,
    if_statement,
    if_else_statement,
    u8_values,
    u32_values,
    u8_slices,
    slice_indexing,
} = module.instance.exports;

/** @type {(n: BigInt)} **/
function decodeSliceFromI64(n) {
    const mask = (1n << 32n) - 1n;
    return {
        len: Number((n >> 32n) & mask),
        ptr: Number(n & mask),
    };
}

function decodeStringFromU8Slice(slice) {
    const bytes = new Uint8Array(u_memory.buffer, slice.ptr, slice.len);
    const decoder = new TextDecoder();
    return decoder.decode(bytes);
}

function runTests(tests) {
    for (const [key, value] of Object.entries(tests)) {
        console.log(`Running "${key}"...`);
        const res = value.expr();
        if (res === value.expected) {
            console.log(`Passed!`);
        } else {
            console.log("Failed: expected: ", value.expected, "; got: ", res);
        }
    }
}

runTests({
    add: {
        expr: () => add(1, 2),
        expected: 3,
    },
    add2: {
        expr: () => add2(4),
        expected: 6,
    },
    echo: {
        expr: () => echo(987654321),
        expected: 987654321,
    },
    big: {
        expr: () => big(),
        expected: 123456789,
    },
    nop: {
        expr: () => nop(),
        expected: undefined,
    },
    variables: {
        expr: () => variables(),
        expected: 42,
    },
    expressions: {
        expr: () => expressions(),
        expected: 42,
    },
    expressions2: {
        expr: () => expressions2(),
        expected: 42,
    },
    expressions3: {
        expr: () => expressions3(),
        expected: 42,
    },
    functions: {
        expr: () => functions(),
        expected: 41,
    },
    external_functions: {
        expr: () => external_functions(),
        expected: 42,
    },
    boolean_values: {
        expr: () => boolean_values(42),
        expected: 1,
    },
    boolean_values2: {
        expr: () => boolean_values2(36),
        expected: 1,
    },
    if_statement_positive: {
        expr: () => if_statement(20),
        expected: 2,
    },
    if_statement_negative: {
        expr: () => if_statement(21),
        expected: 1,
    },
    if_else_statement_positive: {
        expr: () => if_else_statement(20),
        expected: 1,
    },
    if_else_statement_negative: {
        expr: () => if_else_statement(21),
        expected: 1,
    },
    u8_values: {
        expr: () => u8_values(),
        expected: 42,
    },
    u32_values: {
        expr: () => u32_values(),
        expected: 42,
    },
    u8_slices: {
        expr: () => decodeStringFromU8Slice(decodeSliceFromI64(u8_slices())),
        expected: "Hello, World!",
    },
    slice_indexing: {
        expr: () => slice_indexing(),
        expected: 42,
    }
});
