import { WASI, File, OpenFile, PreopenDirectory } from "@bjorn3/browser_wasi_shim";

export class Compiler {
    constructor() {
        this.wasi = null;
        this.module = null;
        this.stdout = null;
        this.stdin = null;
        this.stderr = null;
        this.input_file = null;
        this.output_file = null;
        this.default_args = ["u", "main.u"];
    }

    async init(moduleUrl) {
        const env = [];
        this.input_file = new File([]);
        this.output_file = new File([]);
        const fds = [
            null,
            null,
            null,
            new PreopenDirectory(".", {
                "main.u": this.input_file,
                "a.out": this.output_file,
            }),
        ];

        this.wasi = new WASI(this.default_args, env, fds);

        this.module = await WebAssembly.compile(await (await fetch(moduleUrl)).arrayBuffer());
    }

    getOutput() {
        const decoder = new TextDecoder();
        const message_stdout = decoder.decode(this.stdout.data);
        return message_stdout;
    }

    getErrors() {
        const decoder = new TextDecoder();
        const message_stderr = decoder.decode(this.stderr.data);
        return message_stderr;
    }

    resetStdio() {
        this.stdin = new File();
        this.stdout = new File();
        this.stderr = new File();
        this.wasi.fds[0] = new OpenFile(this.stdin);
        this.wasi.fds[1] = new OpenFile(this.stdout);
        this.wasi.fds[2] = new OpenFile(this.stderr);
    }

    /** @type {(source_code: string, extra_args: string[])} **/
    async compile(source_code, extra_args) {
        try {
            if (extra_args) {
                this.wasi.args = [...this.default_args, ...extra_args];
            } else {
                this.wasi.args = [...this.default_args];
            }
            const encoder = new TextEncoder();
            const encoded_source = encoder.encode(source_code);
            this.input_file.data = encoded_source;

            this.resetStdio();

            const instance = await WebAssembly.instantiate(this.module, {
                "wasi_snapshot_preview1": this.wasi.wasiImport,
            });

            console.log("Running", this.wasi.args);
            this.wasi.start(instance);

            console.info("stdout:\n" + this.getOutput());

            console.info("stderr:\n" + this.getErrors());

            return this.output_file.data;
        } catch {
            console.error("Error occured during compilation:\n", "stderr:\n", this.getErrors(), "stdout:\n", this.getOutput());
            return null;
        }
    }
};
