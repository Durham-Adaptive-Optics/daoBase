export type DType =
    | 'uint8' | 'int8' | 'uint16' | 'int16' | 'uint32' | 'int32'
    | 'uint64' | 'int64' | 'float32' | 'float64' | 'complex64' | 'complex128';

export type Frame =
    | Uint8Array | Int8Array | Uint16Array | Int16Array | Uint32Array | Int32Array
    | BigUint64Array | BigInt64Array | Float32Array | Float64Array;

export class Shm {
    readonly name: string;
    /** Same order as numpy's shape in Python (rows, cols for an image). */
    readonly shape: number[];
    readonly dtype: DType;
    readonly atype: number;
    readonly depth: number;
    /** Frame counter (cnt0). */
    readonly counter: bigint;
    readonly closed: boolean;
    readonly size: number;
    /** Copy of the newest frame, flat, row-major (complex: re, im interleaved). */
    getData(): Frame;
    setData(data: Frame | ArrayLike<number> | ArrayLike<bigint>): void;
    /** true: a frame was posted; false: timed out. */
    wait(timeoutMs?: number): Promise<boolean>;
    next(timeoutMs?: number): Promise<Frame | null>;
    frames(timeoutMs?: number): AsyncGenerator<Frame>;
    close(): void;
}

export function open(name: string): Shm;
/** shape: 1 to 3 sizes, [n] for a 1-D SHM, [rows, cols] for 2-D. */
export function create(name: string, shape: number[], dtype?: DType,
                       opts?: { depth?: number; data?: Frame | ArrayLike<number> }): Shm;
export const DTYPES: Record<DType, { atype: number; array: Function; complex?: boolean }>;
