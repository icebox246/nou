export class Runner {
    constructor() {
        this.wasm = null;
        this.output = [];
        this.env = {
            log_int: (x) => {
                this.output.push(x);
            },
            log_bool: (b) => {
                this.output.push(b ? "true" : "false");
            },
            get_int: (def) => {
                return Number.parseInt(prompt("NoU asks for a number", def));
            }
        };
    }

    /** @type {(bytes: Uint8Array)} **/
    async init(bytes) {
        this.wasm = await WebAssembly.instantiate(bytes, { env: this.env });
    }

    run() {
        const run = this.wasm.instance.exports.run;
        if (run) {
            try {
                run();
            } catch (e) {
                this.output.push("ERROR: " + e.toString());
            }
        } else {
            this.output.push("ERROR: missing 'run := fn;'");
        }
    }

    getOutput() {
        return this.output.join("\n") + "\n";
    }
}
