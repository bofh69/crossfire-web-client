<script lang="ts">
  import { disconnectAndReload } from "../../lib/disconnect";

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

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
    </div>
  {/if}
</div>
