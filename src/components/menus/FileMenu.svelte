<script lang="ts">
  import { clientDisconnect } from '../../lib/client';

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
    onDisconnect: () => void;
  }
  let { fading, isOpen, onToggle, onClose, onDisconnect }: Props = $props();

  function handleDisconnect() {
    clientDisconnect();
    onDisconnect();
    onClose();
  }
</script>

<div class="menu-item">
  <button class="menu-button" onclick={onToggle} oncontextmenu={(e) => { e.preventDefault(); onToggle(); }}>File</button>
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        onclick={handleDisconnect}
        oncontextmenu={(e) => { e.preventDefault(); handleDisconnect(); }}
      >Disconnect</button>
    </div>
  {/if}
</div>
