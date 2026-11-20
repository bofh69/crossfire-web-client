<script lang="ts">
  /**
   * ClientTour — help balloons that point at the parts of the game screen,
   * one at a time, with the rest of the screen dimmed.  Shown while
   * `clientTour.active` is true.
   */
  import { onMount, tick } from "svelte";
  import {
    CLIENT_TOUR_STEPS,
    clientTour,
    type ClientTourStep,
  } from "../lib/client_tour.svelte";

  /** Where the balloon is, relative to the element it points at. */
  type Side = "bottom" | "center" | "inside" | "left" | "right" | "top";

  /** Space between the highlighted element and the balloon, in pixels. */
  const GAP = 14;
  /** Space around the highlighted element, in pixels. */
  const HIGHLIGHT_PAD = 4;
  /** Smallest space between the balloon and the window edge, in pixels. */
  const MARGIN = 8;
  /** Closest the arrow may come to a corner of the balloon, in pixels. */
  const ARROW_INSET = 18;

  /** The steps whose element is on screen. */
  let steps = $state<ClientTourStep[]>([]);
  /** Index in `steps` of the balloon shown. */
  let index = $state(0);
  /** The highlighted element's box, or null for a step without one. */
  let highlight = $state<DOMRect | null>(null);
  /** The balloon's position, side and arrow offset along that side. */
  let placement = $state<{
    left: number;
    top: number;
    side: Side;
    arrow: number;
  }>({ left: 0, top: 0, side: "center", arrow: 0 });

  let balloonEl = $state<HTMLDivElement | undefined>();
  let nextButton = $state<HTMLButtonElement | undefined>();

  const step = $derived(steps[index]);
  const isLast = $derived(index === steps.length - 1);

  /** The visible element matching `selector`, or null. */
  function findShown(selector: string): HTMLElement | null {
    const el = document.querySelector<HTMLElement>(selector);
    if (!el) return null;
    const rect = el.getBoundingClientRect();
    return rect.width > 0 && rect.height > 0 ? el : null;
  }

  function clamp(value: number, min: number, max: number): number {
    return Math.max(min, Math.min(max, value));
  }

  /** Move the highlight and the balloon to the current step's element. */
  function layout() {
    if (!step || !balloonEl) return;
    const el = step.target ? findShown(step.target) : null;
    const vw = window.innerWidth;
    const vh = window.innerHeight;
    const bw = balloonEl.offsetWidth;
    const bh = balloonEl.offsetHeight;
    if (!el) {
      highlight = null;
      placement = {
        left: (vw - bw) / 2,
        top: (vh - bh) / 2,
        side: "center",
        arrow: 0,
      };
      return;
    }

    const r = el.getBoundingClientRect();
    highlight = r;
    const room: Record<"bottom" | "left" | "right" | "top", number> = {
      bottom: vh - r.bottom - bh,
      left: r.left - bw,
      right: vw - r.right - bw,
      top: r.top - bh,
    };
    // Use the side with the most room, if the balloon fits there.
    const best = (Object.keys(room) as (keyof typeof room)[]).reduce((a, b) =>
      room[b] > room[a] ? b : a,
    );
    const cx = r.left + r.width / 2;
    const cy = r.top + r.height / 2;
    if (room[best] < GAP + MARGIN) {
      // The element fills most of the window: put the balloon inside it.
      placement = {
        left: clamp(cx - bw / 2, MARGIN, vw - bw - MARGIN),
        top: clamp(r.bottom - bh - 3 * GAP, MARGIN, vh - bh - MARGIN),
        side: "inside",
        arrow: 0,
      };
    } else if (best === "bottom" || best === "top") {
      const left = clamp(cx - bw / 2, MARGIN, vw - bw - MARGIN);
      placement = {
        left,
        top: best === "bottom" ? r.bottom + GAP : r.top - bh - GAP,
        side: best,
        arrow: clamp(cx - left, ARROW_INSET, bw - ARROW_INSET),
      };
    } else {
      const top = clamp(cy - bh / 2, MARGIN, vh - bh - MARGIN);
      placement = {
        left: best === "right" ? r.right + GAP : r.left - bw - GAP,
        top,
        side: best,
        arrow: clamp(cy - top, ARROW_INSET, bh - ARROW_INSET),
      };
    }
  }

  /** Show the step at `next`, or end the tour after the last one. */
  async function go(next: number) {
    if (next >= steps.length) {
      clientTour.stop();
      return;
    }
    index = clamp(next, 0, steps.length - 1);
    await tick();
    layout();
    nextButton?.focus();
  }

  function handleKeyDown(e: KeyboardEvent) {
    if (e.key === "Escape") {
      clientTour.stop();
    } else if (e.key === "ArrowRight") {
      void go(index + 1);
    } else if (e.key === "ArrowLeft") {
      void go(index - 1);
    } else {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
  }

  onMount(() => {
    // The game screen has just been laid out; wait for it to settle.
    const frame = requestAnimationFrame(() => {
      steps = CLIENT_TOUR_STEPS.filter(
        (s) => !s.target || findShown(s.target) !== null,
      );
      void go(0);
    });
    return () => cancelAnimationFrame(frame);
  });
</script>

<svelte:window onkeydown={handleKeyDown} onresize={layout} />

{#if step}
  <div class="tour-blocker" aria-hidden="true"></div>
  {#if highlight}
    <div
      class="tour-highlight"
      style:left="{highlight.left - HIGHLIGHT_PAD}px"
      style:top="{highlight.top - HIGHLIGHT_PAD}px"
      style:width="{highlight.width + 2 * HIGHLIGHT_PAD}px"
      style:height="{highlight.height + 2 * HIGHLIGHT_PAD}px"
    ></div>
  {:else}
    <div class="tour-dim"></div>
  {/if}
  <div
    class="tour-balloon side-{placement.side}"
    bind:this={balloonEl}
    style:left="{placement.left}px"
    style:top="{placement.top}px"
    style:--arrow="{placement.arrow}px"
    role="dialog"
    aria-modal="true"
    aria-labelledby="tour-title"
  >
    <p class="tour-title" id="tour-title">{step.title}</p>
    {#each step.text as paragraph}
      <p>{paragraph}</p>
    {/each}
    <div class="tour-footer">
      <span class="tour-dots" aria-label="Step {index + 1} of {steps.length}">
        {#each steps as _, i}
          <span class="dot" class:current={i === index}></span>
        {/each}
      </span>
      {#if !isLast}
        <button class="tour-skip" onclick={() => clientTour.stop()}>
          Skip tour
        </button>
      {/if}
      {#if index > 0}
        <button onclick={() => go(index - 1)}>Back</button>
      {/if}
      <button
        class="tour-next"
        bind:this={nextButton}
        onclick={() => go(index + 1)}
      >
        {isLast ? "Start playing" : "Next"}
      </button>
    </div>
  </div>
{/if}

<style>
  .tour-blocker {
    position: fixed;
    inset: 0;
    z-index: 300;
  }

  .tour-dim {
    position: fixed;
    inset: 0;
    z-index: 301;
    background: rgba(0, 0, 0, 0.55);
    pointer-events: none;
  }

  .tour-highlight {
    position: fixed;
    z-index: 301;
    border: 2px solid var(--text-warm-dim);
    border-radius: 6px;
    box-shadow: 0 0 0 200vmax rgba(0, 0, 0, 0.55);
    pointer-events: none;
    transition:
      left 0.25s ease,
      top 0.25s ease,
      width 0.25s ease,
      height 0.25s ease;
  }

  .tour-balloon {
    position: fixed;
    z-index: 302;
    width: clamp(260px, 26rem, calc(100vw - 16px));
    padding: 0.9rem 1.1rem 0.8rem;
    border: 1px solid var(--accent);
    border-radius: 8px;
    background: linear-gradient(var(--bg-warm), #3a2d20);
    box-shadow: 0 6px 24px rgba(0, 0, 0, 0.7);
    color: var(--text-warm);
    font-size: max(0.9rem, 13px);
    transition:
      left 0.25s ease,
      top 0.25s ease;
  }

  .tour-balloon p {
    margin: 0 0 0.6em;
    line-height: 1.45;
  }

  .tour-balloon .tour-title {
    font-size: 1.2em;
    font-weight: bold;
    color: #f0e0c0;
  }

  /* The arrow: a rotated square on the side facing the element. */
  .tour-balloon::before {
    content: "";
    position: absolute;
    width: 14px;
    height: 14px;
    background: var(--bg-warm);
    border: 1px solid var(--accent);
    transform: rotate(45deg);
  }

  .side-center::before,
  .side-inside::before {
    display: none;
  }

  .side-bottom::before {
    top: -8px;
    left: calc(var(--arrow) - 7px);
    border-right: none;
    border-bottom: none;
  }

  .side-top::before {
    bottom: -8px;
    left: calc(var(--arrow) - 7px);
    background: #3a2d20;
    border-left: none;
    border-top: none;
  }

  .side-right::before {
    left: -8px;
    top: calc(var(--arrow) - 7px);
    border-right: none;
    border-top: none;
  }

  .side-left::before {
    right: -8px;
    top: calc(var(--arrow) - 7px);
    border-left: none;
    border-bottom: none;
  }

  .tour-footer {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-top: 0.8rem;
  }

  .tour-dots {
    display: flex;
    gap: 4px;
    margin-right: auto;
  }

  .dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--text-warm-dim);
    opacity: 0.35;
  }

  .dot.current {
    opacity: 1;
  }

  .tour-footer button {
    padding: 0.35em 0.9em;
    border: 1px solid var(--accent);
    border-radius: 4px;
    background: var(--bg-darker);
    color: var(--text-warm);
    font: inherit;
    white-space: nowrap;
    cursor: pointer;
  }

  .tour-footer button:hover {
    background: var(--bg-warm-hover);
  }

  .tour-footer .tour-next {
    background: var(--accent);
    color: #fff8e8;
    font-weight: bold;
  }

  .tour-footer .tour-skip {
    border-color: transparent;
    background: transparent;
    color: var(--text-warm-dim);
  }
</style>
