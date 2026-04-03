/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file
 * TypeScript conversion of newsocket.c using WebSockets instead of TCP
 * sockets. Provides binary packet building, reading, and WebSocket-based
 * network communication for the Crossfire protocol.
 */

import {
  MAXSOCKBUF,
  VERSION_CS,
  VERSION_SC,
  EPORT,
  COMMAND_WINDOW,
  COMMAND_MAX,
} from "./protocol";

/** Maximum payload size for a SockList (MAXSOCKBUF minus 2-byte length header). */
const MAX_DATA_SIZE = MAXSOCKBUF - 2;

/**
 * Buffer builder for constructing binary Crossfire protocol packets.
 *
 * Mirrors the C SockList struct and its SockList_Add* helpers, but backed
 * by an ArrayBuffer/DataView instead of a raw byte array.
 */
export class SockList {
  private buffer: ArrayBuffer;
  private view: DataView;
  private bytes: Uint8Array;
  private _len: number;

  constructor() {
    this.buffer = new ArrayBuffer(MAX_DATA_SIZE);
    this.view = new DataView(this.buffer);
    this.bytes = new Uint8Array(this.buffer);
    this._len = 0;
  }

  /** Current number of bytes written into the buffer. */
  get len(): number {
    return this._len;
  }

  /** Add a single unsigned byte. */
  addChar(c: number): void {
    if (this._len + 1 > MAX_DATA_SIZE) {
      console.error(
        `SockList.addChar: Could not write ${c & 0xff} to socket: Buffer full.`
      );
      return;
    }
    this.view.setUint8(this._len, c & 0xff);
    this._len += 1;
  }

  /** Add a 16-bit unsigned integer in network (big-endian) byte order. */
  addShort(s: number): void {
    if (this._len + 2 > MAX_DATA_SIZE) {
      console.error(
        `SockList.addShort: Could not write ${s & 0xffff} to socket: Buffer full.`
      );
      return;
    }
    this.view.setUint16(this._len, s & 0xffff, false);
    this._len += 2;
  }

  /** Add a 32-bit unsigned integer in network (big-endian) byte order. */
  addInt(i: number): void {
    if (this._len + 4 > MAX_DATA_SIZE) {
      console.error(
        `SockList.addInt: Could not write ${i >>> 0} to socket: Buffer full.`
      );
      return;
    }
    this.view.setUint32(this._len, i >>> 0, false);
    this._len += 4;
  }

  /** Add raw string bytes (no null terminator). */
  addString(s: string): void {
    const encoded = new TextEncoder().encode(s);
    let writeLen = encoded.length;
    if (this._len + writeLen > MAX_DATA_SIZE) {
      writeLen = MAX_DATA_SIZE - this._len;
    }
    this.bytes.set(encoded.subarray(0, writeLen), this._len);
    this._len += writeLen;
  }

  /** Return a copy of the built packet data. */
  getData(): Uint8Array {
    return new Uint8Array(this.buffer, 0, this._len);
  }
}

// ---------------------------------------------------------------------------
// Reader helpers – extract typed values from a received binary buffer.
// ---------------------------------------------------------------------------

/** Read an unsigned 8-bit integer from `data` at `offset`. */
export function getCharFromData(data: DataView, offset: number): number {
  return data.getUint8(offset);
}

/** Read an unsigned 16-bit integer (big-endian) from `data` at `offset`. */
export function getShortFromData(data: DataView, offset: number): number {
  return data.getUint16(offset, false);
}

/**
 * Read a signed 32-bit integer (big-endian) from `data` at `offset`.
 *
 * The original C implementation returns a signed int via bit-shifting,
 * so we use getInt32 to preserve that behaviour.
 */
export function getIntFromData(data: DataView, offset: number): number {
  return data.getInt32(offset, false);
}

/** Read a signed 64-bit integer (big-endian) from `data` at `offset`. */
export function getInt64FromData(data: DataView, offset: number): bigint {
  return data.getBigInt64(offset, false);
}

/** Decode a UTF-8 string of `len` bytes starting at `offset`. */
export function getStringFromData(
  data: Uint8Array,
  offset: number,
  len: number
): string {
  return new TextDecoder().decode(data.subarray(offset, offset + len));
}

