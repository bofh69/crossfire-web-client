/**
 * sound.ts – Web-based sound/music manager for the Crossfire web client.
 *
 * Plays sound effects and background music using the Web Audio API and
 * HTMLAudioElement.  Sound assets are expected to be available under the
 * `sounds/` path served by the web server (populated from the
 * crossfire-sounds submodule).
 *
 * The sound configuration (sounds.conf) is fetched once at init and maps
 * logical sound names to filenames + default volumes.
 */

import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { loadConfig, saveConfig } from './storage.js';
import {
  MUSIC_VOLUME_CAP,
  MUSIC_FADE_OUT_MS,
  MUSIC_FADE_STOP_MS,
  MUSIC_FADE_IN_MS,
  MUSIC_FADE_STEP_MS,
} from './constants.js';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

interface SoundInfo {
  file: string;
  vol: number;       // 0–100
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

/** Base URL prefix for sound assets (no trailing slash). */
const SOUND_BASE = 'sounds';

/** Parsed sounds.conf mapping: logical-name → SoundInfo */
let soundConfig: Map<string, SoundInfo> | null = null;

/** True once init has completed (successfully or not). */
let initDone = false;

/** Web AudioContext used for sound effects. */
let audioCtx: AudioContext | null = null;

/** Cache of decoded AudioBuffers keyed by filename. */
const bufferCache = new Map<string, AudioBuffer>();

/** Set of filenames currently being fetched (avoid duplicate requests). */
const fetchingBuffers = new Set<string>();

/** Current background music element (playing or fading in). */
let musicElement: HTMLAudioElement | null = null;

/** Music element currently being faded out. */
let fadingOutElement: HTMLAudioElement | null = null;

/** Interval ID for any in-progress fade-out animation. */
let fadeOutIntervalId: ReturnType<typeof setInterval> | null = null;

/** Interval ID for any in-progress fade-in animation. */
let fadeInIntervalId: ReturnType<typeof setInterval> | null = null;

/** Name of the music track currently playing (without extension). */
let currentMusic = '';

/** Master sound-enabled flag. */
let soundEnabled = true;

/** Whether background music is muted (independent of soundEnabled). */
let musicMuted: boolean = loadConfig<boolean>('musicMuted', false);

/** Whether sound effects are muted (independent of soundEnabled). */
let sfxMuted: boolean = loadConfig<boolean>('sfxMuted', false);

/** Music volume 0–100. */
let musicVolume = 100;

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

/**
 * Initialise the sound subsystem.
 *
 * Fetches `sounds/sounds.conf`, parses it, and prepares the AudioContext.
 * Safe to call multiple times; only the first call does work.
 */
export async function initSound(): Promise<void> {
  if (initDone) return;
  initDone = true;

  try {
    const resp = await fetch(`${SOUND_BASE}/sounds.conf`);
    if (!resp.ok) {
      LOG(LogLevel.Warning, 'initSound', `Failed to fetch sounds.conf: ${resp.status}`);
      return;
    }
    const text = await resp.text();
    soundConfig = parseSoundsConf(text);
    LOG(LogLevel.Info, 'initSound', `Loaded ${soundConfig.size} sound definitions`);
  } catch (err) {
    LOG(LogLevel.Warning, 'initSound', `Could not load sounds.conf: ${err}`);
  }
}

/**
 * Parse the `sounds.conf` format used by the crossfire-sounds package.
 *
 * Each non-comment, non-empty line has the format:
 *   name:volume:filename
 */
function parseSoundsConf(text: string): Map<string, SoundInfo> {
  const map = new Map<string, SoundInfo>();
  for (const raw of text.split('\n')) {
    const line = raw.replace(/\r$/, '').trim();
    if (line.length === 0 || line[0] === '#') continue;
    const parts = line.split(':');
    if (parts.length < 3) {
      LOG(LogLevel.Warning, 'parseSoundsConf', `Bad line: ${line}`);
      continue;
    }
    const name = parts[0]!;
    const vol = parseInt(parts[1]!, 10);
    const file = parts[2]!;
    if (name && file) {
      map.set(name, { file, vol: isNaN(vol) ? 100 : vol });
    }
  }
  return map;
}

// ---------------------------------------------------------------------------
// AudioContext helpers
// ---------------------------------------------------------------------------

/** Lazily create (or resume) the AudioContext after a user gesture. */
function ensureAudioContext(): AudioContext | null {
  if (!audioCtx) {
    try {
      audioCtx = new AudioContext();
    } catch {
      LOG(LogLevel.Warning, 'ensureAudioContext', 'Web Audio API not available');
      return null;
    }
  }
  if (audioCtx.state === 'suspended') {
    audioCtx.resume().catch(() => { /* ignored */ });
  }
  return audioCtx;
}

/**
 * Fetch and decode a sound file, returning a cached AudioBuffer.
 * Returns null if the file cannot be loaded.
 */
async function loadBuffer(filename: string): Promise<AudioBuffer | null> {
  const cached = bufferCache.get(filename);
  if (cached) return cached;

  if (fetchingBuffers.has(filename)) return null;   // already in-flight
  fetchingBuffers.add(filename);

  const ctx = ensureAudioContext();
  if (!ctx) { fetchingBuffers.delete(filename); return null; }

  try {
    const url = `${SOUND_BASE}/${filename}`;
    const resp = await fetch(url);
    if (!resp.ok) {
      LOG(LogLevel.Warning, 'loadBuffer', `${url}: ${resp.status}`);
      return null;
    }
    const arrayBuf = await resp.arrayBuffer();
    const audioBuf = await ctx.decodeAudioData(arrayBuf);
    bufferCache.set(filename, audioBuf);
    return audioBuf;
  } catch (err) {
    LOG(LogLevel.Warning, 'loadBuffer', `Failed to decode ${filename}: ${err}`);
    return null;
  } finally {
    fetchingBuffers.delete(filename);
  }
}

// ---------------------------------------------------------------------------
// Public API – sound effects
// ---------------------------------------------------------------------------

/**
 * Play a sound effect.
 *
 * Called from `Sound2Cmd` with the parameters parsed from the server.
 *
 * @param _x      Horizontal offset from the player.
 * @param _y      Vertical offset from the player.
 * @param _dir    Direction of the sound source.
 * @param vol     Volume (0–100) sent by the server.
 * @param _type   Sound type (SOUND_TYPE_*).
 * @param sound   Logical sound name (looked up in sounds.conf).
 * @param _source Source object name (informational).
 */
export function playSound(
  _x: number, _y: number, _dir: number, vol: number, _type: number,
  sound: string, _source: string,
): void {
  if (!soundEnabled || sfxMuted || !soundConfig) return;

  const si = soundConfig.get(sound);
  if (!si) {
    LOG(LogLevel.Debug, 'playSound', `Unknown sound: ${sound}`);
    return;
  }

  const ctx = ensureAudioContext();
  if (!ctx) return;

  // Fire-and-forget: load + play.
  loadBuffer(si.file).then(buf => {
    if (!buf) return;
    const src = ctx.createBufferSource();
    src.buffer = buf;

    const gain = ctx.createGain();
    // Combine the per-sound volume from config with the server volume.
    gain.gain.value = (si.vol / 100) * (vol / 100);
    src.connect(gain).connect(ctx.destination);
    src.start();
  });
}

// ---------------------------------------------------------------------------
// Public API – music
// ---------------------------------------------------------------------------

/** Stop and discard any element that is currently fading out. */
function stopFadeOut(): void {
  if (fadeOutIntervalId !== null) {
    clearInterval(fadeOutIntervalId);
    fadeOutIntervalId = null;
  }
  if (fadingOutElement) {
    fadingOutElement.pause();
    fadingOutElement.src = '';
    fadingOutElement = null;
  }
}

/** Cancel any in-progress fade-in (the element keeps its current volume). */
function stopFadeIn(): void {
  if (fadeInIntervalId !== null) {
    clearInterval(fadeInIntervalId);
    fadeInIntervalId = null;
  }
}

/**
 * Linearly fade out `el` over `durationMs` milliseconds, then call `onComplete`.
 * Assumes `fadingOutElement` has already been set to `el` by the caller.
 */
function fadeOutElement(el: HTMLAudioElement, durationMs: number, onComplete: () => void): void {
  const startVol = el.volume;
  const steps = Math.max(1, Math.round(durationMs / MUSIC_FADE_STEP_MS));
  const stepVol = startVol / steps;
  let step = 0;
  fadeOutIntervalId = setInterval(() => {
    step++;
    el.volume = Math.max(0, startVol - stepVol * step);
    if (step >= steps) {
      clearInterval(fadeOutIntervalId!);
      fadeOutIntervalId = null;
      el.pause();
      el.src = '';
      if (fadingOutElement === el) fadingOutElement = null;
      onComplete();
    }
  }, MUSIC_FADE_STEP_MS);
}

/** Linearly fade in `el` from 0 to `targetVol` over `durationMs` milliseconds. */
function fadeInElement(el: HTMLAudioElement, targetVol: number, durationMs: number): void {
  el.volume = 0;
  const steps = Math.max(1, Math.round(durationMs / MUSIC_FADE_STEP_MS));
  const stepVol = targetVol / steps;
  let step = 0;
  fadeInIntervalId = setInterval(() => {
    step++;
    el.volume = Math.min(targetVol, stepVol * step);
    if (step >= steps) {
      clearInterval(fadeInIntervalId!);
      fadeInIntervalId = null;
      el.volume = targetVol;  // ensure we land exactly on the target
    }
  }, MUSIC_FADE_STEP_MS);
}

/**
 * Create a new Audio element for `name` and start playing it.
 * If `fadeIn` is true the track fades in over `MUSIC_FADE_IN_MS`; otherwise
 * it starts at full volume immediately.  OGG is tried first, with MP3 as fallback.
 */
function startTrack(name: string, fadeIn: boolean): void {
  const el = new Audio();
  el.loop = true;
  const targetVol = Math.min(musicVolume, 100) / 100 * MUSIC_VOLUME_CAP;  // cap at 75 % like old client
  musicElement = el;

  const oggUrl = `${SOUND_BASE}/music/${name}.ogg`;
  const mp3Url = `${SOUND_BASE}/music/${name}.mp3`;

  el.src = oggUrl;
  el.play().then(() => {
    if (fadeIn) {
      fadeInElement(el, targetVol, MUSIC_FADE_IN_MS);
    } else {
      el.volume = targetVol;
    }
  }).catch(() => {
    // OGG may not be supported; try MP3.
    el.src = mp3Url;
    el.play().then(() => {
      if (fadeIn) {
        fadeInElement(el, targetVol, MUSIC_FADE_IN_MS);
      } else {
        el.volume = targetVol;
      }
    }).catch(err => {
      LOG(LogLevel.Warning, 'playMusic', `Could not play music "${name}": ${err}`);
    });
  });
}

/**
 * Start playing background music.
 *
 * Music files live under `sounds/music/{name}.ogg` (falling back to `.mp3`).
 * Passing `"NONE"` stops the current music.
 *
 * When switching tracks the old song fades out over `MUSIC_FADE_OUT_MS` and
 * the new one fades in over `MUSIC_FADE_IN_MS`.  When stopping (`"NONE"`),
 * the old song fades out over `MUSIC_FADE_STOP_MS`.  If no music is currently
 * playing the new track starts immediately at full volume.
 *
 * @param name  Music track name (without path or extension), or `"NONE"`.
 */
export function playMusic(name: string): void {
  if (!soundEnabled || musicMuted) return;
  if (name === currentMusic) return;
  currentMusic = name;

  // Cancel any in-progress fade-in; we will handle the element below.
  stopFadeIn();

  const isStop = name === 'NONE' || name === '';
  const fadeOutDuration = isStop ? MUSIC_FADE_STOP_MS : MUSIC_FADE_OUT_MS;

  if (musicElement) {
    // Move the playing element to the "fading out" slot, then fade it out.
    stopFadeOut();  // discard any previous fading-out element
    fadingOutElement = musicElement;
    musicElement = null;
    const el = fadingOutElement;
    const trackName = name;  // capture for the async callback
    fadeOutElement(el, fadeOutDuration, () => {
      if (!isStop) startTrack(trackName, true);
    });
  } else {
    // Nothing currently playing (may have been mid-fade-out); cancel that and
    // start the new track immediately without fading in.
    stopFadeOut();
    if (!isStop) startTrack(name, false);
  }
}

// ---------------------------------------------------------------------------
// Configuration helpers
// ---------------------------------------------------------------------------

/** Enable or disable all sound. */
export function setSoundEnabled(enabled: boolean): void {
  soundEnabled = enabled;
  if (!enabled) {
    stopAll();
  }
}

/** Set music volume (0–100). */
export function setMusicVolume(vol: number): void {
  musicVolume = vol;
  if (musicElement) {
    musicElement.volume = Math.min(vol, 100) / 100 * MUSIC_VOLUME_CAP;
  }
}

/** Stop all sound effects and music. */
export function stopAll(): void {
  stopFadeIn();
  stopFadeOut();
  if (musicElement) {
    musicElement.pause();
    musicElement.src = '';
    musicElement = null;
  }
  currentMusic = '';
}

// ---------------------------------------------------------------------------
// Mute controls
// ---------------------------------------------------------------------------

/** Return whether music is currently muted. */
export function getMusicMuted(): boolean {
  return musicMuted;
}

/** Return whether sound effects are currently muted. */
export function getSfxMuted(): boolean {
  return sfxMuted;
}

/**
 * Mute or unmute background music.
 * Persists the setting and immediately pauses or restarts playback.
 */
export function setMusicMuted(muted: boolean): void {
  musicMuted = muted;
  saveConfig('musicMuted', muted);
  if (muted) {
    // Cancel any in-progress fades and stop all music immediately.
    stopFadeIn();
    stopFadeOut();
    if (musicElement) {
      musicElement.pause();
      musicElement.src = '';
      musicElement = null;
    }
  } else {
    // Resume the track that was playing before muting.
    if (currentMusic && currentMusic !== 'NONE') {
      const nameToResume = currentMusic;
      currentMusic = ''; // force playMusic to re-start it
      playMusic(nameToResume);
    }
  }
}

/**
 * Mute or unmute sound effects.
 * Persists the setting; takes effect on the next sound played.
 */
export function setSfxMuted(muted: boolean): void {
  sfxMuted = muted;
  saveConfig('sfxMuted', muted);
}
