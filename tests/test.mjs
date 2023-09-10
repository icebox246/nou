import { readFileSync } from 'fs';

const module = await WebAssembly.instantiate(readFileSync('a.out'), {
    env: {
        log_int(n) {
            console.log(n);
            return n;
        }
    }
});

const { add, add2, big, echo, nop, variables, expressions, expressions2, functions, external_functions } = module.instance.exports;

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
    functions: {
        expr: () => functions(),
        expected: 41,
    },
    external_functions: {
        expr: () => external_functions(),
        expected: 42,
    },
});
