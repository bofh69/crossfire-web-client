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
    data-ui-nav-entry="menubar"
    data-ui-nav-group="menubar"
    data-ui-nav-group-policy="horizontal"
    data-ui-nav-id="ui-menu-connection"
    data-ui-nav-panel="menubar"
    onclick={onToggle}
    oncontextmenu={(e) => {
      e.preventDefault();
      onToggle();
    }}>Connection</button
  >
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        data-ui-nav-id="ui-menu-connection-disconnect"
        data-ui-nav-panel="menubar"
        onclick={handleDisconnect}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleDisconnect();
        }}>Disconnect</button
      >
      <button
        data-ui-nav-id="ui-menu-connection-register"
        data-ui-nav-panel="menubar"
        onclick={registerWebCrossfireHandler}
        oncontextmenu={(e) => {
          e.preventDefault();
          registerWebCrossfireHandler();
        }}>Register as web+crossfire-URL handler</button
      >
      {#if hasWebSocketRecording}
        <button
          data-ui-nav-id="ui-menu-connection-marker"
          data-ui-nav-panel="menubar"
          onclick={addRecordingMarker}
          oncontextmenu={(e) => {
            e.preventDefault();
            addRecordingMarker();
          }}>Add recording marker</button
        >
        <button
          data-ui-nav-id="ui-menu-connection-download"
          data-ui-nav-panel="menubar"
          onclick={downloadRecording}
          oncontextmenu={(e) => {
            e.preventDefault();
            downloadRecording();
          }}>Download recording log</button
        >
        <button
          data-ui-nav-id="ui-menu-connection-stop-recording"
          data-ui-nav-panel="menubar"
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
