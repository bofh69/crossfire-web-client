<script lang="ts">
  import { gameEvents } from "../../lib/events";

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  function handleZoomIn() {
    gameEvents.emit("zoomIn");
    onClose();
  }

  function handleZoomOut() {
    gameEvents.emit("zoomOut");
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
    }}>View</button
  >
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        onclick={handleZoomIn}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleZoomIn();
        }}>Zoom In</button
      >
      <button
        onclick={handleZoomOut}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleZoomOut();
        }}>Zoom Out</button
      >
    </div>
  {/if}
</div>
