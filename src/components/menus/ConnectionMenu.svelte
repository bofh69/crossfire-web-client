<script lang="ts">
  import { disconnectAndReload } from "../../lib/disconnect";
  import {
    addWebSocketRecordingMarker,
    downloadWebSocketRecordingLog,
    isWebSocketRecordingActive,
    isWebSocketRecordingRequested,
    stopWebSocketRecording,
  } from "../../lib/websocket-recording";

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();
  const hasWebSocketRecording = isWebSocketRecordingRequested();

  function handleDisconnect() {
    disconnectAndReload();
  }

  function registerWebCrossfireHandler() {
    try {
      navigator.registerProtocolHandler("web+crossfire", "/?server=%s");
    } catch (e) {
      console.warn("registerProtocolHandler failed:", e);
    }
    onClose();
  }

  function addRecordingMarker() {
    if (!isWebSocketRecordingActive()) {
      alert("Cannot add marker: recording is not active.");
      onClose();
      return;
    }
    const marker = window.prompt("Add recording marker:");
    if (marker === null) return;
    if (!addWebSocketRecordingMarker(marker)) {
      alert("Marker cannot be empty or contain only whitespace.");
      return;
    }
    onClose();
  }

  function stopRecording() {
    if (!stopWebSocketRecording()) {
      alert("Recording is already stopped.");
    }
    onClose();
  }

  function downloadRecording() {
    if (!downloadWebSocketRecordingLog()) {
      alert("No recorded websocket traffic to download yet.");
    }
    onClose();
  }
</script>

<div class="menu-item">
  <button
    class="menu-button"
    onclick={onToggle}
    oncontextmenu={(e) => {
      e.preventDefault();
      onToggle();
    }}>Connection</button
  >
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        onclick={handleDisconnect}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleDisconnect();
        }}>Disconnect</button
      >
      <button
        onclick={registerWebCrossfireHandler}
        oncontextmenu={(e) => {
          e.preventDefault();
          registerWebCrossfireHandler();
        }}>Register as web+crossfire-URL handler</button
      >
      {#if hasWebSocketRecording}
        <button
          onclick={addRecordingMarker}
          oncontextmenu={(e) => {
            e.preventDefault();
            addRecordingMarker();
          }}>Add recording marker</button
        >
        <button
          onclick={downloadRecording}
          oncontextmenu={(e) => {
            e.preventDefault();
            downloadRecording();
          }}>Download recording log</button
        >
        <button
          onclick={stopRecording}
          oncontextmenu={(e) => {
            e.preventDefault();
            stopRecording();
          }}>Stop recording</button
        >
      {/if}
    </div>
  {/if}
</div>
