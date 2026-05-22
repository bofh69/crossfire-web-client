import type { GamepadProfile } from "./gamepad_defaults";

/**
 * Shared UI navigation mode for keyboard/gamepad-only UI traversal.
 */

export type UiNavAction =
  | "activate"
  | "exit"
  | "menu"
  | "move-down"
  | "move-end"
  | "move-home"
  | "move-left"
  | "move-page-down"
  | "move-page-up"
  | "move-right"
  | "move-up"
  | "next-component"
  | "next"
  | "prev-component"
  | "prev";

export type UiNavDirectionalAction = Extract<
  UiNavAction,
  | "move-down"
  | "move-left"
  | "move-page-down"
  | "move-page-up"
  | "move-right"
  | "move-up"
>;

type UiNavGroupPolicy = "both" | "horizontal" | "vertical";

/**
 * Entry-point aliases that can be used by commands and remembered state.
 */
export const UI_NAV_TARGET_NAMES = [
  "ground",
  "hotbar",
  "info",
  "inventory",
  "knowledge",
  "menubar",
  "protections",
  "quests",
  "skills",
  "spells",
] as const;

type UiNavTargetName = (typeof UI_NAV_TARGET_NAMES)[number];

/**
 * App-provided callbacks for resolving high-level target names to concrete DOM target IDs.
 */
export interface UiNavCallbacks {
  /** Resolve a target alias such as `inventory` to a concrete DOM target ID. */
  resolveTargetId?: (target: string) => Promise<string | null> | string | null;
  /** Handle Escape while in UI-nav mode. Return true when handled. */
  onEscape?: () => boolean;
  /** Called whenever UI-nav mode exits. */
  onExit?: () => void;
  /** Called after the active target changes to a different element. */
  onTargetChanged?: (
    previousTargetId: string | null,
    nextTargetId: string | null,
  ) => void;
  /**
   * Called to resolve cross-component navigation when directional movement
   * would cross a component boundary or is cross-axis from a group.
   * Returns a target ID to navigate to, or null to use default behavior.
   */
  resolveNavBoundary?: (
    panel: string | null,
    group: string | null,
    direction: UiNavDirectionalAction,
  ) => string | null;
  /**
   * Called to resolve component-level sequential navigation.
   * Used for Tab/Shift+Tab in UI-nav mode.
   * Returns a target ID to navigate to, or null to use default behavior.
   */
  resolveSequentialTarget?: (
    panel: string | null,
    group: string | null,
    direction: "next" | "prev",
  ) => string | null;
}

interface UiNavState {
  autoExitOnCommand: boolean;
  callbacks: UiNavCallbacks | null;
  currentPanel: string | null;
  currentTargetId: string | null;
  currentTargetEl: HTMLElement | null;
  enabled: boolean;
  lastGroup: string | null;
  lastNonMenuGroup: string | null;
  lastNonMenuPanel: string | null;
  lastNonMenuTargetId: string | null;
  lastPanel: string | null;
  lastTargetId: string | null;
  mutationObserver: MutationObserver | null;
  pendingSyncFrame: number | null;
  prevStickDir: UiNavAction | null;
}

const ACTIVE_CLASS = "ui-nav-active-target";
const BODY_MODE_CLASS = "ui-nav-mode";
const DEFAULT_TARGET: UiNavTargetName = "menubar";
const TARGET_SELECTOR =
  '[data-ui-nav-id], button:not([data-ui-nav-skip="true"])';
let autoTargetCounter = 0;

const uiNavState: UiNavState = {
  autoExitOnCommand: true,
  callbacks: null,
  currentPanel: null,
  currentTargetEl: null,
  currentTargetId: null,
  enabled: false,
  lastGroup: null,
  lastNonMenuGroup: null,
  lastNonMenuPanel: null,
  lastNonMenuTargetId: null,
  lastPanel: null,
  lastTargetId: null,
  mutationObserver: null,
  pendingSyncFrame: null,
  prevStickDir: null,
};

/**
 * Install app-level callbacks used by UI navigation mode.
 */
