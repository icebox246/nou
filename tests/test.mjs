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
} = module.instance.exports;

function runTests(tests) {
    for (const [key, value] of Object.entries(tests)) {
        console.log(`Running "${key}"...`);
        const res = value.expr();
        if (res === value.expected) {
            console.log(`Passed!`);
        } else {
            console.log(`Failed: expected "${value.expected}", got "${res}"`);
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
});
