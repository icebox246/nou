export class Slice {
    /** @type{(len: number, ptr: number)} */
    constructor(len, ptr) {
        this.len = len;
        this.ptr = ptr;
    }
}
export class Runner {
    constructor() {
        this.wasm = null;
        this.output = [];
        this.env = {
            log: (u8_slice) => {
                const text = this.decodeStringFromU8Slice(
                    this.decodeSliceFromI64(u8_slice)
                );
                this.output.push(text);
            },
            ask: (u8_slice) => {
                const slice = this.decodeSliceFromI64(u8_slice);
                const bytes = this.decodeBytesFromU8Slice(slice);

                const text = prompt("U asks for input:");

                const encoder = new TextEncoder();
                const encoded = encoder.encode(text);
                bytes.set(encoded, 0);

                const out = new Slice(encoded.byteLength, slice.ptr);
                const out_encoded = this.encodeI64FromSlice(out);
                return out_encoded;
            }
        };
    }

    /** @type {(n: BigInt)} */
    decodeSliceFromI64(n) {
        const mask = (1n << 32n) - 1n;
        return new Slice(
            Number((n >> 32n) & mask),
            Number(n & mask),
        );
    }

    /** @type {(slice: Slice)} */
    encodeI64FromSlice(slice) {
        const mask = (1n << 32n) - 1n;
        return ((BigInt(slice.len) & mask) << 32n) | (BigInt(slice.ptr) & mask);
    }

    /** @type {(slice: Slice)} */
    decodeBytesFromU8Slice(slice) {
        const bytes = new Uint8Array(
            this.getUMemory().buffer,
            slice.ptr,
            slice.len
        );
        return bytes;
    }

    /** @type {(slice: Slice)} */
    decodeStringFromU8Slice(slice) {
        const bytes = this.decodeBytesFromU8Slice(slice);
        const decoder = new TextDecoder();
        return decoder.decode(bytes);
    }

    /** @returns {WebAssembly.Memory} */
    getUMemory() {
        return this.wasm.instance.exports.u_memory;
    }

    /** @type {(bytes: Uint8Array)} */
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