export function setUiNavCallbacks(callbacks: UiNavCallbacks): void {
  uiNavState.callbacks = callbacks;
}

/**
 * Whether UI navigation mode is currently enabled.
 */
export function isUiNavEnabled(): boolean {
  return uiNavState.enabled;
}

/**
 * The currently active UI navigation target ID, if any.
 */
export function getUiNavActiveTargetId(): string | null {
  return uiNavState.currentTargetId;
}

/**
 * Notify the UI navigation subsystem that a player command was run from the UI.
 * When auto-exit is enabled, this leaves UI navigation mode.
 */
export function notifyUiNavCommandIssued(): void {
  if (!uiNavState.enabled || !uiNavState.autoExitOnCommand) {
    return;
  }
  exitUiNavMode(true);
}

/**
 * Enter UI navigation mode and focus the requested target if possible.
 */
export async function enterUiNavMode(options?: {
  autoExitOnCommand?: boolean;
  target?: string;
}): Promise<boolean> {
  uiNavState.enabled = true;
  uiNavState.autoExitOnCommand = options?.autoExitOnCommand ?? true;
  document.body.classList.add(BODY_MODE_CLASS);
  startMutationObserver();
  resetGamepadUiNavState();

  let targetId: string | null = null;
  if (options?.target) {
    targetId = await resolveTargetId(options.target);
  } else if (
    uiNavState.currentTargetId &&
    findVisibleTargetById(uiNavState.currentTargetId)
  ) {
    targetId = uiNavState.currentTargetId;
  } else if (
    uiNavState.lastTargetId &&
    findVisibleTargetById(uiNavState.lastTargetId)
  ) {
    targetId = uiNavState.lastTargetId;
  } else if (uiNavState.lastGroup) {
    targetId = firstVisibleInGroup(uiNavState.lastGroup);
  } else if (uiNavState.lastPanel) {
    targetId =
      (await resolveTargetId(uiNavState.lastPanel)) ??
      findPanelFallbackTarget(uiNavState.lastPanel);
  }

  if (!targetId) {
    targetId = await resolveTargetId(DEFAULT_TARGET);
  }
  if (!targetId) {
    targetId = firstVisibleTargetId();
  }
  if (!targetId) {
    exitUiNavMode(false);
    return false;
  }

  return setActiveTargetById(targetId);
}

/**
 * Leave UI navigation mode.
 */
export function exitUiNavMode(remember = true): void {
  const wasEnabled = uiNavState.enabled;
  if (remember) {
    rememberCurrentTarget();
  }
  uiNavState.enabled = false;
  document.body.classList.remove(BODY_MODE_CLASS);
  stopMutationObserver();
  resetGamepadUiNavState();
  clearActiveTarget();
  if (wasEnabled) {
    uiNavState.callbacks?.onExit?.();
  }
}

/**
 * Handle a keyboard event using UI navigation mode bindings.
 */
export function handleUiNavKeyDown(event: KeyboardEvent): boolean {
  if (!uiNavState.enabled) {
    return false;
  }
  if (event.key === "Escape" && uiNavState.callbacks?.onEscape?.()) {
    return true;
  }

  const action = keyboardActionForEvent(event);
  if (!action) {
    return false;
  }
  if (
    action === "menu" &&
    (event.key === "ContextMenu" || event.key === "Menu")
  ) {
    event.preventDefault();
    event.stopPropagation();
  }
  performUiNavAction(action);
  return true;
}

/**
 * Process gamepad input while UI navigation mode is active.
 */