// ---------------------------------------------------------------------------
// CrossfireSocket – WebSocket wrapper for the Crossfire protocol.
// ---------------------------------------------------------------------------

/**
 * Manages a WebSocket connection to a Crossfire server (or WebSocket proxy).
 *
 * The Crossfire TCP protocol frames packets with a 2-byte big-endian length
 * prefix.  Because WebSocket is already message-based, framing is handled by
 * the transport layer and we send/receive raw packet payloads.
 */
export class CrossfireSocket {
  /** Requested client→server protocol version. */
  csVersion: number = VERSION_CS;
  /** Accepted server→client protocol version. */
  scVersion: number = VERSION_SC;
  /** Number of commands sent to the server. */
  commandSent: number = 0;
  /** Number of commands received from the server. */
  commandReceived: number = 0;
  /** Human-readable server name / identifier. */
  serverName: string = "";

  /** Called when a complete packet arrives. `data` is the raw packet payload. */
  onPacket: ((data: Uint8Array) => void) | null = null;
  /** Called when the connection is closed. */
  onDisconnect: (() => void) | null = null;
  /** Called on transport errors. */
  onError: ((error: Event) => void) | null = null;

  private url: string;
  private ws: WebSocket | null = null;

  constructor(url: string) {
    this.url = url;
  }

  /**
   * Open the WebSocket connection.
   *
   * Resolves once the connection is established, or rejects on failure.
   */
  connect(): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      const ws = new WebSocket(this.url);
      ws.binaryType = "arraybuffer";

      ws.addEventListener("open", () => {
        this.ws = ws;
        console.log(`CrossfireSocket: connected to ${this.url}`);
        resolve();
      });

      ws.addEventListener("error", (ev: Event) => {
        if (this.ws === null) {
          // Connection was never established.
          reject(new Error(`CrossfireSocket: failed to connect to ${this.url}`));
        }
        console.error("CrossfireSocket: WebSocket error", ev);
        this.onError?.(ev);
      });

      ws.addEventListener("close", () => {
        console.log("CrossfireSocket: connection closed");
        this.ws = null;
        this.onDisconnect?.();
      });

      ws.addEventListener("message", (ev: MessageEvent) => {
        this.handleMessage(ev);
      });
    });
  }

  /** Close the connection if open. */
  disconnect(): void {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  /** Send a pre-built SockList packet. */
  send(sl: SockList): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn("CrossfireSocket.send: not connected");
      return;
    }
    const data = sl.getData();
    // Log the outgoing binary packet as a text preview (first 64 bytes decoded).
    const preview = new TextDecoder().decode(data.subarray(0, Math.min(64, data.length)));
    console.debug(`[TX binary ${data.length}B] ${preview}`);
    this.ws.send(data);
    this.commandSent++;
  }

  /**
   * Send a simple string command (equivalent to the C `cs_print_string`).
   *
   * The string is encoded to UTF-8 and sent as a binary WebSocket message.
   */
  sendString(cmd: string): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn("CrossfireSocket.sendString: not connected");
      return;
    }
    console.debug(`[TX] ${cmd}`);
    const encoded = new TextEncoder().encode(cmd);
    this.ws.send(encoded);
    this.commandSent++;
  }

  // -----------------------------------------------------------------------
  // Internal
  // -----------------------------------------------------------------------

  private handleMessage(ev: MessageEvent): void {
    this.commandReceived++;

    let payload: Uint8Array;
    if (ev.data instanceof ArrayBuffer) {
      payload = new Uint8Array(ev.data);
    } else {
      // Fallback: treat text frames as UTF-8 bytes.
      payload = new TextEncoder().encode(ev.data as string);
    }

    if (payload.length === 0) {
      return;
    }

    const t0 = performance.now();
    this.onPacket?.(payload);
    const elapsed = performance.now() - t0;
    if (elapsed > 5) {
      console.warn(`[perf:ws] onPacket callback took ${elapsed.toFixed(1)}ms for ${payload.length}B message (#${this.commandReceived})`);
    }
  }
}
