/**
 * WebSocket traffic recording for reproducible client test-case capture.
 *
 * Recording is enabled only when the URL contains the `record` query parameter.
 * Each entry is emitted as one compact text line:
 *
 *   <elapsed_ms>\t<TX|RX>\t<byte_length>\t<base64_payload>
 *   <elapsed_ms>\t<MARK>\t<json_marker_text>
 */

const recordingRequested =
  typeof window !== "undefined" &&
  new URLSearchParams(window.location.search).has("record");

let recordingActive = recordingRequested;
let recordingStartMs = Date.now();
const recordedLines: string[] = [];

function base64Encode(bytes: Uint8Array): string {
  const CHUNK_SIZE = 0x8000;
  const chunks: string[] = [];
  for (let i = 0; i < bytes.length; i += CHUNK_SIZE) {
    const chunk = bytes.subarray(i, i + CHUNK_SIZE);
    chunks.push(String.fromCharCode(...chunk));
  }
  return btoa(chunks.join(""));
}

function elapsedMs(): number {
  return Math.max(0, Date.now() - recordingStartMs);
}

function appendTrafficLine(direction: "RX" | "TX", payload: Uint8Array): void {
  if (!recordingActive) return;
  recordedLines.push(
    `${elapsedMs()}\t${direction}\t${payload.length}\t${base64Encode(payload)}`,
  );
}

function buildDownloadFilename(): string {
  const timestamp = new Date()
    .toISOString()
    .replace(/[:.]/g, "-")
    .replace("T", "_")
    .replace("Z", "");
  return `crossfire-ws-recording-${timestamp}.log`;
}

/**
 * Return true when websocket recording controls should be shown in the UI.
 */
export function isWebSocketRecordingRequested(): boolean {
  return recordingRequested;
}

/**
 * Return true when recording is currently active.
 */
export function isWebSocketRecordingActive(): boolean {
  return recordingActive;
}

/**
 * Record one outgoing websocket payload.
 */
export function recordOutgoingWebSocketPayload(payload: Uint8Array): void {
  appendTrafficLine("TX", payload);
}

/**
 * Record one incoming websocket payload.
 */
export function recordIncomingWebSocketPayload(payload: Uint8Array): void {
  appendTrafficLine("RX", payload);
}

/**
 * Add a user-visible marker line to the recording log.
 */
export function addWebSocketRecordingMarker(text: string): boolean {
  if (!recordingActive) return false;
  const trimmed = text.trim();
  if (!trimmed) return false;
  recordedLines.push(`${elapsedMs()}\tMARK\t${JSON.stringify(trimmed)}`);
  return true;
}

/**
 * Stop websocket traffic recording.
 */
export function stopWebSocketRecording(): boolean {
  if (!recordingActive) return false;
  recordingActive = false;
  return true;
}

/**
 * Download the recorded websocket log as a plain text file.
 */
export function downloadWebSocketRecordingLog(): boolean {
  if (recordedLines.length === 0) return false;
  const content = recordedLines.join("\n") + "\n";
  const blob = new Blob([content], { type: "text/plain;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = buildDownloadFilename();
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
  return true;
}

/**
 * Reset recording memory and start recording from now.
 * Intended for internal/testing use.
 */
export function resetWebSocketRecordingForTesting(): void {
  recordedLines.length = 0;
  recordingStartMs = Date.now();
  recordingActive = recordingRequested;
}