export function processUiNavGamepad(
  gamepad: Gamepad,
  prevButtons: boolean[],
  activeProfile: GamepadProfile,
): boolean {
  if (!uiNavState.enabled) {
    return false;
  }

  const stickAction = gamepadStickAction(gamepad, activeProfile);
  if (stickAction !== uiNavState.prevStickDir) {
    uiNavState.prevStickDir = stickAction;
    if (stickAction) {
      performUiNavAction(stickAction);
    }
  }

  const buttonActions: Array<[button: number, action: UiNavAction]> = [
    [0, "activate"],
    [1, "exit"],
    [2, "menu"],
    [4, "prev"],
    [5, "next"],
    [12, "move-up"],
    [13, "move-down"],
    [14, "move-left"],
    [15, "move-right"],
  ];

  for (const [button, action] of buttonActions) {
    if (button >= gamepad.buttons.length) {
      continue;
    }
    const pressed = gamepad.buttons[button]!.pressed;
    const wasPressed = prevButtons[button] ?? false;
    if (pressed && !wasPressed) {
      performUiNavAction(action);
    }
  }

  for (let i = 0; i < gamepad.buttons.length; i++) {
    prevButtons[i] = gamepad.buttons[i]!.pressed;
  }
  return true;
}

/**
 * Perform a single UI navigation action.
 */
export function performUiNavAction(action: UiNavAction): boolean {
  switch (action) {
    case "activate":
      return activateCurrentTarget();
    case "exit":
      exitUiNavMode(true);
      return true;
    case "menu":
      return openMenuForCurrentTarget();
    case "move-down":
    case "move-end":
    case "move-home":
    case "move-left":
    case "move-page-down":
    case "move-page-up":
    case "move-right":
    case "move-up":
      if (action === "move-home") {
        return moveToBoundary(false);
      }
      if (action === "move-end") {
        return moveToBoundary(true);
      }
      return moveDirectional(action);
    case "next-component":
      return moveSequentialByComponent(1);
    case "next":
      return moveSequential(1);
    case "prev-component":
      return moveSequentialByComponent(-1);
    case "prev":
      return moveSequential(-1);
  }
}

/**
 * Open the menu for the current UI navigation target, if one exists.
 */
export function openUiNavMenu(): boolean {
  return openMenuForCurrentTarget();
}

/**
 * Focus a specific UI navigation target by ID, if it is currently visible.
 */
export function focusUiNavTarget(targetId: string): boolean {
  return setActiveTargetById(targetId);
}

function keyboardActionForEvent(event: KeyboardEvent): UiNavAction | null {
  switch (event.key) {
    case "ArrowDown":
      return "move-down";
    case "ArrowLeft":
      return "move-left";
    case "ArrowRight":
      return "move-right";
    case "ArrowUp":
      return "move-up";
    case "PageDown":
      return "move-page-down";
    case "PageUp":
      return "move-page-up";
    case "Home":
      return "move-home";
    case "End":
      return "move-end";
    case "Enter":
    case " ":
    case "Spacebar":
      return "activate";
    case "Escape":
      return "exit";
    case "Tab":
      return event.shiftKey ? "prev-component" : "next-component";
    case "ContextMenu":
    case "Menu":
      return "menu";
    default:
      if (event.key.length === 1 && event.key.toLowerCase() === "m") {
        return "menu";
      }
      return null;
  }
}

function gamepadStickAction(
  gamepad: Gamepad,
  activeProfile: GamepadProfile,
): UiNavAction | null {
  const axes = activeProfile.walkStick;
  const rawX = gamepad.axes[axes.axisX] ?? 0;
  const rawY = gamepad.axes[axes.axisY] ?? 0;
  const x = axes.invertX ? -rawX : rawX;
  const y = axes.invertY ? -rawY : rawY;
  const threshold = Math.max(0.35, activeProfile.walkThreshold);

  if (Math.abs(x) < threshold && Math.abs(y) < threshold) {
    return null;
  }
  if (Math.abs(x) > Math.abs(y)) {
    return x > 0 ? "move-right" : "move-left";
  }
  return y > 0 ? "move-down" : "move-up";
}

function resetGamepadUiNavState(): void {
  uiNavState.prevStickDir = null;
}

function activateCurrentTarget(): boolean {
  const target = getCurrentVisibleTarget();
  if (!target) {
    return false;
  }
  if (target.dataset.uiNavMenu === "context") {
    return openMenuForCurrentTarget();
  }
  target.click();
  scheduleSync();
  return true;
}

