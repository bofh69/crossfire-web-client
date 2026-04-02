<script lang="ts">
  import { clientDisconnect } from '../lib/client';

  interface Props {
    onDisconnect: () => void;
  }

  let { onDisconnect }: Props = $props();

  let activeMenu = $state<string | null>(null);

  function toggleMenu(menu: string) {
    activeMenu = activeMenu === menu ? null : menu;
  }

  function closeMenu() {
    activeMenu = null;
  }

  function handleDisconnect() {
    clientDisconnect();
    onDisconnect();
    closeMenu();
  }
</script>

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="menu-bar" onclick={(e: MouseEvent) => e.stopPropagation()}>
  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('file')}>File</button>
    {#if activeMenu === 'file'}
      <div class="dropdown">
        <button onclick={handleDisconnect}>Disconnect</button>
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('help')}>Help</button>
    {#if activeMenu === 'help'}
      <div class="dropdown">
        <button onclick={closeMenu}>About Crossfire Web Client</button>
      </div>
    {/if}
  </div>

  <div class="spacer"></div>
  <span class="title">Crossfire</span>
</div>

<!-- Global click handler to close menus -->
<svelte:window onclick={closeMenu} />

<style>
  .menu-bar {
    display: flex;
    align-items: center;
    background: #252525;
    border-bottom: 1px solid #333;
    height: 28px;
    padding: 0 0.25rem;
    font-size: 0.8rem;
    user-select: none;
  }

  .menu-item {
    position: relative;
  }

  .menu-button {
    padding: 0.2rem 0.6rem;
    border: none;
    background: none;
    color: #c0c0c0;
    cursor: pointer;
    font-size: 0.8rem;
  }

  .menu-button:hover {
    background: #333;
  }

  .dropdown {
    position: absolute;
    top: 100%;
    left: 0;
    background: #2a2a2a;
    border: 1px solid #444;
    min-width: 160px;
    z-index: 50;
    box-shadow: 0 2px 8px rgba(0,0,0,0.5);
  }

  .dropdown button {
    display: block;
    width: 100%;
    padding: 0.4rem 0.75rem;
    border: none;
    background: none;
    color: #c0c0c0;
    text-align: left;
    cursor: pointer;
    font-size: 0.8rem;
  }

  .dropdown button:hover {
    background: #3a3a3a;
  }

  .spacer {
    flex: 1;
  }

  .title {
    color: #7a6a4a;
    font-size: 0.75rem;
    padding-right: 0.5rem;
  }
</style>
