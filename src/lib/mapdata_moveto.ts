/**
 * mapdata_moveto.ts — Click-to-move (move-to) pathfinding logic.
 * Extracted from mapdata.ts.
 *
 * Provides the move-to state machine: the player clicks a tile on the map,
 * the client records the absolute destination, and each tick calls
 * `run_move_to()` which issues a walk/run command in the computed direction.
 */

// ── Types ───────────────────────────────────────────────────────────────────

/** Function to compute absolute player position (centre of the view). */
export type PlMposFn = () => { px: number; py: number };

// ── State ───────────────────────────────────────────────────────────────────

export let moveToX = 0;
export let moveToY = 0;
export let moveToAttack = false;

/**
 * Player position at the time the last walk/run command was sent.
 * We hold off sending another command until the player actually moves
 * (pl_pos changes), preventing command flooding that causes overshoot.
 */
let moveToLastPos: { px: number; py: number } | null = null;

/**
 * Number of consecutive ticks where movement was expected (a command was
 * sent) but the player's position did not change.  After a threshold this
 * indicates the path is blocked (wall, etc.) and we abandon the move-to.
 */
let moveToStuckTicks = 0;

/** After this many consecutive stuck ticks, abandon the move-to. */
const MOVE_TO_STUCK_LIMIT = 3;

/** Callbacks injected by mapdata.ts. */
let plMposFn: PlMposFn = () => ({ px: 0, py: 0 });
let stopRunFn: () => void = () => {};
let walkDirFn: (dir: number) => void = () => {};
let runDirFn: (dir: number) => void = () => {};

/** Wire callbacks so the move-to subsystem can interact with the player. */
export function setMoveToCallbacks(cbs: {
    plMpos: PlMposFn;
    stopRun: () => void;
    walkDir: (dir: number) => void;
    runDir: (dir: number) => void;
}): void {
    plMposFn = cbs.plMpos;
    stopRunFn = cbs.stopRun;
    walkDirFn = cbs.walkDir;
    runDirFn = cbs.runDir;
}

// ── Public API ──────────────────────────────────────────────────────────────

/** Set the move-to destination.  `dx` and `dy` are relative to the player. */
export function set_move_to(dx: number, dy: number): void {
    const oldX = moveToX;
    const oldY = moveToY;

    const pos = plMposFn();
    moveToX = pos.px + dx;
    moveToY = pos.py + dy;

    // Detect double-click on the same destination.
    if (moveToX === oldX && moveToY === oldY) {
        moveToAttack = true;
    } else {
        moveToAttack = false;
    }

    // Always reset the rate-limit position and stuck counter so that a new
    // click immediately sends the first command, even if the previous
    // move-to was blocked (e.g. a wall stopped the player).
    moveToLastPos = null;
    moveToStuckTicks = 0;
}

/** Clear the current move-to destination. */
export function clear_move_to(): void {
    moveToX = 0;
    moveToY = 0;
    moveToAttack = false;
    moveToLastPos = null;
    moveToStuckTicks = 0;
}

/** Return true if the player is at (or has no) move-to destination. */
export function is_at_moveto(): boolean {
    if (moveToX === 0 && moveToY === 0) {
        return true;
    }
    const pos = plMposFn();
    return pos.px === moveToX && pos.py === moveToY;
}

/** Take one step towards the move-to destination. */
export function run_move_to(): void {
    if (moveToX === 0 && moveToY === 0) {
        return;
    }

    if (is_at_moveto()) {
        clear_move_to();
        stopRunFn();
        return;
    }

    const pos = plMposFn();

    // Rate-limit: don't queue another command until the player has actually
    // moved since the last one was sent.  Without this guard, sending one
    // command per local timer tick (125 ms) causes the server to receive
    // several identical direction commands before a single mapscroll arrives,
    // making the player overshoot the destination.
    if (moveToLastPos !== null &&
        pos.px === moveToLastPos.px &&
        pos.py === moveToLastPos.py) {
        // The player didn't move — could be normal network latency, or the
        // path is blocked (wall).  Count consecutive stuck ticks and give up
        // after the threshold so the yellow border doesn't linger forever.
        moveToStuckTicks++;
        if (moveToStuckTicks >= MOVE_TO_STUCK_LIMIT) {
            clear_move_to();
        }
        return;
    }

    const dx = moveToX - pos.px;
    const dy = moveToY - pos.py;
    const dir = relative_direction(dx, dy);

    if (moveToAttack) {
        runDirFn(dir);
    } else {
        walkDirFn(dir);
    }

    moveToLastPos = { px: pos.px, py: pos.py };
    moveToStuckTicks = 0;
}

/**
 * Compute the direction (0-8) from a relative dx/dy offset.
 *
 * Returns:
 *   0 = standing still,
 *   1 = north, 2 = northeast, 3 = east, 4 = southeast,
 *   5 = south, 6 = southwest, 7 = west, 8 = northwest
 */
export function relative_direction(dx: number, dy: number): number {
    if (dx === 0 && dy === 0) return 0;
    if (dx === 0 && dy < 0) return 1;
    if (dx > 0 && dy < 0) return 2;
    if (dx > 0 && dy === 0) return 3;
    if (dx > 0 && dy > 0) return 4;
    if (dx === 0 && dy > 0) return 5;
    if (dx < 0 && dy > 0) return 6;
    if (dx < 0 && dy === 0) return 7;
    if (dx < 0 && dy < 0) return 8;
    return 0;
}