function openMenuForCurrentTarget(): boolean {
  const target = getCurrentVisibleTarget();
  if (!target) {
    return false;
  }
  if (target.dataset.uiNavMenu !== "context") {
    return false;
  }

  const rect = target.getBoundingClientRect();
  const clientX = rect.left + rect.width / 2;
  const clientY = rect.top + rect.height / 2;
  target.dispatchEvent(
    new MouseEvent("contextmenu", {
      bubbles: true,
      button: 2,
      cancelable: true,
      clientX,
      clientY,
    }),
  );
  scheduleSync();
  return true;
}

function moveDirectional(action: UiNavDirectionalAction): boolean {
  if (action === "move-page-up" || action === "move-page-down") {
    return moveByPage(action === "move-page-up" ? -1 : 1);
  }

  const targets = visibleTargets();
  if (targets.length === 0) {
    return false;
  }

  const current = getCurrentVisibleTarget() ?? targets[0]!;
  const constrainedTargets = constrainedTargetsForAction(
    current,
    action,
    targets,
  );
  if (constrainedTargets) {
    const bestConstrainedTarget = pickDirectionalTarget(
      current,
      action,
      constrainedTargets,
    );
    if (bestConstrainedTarget) {
      return setActiveTargetElement(bestConstrainedTarget);
    }

    const boundaryTargetId = directionalBoundaryTargetId(current, action);
    if (boundaryTargetId) {
      return setActiveTargetById(boundaryTargetId);
    }

    if (tryNavBoundaryCallback(current, action)) {
      return true;
    }

    return moveSequentialInTargets(
      constrainedTargets,
      current,
      action === "move-up" || action === "move-left" ? -1 : 1,
    );
  }

  if (tryMenuOriginDirectionalFallback(action, targets)) {
    return true;
  }

  // Cross-axis from a group: try app-level boundary routing before geometry.
  if (current.dataset.uiNavGroup && tryNavBoundaryCallback(current, action)) {
    return true;
  }

  const bestTarget = pickDirectionalTarget(current, action, targets);
  if (bestTarget) {
    return setActiveTargetElement(bestTarget);
  }
  return moveSequential(
    action === "move-up" || action === "move-left" ? -1 : 1,
  );
}

function tryNavBoundaryCallback(
  current: HTMLElement,
  action: UiNavDirectionalAction,
): boolean {
  const panel = current.dataset.uiNavPanel ?? null;
  const group = current.dataset.uiNavGroup ?? null;
  const targetId = uiNavState.callbacks?.resolveNavBoundary?.(
    panel,
    group,
    action,
  );
  if (!targetId) return false;
  return setActiveTargetById(targetId);
}

function tryMenuOriginDirectionalFallback(
  action: UiNavDirectionalAction,
  targets: HTMLElement[],
): boolean {
  if (
    uiNavState.currentPanel !== "menu" ||
    (action !== "move-up" && action !== "move-down")
  ) {
    return false;
  }

  const origin = rememberedNonMenuTarget(action === "move-up" ? -1 : 1);
  if (!origin) {
    return false;
  }

  const candidates = targetsForCurrentList(origin, targets);
  if (
    candidates.length > 1 &&
    origin.dataset.uiNavId === uiNavState.lastNonMenuTargetId
  ) {
    return moveSequentialInTargets(
      candidates,
      origin,
      action === "move-up" ? -1 : 1,
    );
  }
  return setActiveTargetElement(origin);
}

function moveByPage(deltaPages: number): boolean {
  const targets = visibleTargets();
  if (targets.length === 0) {
    return false;
  }

  const current = getCurrentVisibleTarget() ?? targets[0]!;
  const candidates = targetsForCurrentList(current, targets);
  if (candidates.length === 0) {
    return false;
  }

  const currentIndex = candidates.findIndex(
    (candidate) => candidate.dataset.uiNavId === current.dataset.uiNavId,
  );
  const startIndex = currentIndex >= 0 ? currentIndex : 0;
  const pageSize = pageSizeForTarget(current);
  const nextIndex = Math.max(
    0,
    Math.min(candidates.length - 1, startIndex + deltaPages * pageSize),
  );
  return setActiveTargetElement(candidates[nextIndex]!);
}

