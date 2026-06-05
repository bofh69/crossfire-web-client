#!/usr/bin/env node

import fs from "node:fs/promises";
import path from "node:path";

/**
 * Print command usage help to stderr.
 */
function usage() {
  const script = path.basename(
    process.argv[1] ?? "convert-ws-recording-log.mjs",
  );
  console.error(
    `Usage: node scripts/${script} <input.log> [output.txt]\n` +
      "Converts websocket recording logs to: timestamp TX/RX/MARK/KEY/UI text",
  );
}

/**
 * Convert bytes to a C-string-safe representation.
 *
 * Printable ASCII is kept as-is, newline becomes `\n`, and other bytes become
 * `\xXX`. Backslash and double-quote are escaped.
 *
 * @param {Uint8Array | Buffer} bytes
 * @returns {string}
 */
function toCString(bytes) {
  let out = "";
  for (const byte of bytes) {
    if (byte === 0x0a) {
      out += "\\n";
      continue;
    }
    if (byte === 0x5c) {
      out += "\\\\";
      continue;
    }
    if (byte === 0x22) {
      out += '\\"';
      continue;
    }
    if (byte >= 0x20 && byte <= 0x7e) {
      out += String.fromCharCode(byte);
      continue;
    }
    out += `\\x${byte.toString(16).toUpperCase().padStart(2, "0")}`;
  }
  return out;
}

/**
 * Parse one compact recording-log line and convert it to the target output
 * format, or return null for blank lines.
 *
 * @param {string} line
 * @param {number} lineNumber
 * @returns {string | null}
 */
function parseLine(line, lineNumber) {
  if (!line.trim()) return null;
  const firstTab = line.indexOf("\t");
  const secondTab = line.indexOf("\t", firstTab + 1);
  if (firstTab <= 0 || secondTab <= firstTab) {
    throw new Error(
      `Line ${lineNumber}: malformed line (expected tab-separated)`,
    );
  }

  const timestamp = line.slice(0, firstTab);
  const direction = line.slice(firstTab + 1, secondTab);
  const rest = line.slice(secondTab + 1);

  if (direction === "MARK" || direction === "KEY" || direction === "UI") {
    let payloadData;
    try {
      payloadData = JSON.parse(rest);
    } catch {
      throw new Error(`Line ${lineNumber}: invalid ${direction} JSON payload`);
    }
    if (direction === "MARK" && typeof payloadData !== "string") {
      throw new Error(`Line ${lineNumber}: MARK payload must be a JSON string`);
    }
    if (
      (direction === "KEY" || direction === "UI") &&
      (!payloadData || typeof payloadData !== "object")
    ) {
      throw new Error(
        `Line ${lineNumber}: ${direction} payload must be a JSON object`,
      );
    }
    const text =
      direction === "MARK" ? payloadData : JSON.stringify(payloadData ?? {});
    return `${timestamp} ${direction} ${toCString(new TextEncoder().encode(text))}`;
  }

  if (direction !== "RX" && direction !== "TX") {
    throw new Error(`Line ${lineNumber}: unsupported direction "${direction}"`);
  }

  const thirdTab = rest.indexOf("\t");
  if (thirdTab <= 0) {
    throw new Error(`Line ${lineNumber}: malformed TX/RX line`);
  }
  const byteLengthStr = rest.slice(0, thirdTab);
  const base64Payload = rest.slice(thirdTab + 1);
  if (!/^\d+$/.test(byteLengthStr)) {
    throw new Error(
      `Line ${lineNumber}: invalid byte length "${byteLengthStr}"`,
    );
  }
  const expectedLength = Number(byteLengthStr);
  if (!Number.isSafeInteger(expectedLength) || expectedLength < 0) {
    throw new Error(
      `Line ${lineNumber}: invalid byte length "${byteLengthStr}"`,
    );
  }
  let payloadBytes;
  try {
    payloadBytes = Buffer.from(base64Payload, "base64");
  } catch {
    throw new Error(`Line ${lineNumber}: invalid base64 payload`);
  }
  if (payloadBytes.length !== expectedLength) {
    throw new Error(
      `Line ${lineNumber}: byte length mismatch (expected ${expectedLength}, got ${payloadBytes.length})`,
    );
  }
  return `${timestamp} ${direction} ${toCString(payloadBytes)}`;
}

/**
 * CLI entry point: read the input log, convert all lines, and write to either
 * stdout or an optional output file.
 */
async function main() {
  const [, , inputPath, outputPath] = process.argv;
  if (!inputPath) {
    usage();
    process.exitCode = 1;
    return;
  }

  const input = await fs.readFile(inputPath, "utf8");
  const lines = input.split(/\r?\n/);
  const outputLines = [];
  for (let i = 0; i < lines.length; i++) {
    const converted = parseLine(lines[i], i + 1);
    if (converted !== null) {
      outputLines.push(converted);
    }
  }
  const output = outputLines.join("\n") + (outputLines.length > 0 ? "\n" : "");

  if (outputPath) {
    await fs.writeFile(outputPath, output, "utf8");
    return;
  }
  process.stdout.write(output);
}

main().catch((error) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`Error: ${message}`);
  process.exitCode = 1;
});
