/**
 * cmd_notifications.ts — Handlers for quest and knowledge notification commands.
 *
 * Handles:
 *   addquest  — server sends quest data at login or when new quests start
 *   updquest  — server updates an existing quest's completion/step
 *   addknowledge — server sends knowledge items at login or when new knowledge is gained
 */

import { BinaryReader } from './binary_reader.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { gameEvents } from './events.js';

// ── Quest ──────────────────────────────────────────────────────────────────

export interface Quest {
  code: number;
  title: string;
  face: number;
  replay: boolean;
  parent: number;
  end: boolean;
  step: string;
}

/** All known quests, keyed by quest code. */
export const quests: Map<number, Quest> = new Map();

export function clearNotifications(): void {
  quests.clear();
  knowledgeItems.clear();
  knowledgeTypeInfos.clear();
}

export function AddQuestCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  try {
    while (reader.remaining >= 4) {
      const code = reader.readUint32();
      const title = reader.readL2String();
      const face = reader.readInt32();
      const replay = reader.readUint8() !== 0;
      const parent = reader.readInt32();
      const end = reader.readUint8() !== 0;
      const step = reader.readL2String();
      quests.set(code, { code, title, face, replay, parent, end, step });
    }
  } catch (e) {
    LOG(LogLevel.Warning, 'AddQuestCmd', `Parse error: ${e}`);
  }
  gameEvents.emit('questUpdate');
}

export function UpdQuestCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  try {
    const code = reader.readUint32();
    const end = reader.readUint8() !== 0;
    const step = reader.readL2String();
    const existing = quests.get(code);
    if (existing) {
      existing.end = end;
      existing.step = step;
    } else {
      LOG(LogLevel.Warning, 'UpdQuestCmd', `updquest for unknown code ${code}`);
    }
  } catch (e) {
    LOG(LogLevel.Warning, 'UpdQuestCmd', `Parse error: ${e}`);
  }
  gameEvents.emit('questUpdate');
}

// ── Knowledge ──────────────────────────────────────────────────────────────

export interface KnowledgeItem {
  code: number;
  type: string;
  title: string;
  face: number;
}

export interface KnowledgeTypeInfo {
  type: string;
  displayName: string;
  face: number;
  attempt: boolean;
}

/** All known knowledge items, keyed by knowledge code. */
export const knowledgeItems: Map<number, KnowledgeItem> = new Map();

/** Knowledge type metadata from the knowledge_info replyinfo, keyed by type string. */
export const knowledgeTypeInfos: Map<string, KnowledgeTypeInfo> = new Map();

/**
 * Parse the text body of the knowledge_info replyinfo reply.
 * Format: one line per type — `type:display name:face number:attempt`
 * The first line has empty type/name and gives the generic-type face.
 */
export function parseKnowledgeInfo(text: string): void {
  for (const line of text.split('\n')) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    const parts = trimmed.split(':');
    if (parts.length < 4) continue;
    const type = parts[0]!;
    const displayName = parts[1]!;
    const face = parseInt(parts[2]!, 10);
    const attempt = parts[3] === '1';
    knowledgeTypeInfos.set(type, { type, displayName, face, attempt });
  }
  gameEvents.emit('knowledgeUpdate');
}

export function AddKnowledgeCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  try {
    while (reader.remaining >= 4) {
      const code = reader.readUint32();
      const type = reader.readL2String();
      const title = reader.readL2String();
      const face = reader.readInt32();
      knowledgeItems.set(code, { code, type, title, face });
    }
  } catch (e) {
    LOG(LogLevel.Warning, 'AddKnowledgeCmd', `Parse error: ${e}`);
  }
  gameEvents.emit('knowledgeUpdate');
}