function moveToBoundary(toEnd: boolean): boolean {
  const targets = visibleTargets();
  if (targets.length === 0) {
    return false;
  }

  const current = getCurrentVisibleTarget() ?? targets[0]!;
  const candidates = targetsForCurrentList(current, targets);
  if (candidates.length === 0) {
    return false;
  }
  return setActiveTargetElement(
    toEnd ? candidates[candidates.length - 1]! : candidates[0]!,
  );
}

function targetsForCurrentList(
  current: HTMLElement,
  targets: HTMLElement[],
): HTMLElement[] {
  const group = current.dataset.uiNavGroup;
  if (!group) {
    return targets;
  }
  const groupedTargets = targets.filter(
    (candidate) => candidate.dataset.uiNavGroup === group,
  );
  return groupedTargets.length > 0 ? groupedTargets : targets;
}

function pageSizeForTarget(target: HTMLElement): number {
  const scrollParent = findVerticalScrollParent(target);
  if (!scrollParent) {
    return 10;
  }
  const targetHeight = target.getBoundingClientRect().height;
  if (targetHeight <= 0) {
    return 10;
  }
  return Math.max(1, Math.floor(scrollParent.clientHeight / targetHeight));
}

function findVerticalScrollParent(target: HTMLElement): HTMLElement | null {
  let parent = target.parentElement;
  while (parent) {
    const style = window.getComputedStyle(parent);
    const overflowY = style.overflowY;
    if (
      (overflowY === "auto" || overflowY === "scroll") &&
      parent.scrollHeight > parent.clientHeight
    ) {
      return parent;
    }
    parent = parent.parentElement;
  }
  return null;
}

function pickDirectionalTarget(
  current: HTMLElement,
  action: UiNavDirectionalAction,
  targets: HTMLElement[],
): HTMLElement | null {
  if (targets.length === 0) {
    return null;
  }

  const currentRect = current.getBoundingClientRect();
  const currentCx = currentRect.left + currentRect.width / 2;
  const currentCy = currentRect.top + currentRect.height / 2;

  let bestTarget: HTMLElement | null = null;
  let bestScore = Number.POSITIVE_INFINITY;

  for (const candidate of targets) {
    if (candidate === current) {
      continue;
    }
    const rect = candidate.getBoundingClientRect();
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    const dx = cx - currentCx;
    const dy = cy - currentCy;

    let primaryDistance = 0;
    let secondaryDistance = 0;
    switch (action) {
      case "move-up":
        if (cy >= currentCy) continue;
        primaryDistance = currentCy - cy;
        secondaryDistance = Math.abs(dx);
        break;
      case "move-down":
        if (cy <= currentCy) continue;
        primaryDistance = cy - currentCy;
        secondaryDistance = Math.abs(dx);
        break;
      case "move-left":
        if (cx >= currentCx) continue;
        primaryDistance = currentCx - cx;
        secondaryDistance = Math.abs(dy);
        break;
      case "move-right":
        if (cx <= currentCx) continue;
        primaryDistance = cx - currentCx;
        secondaryDistance = Math.abs(dy);
        break;
    }

    const score = primaryDistance * 1000 + secondaryDistance;
    if (score < bestScore) {
      bestScore = score;
      bestTarget = candidate;
    }
  }
  return bestTarget;
}

function constrainedTargetsForAction(
  current: HTMLElement,
  action: UiNavDirectionalAction,
  targets: HTMLElement[],
): HTMLElement[] | null {
  const group = current.dataset.uiNavGroup;
  if (!group) {
    return null;
  }

  const policy = (current.dataset.uiNavGroupPolicy ??
    "both") as UiNavGroupPolicy;
  if (!policyAppliesToAction(policy, action)) {
    return null;
  }

  const groupedTargets = targets.filter(
    (candidate) => candidate.dataset.uiNavGroup === group,
  );
  return groupedTargets.length > 1 ? groupedTargets : null;
}

