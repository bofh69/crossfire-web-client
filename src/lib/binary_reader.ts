/**
 * binary_reader.ts — Auto-advancing binary buffer reader for the Crossfire protocol.
 */

/**
 * Reads typed values from a DataView, automatically advancing the internal
 * position after each read.  Throws {@link RangeError} if a read would exceed
 * the declared buffer length.
 */
export class BinaryReader {
  private readonly view: DataView;
  private _pos: number;
  private readonly _len: number;

  /**
   * @param data  The DataView wrapping the packet buffer.
   * @param len   Number of valid bytes available (defaults to `data.byteLength`).
   */
  constructor(data: DataView, len?: number) {
    this.view = data;
    this._pos = 0;
    this._len = len ?? data.byteLength;
  }

  /** Current read position (bytes from the start of the DataView). */
  get pos(): number { return this._pos; }

  /** Number of bytes not yet consumed. */
  get remaining(): number { return this._len - this._pos; }

  // ── private ──────────────────────────────────────────────────────────────

  private checkRead(n: number): void {
    if (this._pos + n > this._len) {
      throw new RangeError(
        `BinaryReader: attempted to read ${n} byte(s) at offset ${this._pos}` +
        ` but only ${this._len - this._pos} byte(s) remain (buffer length ${this._len})`
      );
    }
  }

  // ── Numeric reads ─────────────────────────────────────────────────────────

  /** Read an unsigned 8-bit integer (0–255). */
  readUint8(): number {
    this.checkRead(1);
    return this.view.getUint8(this._pos++);
  }

  /** Read a signed 8-bit integer (−128–127). */
  readInt8(): number {
    this.checkRead(1);
    return this.view.getInt8(this._pos++);
  }

  /** Read a signed 16-bit big-endian integer. */
  readInt16(): number {
    this.checkRead(2);
    const v = this.view.getInt16(this._pos, false);
    this._pos += 2;
    return v;
  }

  /** Read an unsigned 16-bit big-endian integer. */
  readUint16(): number {
    this.checkRead(2);
    const v = this.view.getUint16(this._pos, false);
    this._pos += 2;
    return v;
  }

  /** Read a signed 32-bit big-endian integer. */
  readInt32(): number {
    this.checkRead(4);
    const v = this.view.getInt32(this._pos, false);
    this._pos += 4;
    return v;
  }

  /** Read an unsigned 32-bit big-endian integer. */
  readUint32(): number {
    this.checkRead(4);
    const v = this.view.getUint32(this._pos, false);
    this._pos += 4;
    return v;
  }

  /** Read a signed 64-bit big-endian integer as a BigInt. */
  readInt64(): bigint {
    this.checkRead(8);
    const v = this.view.getBigInt64(this._pos, false);
    this._pos += 8;
    return v;
  }

  // ── String reads ──────────────────────────────────────────────────────────

  /**
   * Read exactly `len` bytes and decode them as a UTF-8 string.
   */
  readString(len: number): string {
    this.checkRead(len);
    const bytes = new Uint8Array(this.view.buffer, this.view.byteOffset + this._pos, len);
    this._pos += len;
    return new TextDecoder().decode(bytes);
  }

  /**
   * Read bytes until a NUL terminator (0x00) or end of buffer, and decode as UTF-8.
   * Consumes the NUL terminator (does not include it in the returned string).
   */
  readNullTerminatedString(): string {
    const start = this._pos;
    while (this._pos < this._len && this.view.getUint8(this._pos) !== 0) {
      this._pos++;
    }
    const bytes = new Uint8Array(
      this.view.buffer,
      this.view.byteOffset + start,
      this._pos - start,
    );
    if (this._pos < this._len) this._pos++; // consume the NUL byte
    return new TextDecoder().decode(bytes);
  }

  /**
   * Read a length-prefixed string where the length is a uint16 (big-endian).
   * This is the "l2string" format used in several Crossfire protocol commands.
   */
  readL2String(): string {
    const len = this.readUint16();
    return this.readString(len);
  }

  /**
   * Skip `n` bytes without reading them.
   */
  skip(n: number): void {
    this.checkRead(n);
    this._pos += n;
  }
}
