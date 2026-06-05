/**
 * WebSocket traffic recording for reproducible client test-case capture.
 *
 * Recording is enabled only when the URL contains the `record` query parameter.
 * Each entry is emitted as one compact text line:
 *
 *   <elapsed_ms>\t<TX|RX>\t<byte_length>\t<base64_payload>
 *   <elapsed_ms>\t<MARK>\t<json_marker_text>
 *   <elapsed_ms>\t<KEY|UI>\t<json_payload>
 */

const recordingRequested =
  typeof window !== "undefined" &&
  new URLSearchParams(window?.location?.search ?? "").has("record");

let recordingActive = recordingRequested;
let recordingStartMs = Date.now();
const recordedLines: string[] = [];

/**
 * Encode binary payload data to base64 while chunking to avoid call-stack limits.
 */
function base64Encode(bytes: Uint8Array): string {
  // Keep chunks below argument-spread engine limits when passing to
  // String.fromCharCode(...chunk) in browsers.
  const base64ChunkSize = 0x8000;
  const chunks: string[] = [];
  for (let i = 0; i < bytes.length; i += base64ChunkSize) {
    const chunk = bytes.subarray(i, i + base64ChunkSize);
    chunks.push(String.fromCharCode(...chunk));
  }
  return btoa(chunks.join(""));
}

/**
 * Return elapsed milliseconds since recording started.
 */
function elapsedMs(): number {
  return Math.max(0, Date.now() - recordingStartMs);
}

/**
 * Append one timestamped TX/RX entry when recording is active.
 */
function appendTrafficLine(direction: "RX" | "TX", payload: Uint8Array): void {
  if (!recordingActive) return;
  recordedLines.push(
    `${elapsedMs()}\t${direction}\t${payload.length}\t${base64Encode(payload)}`,
  );
}

/**
 * Build the timestamped filename used for downloaded recording logs.
 */
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
 * Record a user key input event.
 *
 * key:     KeyboardEvent.key value (or normalised keysym, e.g. "ArrowUp", "Shift", "a")
 * event:   "press" for regular key presses, "down" for modifier-key holds,
 *          "up" for modifier-key releases
 * command: the command binding the key resulted in (empty string if none)
 */
export function recordKeyInput(
  key: string,
  event: "down" | "press" | "up",
  command: string,
): void {
  if (!recordingActive) return;
  const data: Record<string, string> = { key, event };
  if (command) data.command = command;
  recordedLines.push(`${elapsedMs()}\tKEY\t${JSON.stringify(data)}`);
}

/**
 * Record the current client/character configuration as a snapshot in the log.
 * When replaying, this snapshot can be restored to reproduce the same config.
 */
export function recordConfigSnapshot(
  backup: import("./storage").ConfigBackupV1,
): void {
  if (!recordingActive) return;
  recordedLines.push(
    `${elapsedMs()}\tUI\t${JSON.stringify({ action: "configSnapshot", backup })}`,
  );
}

/**
 * Record one UI interaction event used by replay automation.
 */
export function recordUiInteraction(
  action: string,
  data: Record<string, unknown> = {},
): void {
  if (!recordingActive) return;
  const payload = { action, ...data };
  recordedLines.push(`${elapsedMs()}\tUI\t${JSON.stringify(payload)}`);
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