function policyAppliesToAction(
  policy: UiNavGroupPolicy,
  action: UiNavDirectionalAction,
): boolean {
  const isHorizontal = action === "move-left" || action === "move-right";
  const isVertical =
    action === "move-up" ||
    action === "move-down" ||
    action === "move-page-up" ||
    action === "move-page-down";
  return (
    policy === "both" ||
    (policy === "horizontal" && isHorizontal) ||
    (policy === "vertical" && isVertical)
  );
}

function moveSequential(delta: number): boolean {
  const targets = visibleTargets();
  if (targets.length === 0) {
    return false;
  }

  const current = getCurrentVisibleTarget();
  if (!current) {
    return setActiveTargetElement(
      delta >= 0 ? targets[0]! : targets[targets.length - 1]!,
    );
  }

  const currentIndex = targets.findIndex(
    (target) => target.dataset.uiNavId === current.dataset.uiNavId,
  );
  if (currentIndex < 0) {
    return setActiveTargetElement(
      delta >= 0 ? targets[0]! : targets[targets.length - 1]!,
    );
  }

  const nextIndex = (currentIndex + delta + targets.length) % targets.length;
  return setActiveTargetElement(targets[nextIndex]!);
}

function moveSequentialByComponent(delta: number): boolean {
  const current = getCurrentVisibleTarget();
  const direction: "next" | "prev" = delta >= 0 ? "next" : "prev";
  const targetId = uiNavState.callbacks?.resolveSequentialTarget?.(
    current?.dataset.uiNavPanel ?? null,
    current?.dataset.uiNavGroup ?? null,
    direction,
  );
  if (targetId) {
    return setActiveTargetById(targetId);
  }
  return moveSequential(delta);
}

function moveSequentialInTargets(
  targets: HTMLElement[],
  current: HTMLElement,
  delta: number,
): boolean {
  const currentIndex = targets.findIndex(
    (target) => target.dataset.uiNavId === current.dataset.uiNavId,
  );
  if (currentIndex < 0) {
    return false;
  }
  const nextIndex = (currentIndex + delta + targets.length) % targets.length;
  return setActiveTargetElement(targets[nextIndex]!);
}

function directionalBoundaryTargetId(
  current: HTMLElement,
  action: UiNavDirectionalAction,
): string | null {
  switch (action) {
    case "move-down":
      return current.dataset.uiNavDownTarget ?? null;
    case "move-left":
      return current.dataset.uiNavLeftTarget ?? null;
    case "move-right":
      return current.dataset.uiNavRightTarget ?? null;
    case "move-up":
      return current.dataset.uiNavUpTarget ?? null;
    default:
      return null;
  }
}

function getCurrentVisibleTarget(): HTMLElement | null {
  if (!uiNavState.currentTargetId) {
    return null;
  }
  return findVisibleTargetById(uiNavState.currentTargetId);
}

function setActiveTargetById(targetId: string): boolean {
  const target = findVisibleTargetById(targetId);
  if (!target) {
    return false;
  }
  return setActiveTargetElement(target);
}

function setActiveTargetElement(target: HTMLElement): boolean {
  const previousTargetEl = uiNavState.currentTargetEl;
  const previousTargetId = uiNavState.currentTargetId;
  clearActiveTarget();
  uiNavState.currentTargetEl = target;
  uiNavState.currentTargetId = target.dataset.uiNavId ?? null;
  uiNavState.currentPanel = target.dataset.uiNavPanel ?? null;
  if (uiNavState.currentPanel !== "menu") {
    uiNavState.lastNonMenuTargetId = uiNavState.currentTargetId;
    uiNavState.lastNonMenuPanel = uiNavState.currentPanel;
    uiNavState.lastNonMenuGroup = target.dataset.uiNavGroup ?? null;
  }
  target.classList.add(ACTIVE_CLASS);
  target.setAttribute("data-ui-nav-current", "true");
  ensureTargetFullyVisible(target);
  if (previousTargetEl !== target) {
    uiNavState.callbacks?.onTargetChanged?.(
      previousTargetId,
      uiNavState.currentTargetId,
    );
  }
  return true;
}

function ensureTargetFullyVisible(target: HTMLElement): void {
  const scrollParent = findVerticalScrollParent(target);
  if (!scrollParent) {
    target.scrollIntoView({
      behavior: "auto",
      block: "nearest",
      inline: "nearest",
    });
    return;
  }

  const parentRect = scrollParent.getBoundingClientRect();
  const targetRect = target.getBoundingClientRect();
  const visibleTop = parentRect.top + topOcclusionInset(scrollParent);
  if (targetRect.top < visibleTop) {
    scrollParent.scrollTop += targetRect.top - visibleTop;
    return;
  }
  if (targetRect.bottom > parentRect.bottom) {
    scrollParent.scrollTop += targetRect.bottom - parentRect.bottom;
    return;
  }
  target.scrollIntoView({
    behavior: "auto",
    block: "nearest",
    inline: "nearest",
  });
}

function topOcclusionInset(scrollParent: HTMLElement): number {
  let inset = 0;
  for (const el of scrollParent.querySelectorAll<HTMLElement>("*")) {
    if (!isTargetVisible(el)) continue;
    const style = window.getComputedStyle(el);
    if (style.position !== "sticky") continue;
    if (style.top !== "0px") continue;
    const rect = el.getBoundingClientRect();
    const parentRect = scrollParent.getBoundingClientRect();
    if (rect.bottom <= parentRect.top) continue;
    if (rect.top >= parentRect.bottom) continue;
    inset = Math.max(inset, rect.bottom - parentRect.top);
  }
  return inset;
}

function clearActiveTarget(): void {
  if (uiNavState.currentTargetEl?.isConnected) {
    uiNavState.currentTargetEl.classList.remove(ACTIVE_CLASS);
    uiNavState.currentTargetEl.removeAttribute("data-ui-nav-current");
  }
  uiNavState.currentTargetEl = null;
  uiNavState.currentTargetId = null;
  uiNavState.currentPanel = null;
}

function rememberCurrentTarget(): void {
  if (uiNavState.currentPanel === "menu" && uiNavState.lastNonMenuTargetId) {
    uiNavState.lastTargetId = uiNavState.lastNonMenuTargetId;
    uiNavState.lastPanel = uiNavState.lastNonMenuPanel;
    uiNavState.lastGroup = uiNavState.lastNonMenuGroup;
    return;
  }
  uiNavState.lastTargetId = uiNavState.currentTargetId;
  uiNavState.lastPanel = uiNavState.currentPanel;
  uiNavState.lastGroup = uiNavState.currentTargetEl?.dataset.uiNavGroup ?? null;
}

function rememberedNonMenuTarget(delta: number): HTMLElement | null {
  if (uiNavState.lastNonMenuTargetId) {
    const target = findVisibleTargetById(uiNavState.lastNonMenuTargetId);
    if (target) {
      return target;
    }
  }
  if (!uiNavState.lastNonMenuGroup) {
    return null;
  }
  const targetId =
    delta < 0
      ? lastVisibleInGroup(uiNavState.lastNonMenuGroup)
      : firstVisibleInGroup(uiNavState.lastNonMenuGroup);
  return targetId ? findVisibleTargetById(targetId) : null;
}

function startMutationObserver(): void {
  if (uiNavState.mutationObserver) {
    return;
  }
  uiNavState.mutationObserver = new MutationObserver(() => {
    scheduleSync();
  });
  uiNavState.mutationObserver.observe(document.body, {
    attributeFilter: ["class", "hidden", "style"],
    attributes: true,
    childList: true,
    subtree: true,
  });
}

function stopMutationObserver(): void {
  if (uiNavState.pendingSyncFrame !== null) {
    cancelAnimationFrame(uiNavState.pendingSyncFrame);
    uiNavState.pendingSyncFrame = null;
  }
  uiNavState.mutationObserver?.disconnect();
  uiNavState.mutationObserver = null;
}

function scheduleSync(): void {
  if (!uiNavState.enabled || uiNavState.pendingSyncFrame !== null) {
    return;
  }
  uiNavState.pendingSyncFrame = requestAnimationFrame(() => {
    uiNavState.pendingSyncFrame = null;
    syncActiveTarget();
  });
}

function syncActiveTarget(): void {
  if (!uiNavState.enabled) {
    return;
  }
  const current = getCurrentVisibleTarget();
  if (current) {
    setActiveTargetElement(current);
    return;
  }

  const fallback =
    findPanelFallbackTarget(uiNavState.currentPanel) ?? firstVisibleTargetId();
  if (fallback) {
    setActiveTargetById(fallback);
  }
}

function findPanelFallbackTarget(panel: string | null): string | null {
  if (!panel) {
    return null;
  }
  const candidates = visibleTargets().filter(
    (target) => target.dataset.uiNavPanel === panel,
  );
  return candidates[0]?.dataset.uiNavId ?? null;
}

async function resolveTargetId(target: string): Promise<string | null> {
  const trimmed = normaliseTargetToken(target);
  if (trimmed.length === 0) {
    return null;
  }

  if (findTargetById(trimmed)) {
    return trimmed;
  }

  const resolved = await uiNavState.callbacks?.resolveTargetId?.(trimmed);
  if (resolved) {
    return resolved;
  }
  return null;
}

function normaliseTargetToken(target: string): string {
  return target.trim().toLowerCase();
}

function firstVisibleTargetId(): string | null {
  return visibleTargets()[0]?.dataset.uiNavId ?? null;
}

function findTargetById(targetId: string): HTMLElement | null {
  return document.querySelector<HTMLElement>(
    `[data-ui-nav-id="${cssEscape(targetId)}"]`,
  );
}

function findVisibleTargetById(targetId: string): HTMLElement | null {
  const target = findTargetById(targetId);
  return target && isTargetVisible(target) ? target : null;
}

function visibleTargets(): HTMLElement[] {
  const seen = new Set<HTMLElement>();
  const targets: HTMLElement[] = [];
  for (const target of document.querySelectorAll<HTMLElement>(
    TARGET_SELECTOR,
  )) {
    if (seen.has(target)) {
      continue;
    }
    seen.add(target);
    ensureTargetId(target);
    if (isTargetVisible(target)) {
      targets.push(target);
    }
  }
  return targets;
}

function isTargetVisible(target: HTMLElement): boolean {
  if (!target.isConnected || target.hidden) {
    return false;
  }
  if ("disabled" in target && (target as HTMLButtonElement).disabled) {
    return false;
  }

  const rect = target.getBoundingClientRect();
  if (rect.width <= 0 || rect.height <= 0) {
    return false;
  }

  const style = window.getComputedStyle(target);
  if (style.display === "none" || style.visibility === "hidden") {
    return false;
  }
  return true;
}

function cssEscape(value: string): string {
  return CSS.escape(value);
}

function ensureTargetId(target: HTMLElement): string {
  if (!target.dataset.uiNavId) {
    target.dataset.uiNavId = `ui-auto-target-${autoTargetCounter++}`;
  }
  return target.dataset.uiNavId;
}

/**
 * Returns the ID of the first visible element in a nav group, or null.
 */
export function firstVisibleInGroup(group: string): string | null {
  for (const el of document.querySelectorAll<HTMLElement>(
    `[data-ui-nav-group="${cssEscape(group)}"]`,
  )) {
    if (isTargetVisible(el)) return el.dataset.uiNavId ?? null;
  }
  return null;
}

/**
 * Returns the ID of the last visible element in a nav group, or null.
 */
export function lastVisibleInGroup(group: string): string | null {
  const els = [
    ...document.querySelectorAll<HTMLElement>(
      `[data-ui-nav-group="${cssEscape(group)}"]`,
    ),
  ];
  for (let i = els.length - 1; i >= 0; i--) {
    if (isTargetVisible(els[i]!)) return els[i]!.dataset.uiNavId ?? null;
  }
  return null;
}
