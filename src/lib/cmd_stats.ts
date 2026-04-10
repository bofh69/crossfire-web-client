/**
 * cmd_stats.ts — Stats-related command handlers.
 * Extracted from commands.ts.
 */

import {
  CS_STAT_HP, CS_STAT_MAXHP, CS_STAT_SP, CS_STAT_MAXSP,
  CS_STAT_STR, CS_STAT_INT, CS_STAT_WIS, CS_STAT_DEX, CS_STAT_CON, CS_STAT_CHA,
  CS_STAT_LEVEL, CS_STAT_WC, CS_STAT_AC, CS_STAT_DAM, CS_STAT_ARMOUR,
  CS_STAT_SPEED, CS_STAT_FOOD, CS_STAT_WEAP_SP, CS_STAT_RANGE, CS_STAT_TITLE,
  CS_STAT_POW, CS_STAT_GRACE, CS_STAT_MAXGRACE, CS_STAT_FLAGS, CS_STAT_WEIGHT_LIM,
  CS_STAT_EXP64, CS_STAT_SPELL_ATTUNE, CS_STAT_SPELL_REPEL, CS_STAT_SPELL_DENY,
  CS_STAT_RESIST_START, CS_STAT_RESIST_END, CS_STAT_SKILLINFO, CS_NUM_SKILLS,
  CS_STAT_GOLEM_HP, CS_STAT_GOLEM_MAXHP,
  type Stats,
} from './protocol.js';
import { BinaryReader } from './binary_reader.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { gameEvents } from './events.js';

/** Current player stats */
export const playerStats: Stats = {
  Str: 0, Dex: 0, Con: 0, Wis: 0, Cha: 0, Int: 0, Pow: 0,
  wc: 0, ac: 0, level: 0, hp: 0, maxhp: 0, sp: 0, maxsp: 0,
  grace: 0, maxgrace: 0, exp: BigInt(0), food: 0, dam: 0,
  speed: 0, weaponSp: 0, attuned: 0, repelled: 0, denied: 0,
  flags: 0, resists: new Array(30).fill(0), resistChange: false,
  skillLevel: new Array(CS_NUM_SKILLS).fill(0),
  skillExp: new Array(CS_NUM_SKILLS).fill(BigInt(0)),
  weightLimit: 0,
  golemHp: 0, golemMaxhp: 0,
  range: '',
  title: '',
};

/**
 * Skill names populated from the server's skill_info reply.
 * Index i corresponds to CS_STAT_SKILLINFO + i.
 */
export const skillNames: string[] = new Array(CS_NUM_SKILLS).fill('');

/**
 * Experience table populated from the server's exp_table reply.
 * expTable[level] is the total experience required to reach that level.
 * expTable[0] = 0n (level 0 requires 0 exp, used as base for level 1).
 * expTable[1] is the exp needed for level 1, expTable[2] for level 2, etc.
 */
export const expTable: bigint[] = [];

/**
 * Compute the 0–100 progress percentage of `exp` within the current level band.
 * Returns 100 if the player is at the maximum known level.
 */
export function expBarPercent(exp: bigint, level: number): number {
  if (expTable.length < 2 || level <= 0) return 0;
  const curIdx = Math.min(level, expTable.length - 1);
  const curLevelExp = expTable[curIdx];
  if (curIdx + 1 >= expTable.length) return 100; // at or beyond max level
  const nextLevelExp = expTable[curIdx + 1];
  if (nextLevelExp <= curLevelExp) return 100;
  const ratio = Number(exp - curLevelExp) / Number(nextLevelExp - curLevelExp);
  return Math.max(0, Math.min(100, ratio * 100));
}

export function StatsCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  while (reader.remaining > 0) {
    const stat = reader.readUint8();
    switch (stat) {
      case CS_STAT_HP: playerStats.hp = reader.readInt16(); break;
      case CS_STAT_MAXHP: playerStats.maxhp = reader.readInt16(); break;
      case CS_STAT_SP: playerStats.sp = reader.readInt16(); break;
      case CS_STAT_MAXSP: playerStats.maxsp = reader.readInt16(); break;
      case CS_STAT_GRACE: playerStats.grace = reader.readInt16(); break;
      case CS_STAT_MAXGRACE: playerStats.maxgrace = reader.readInt16(); break;
      case CS_STAT_STR: playerStats.Str = reader.readInt16(); break;
      case CS_STAT_INT: playerStats.Int = reader.readInt16(); break;
      case CS_STAT_WIS: playerStats.Wis = reader.readInt16(); break;
      case CS_STAT_DEX: playerStats.Dex = reader.readInt16(); break;
      case CS_STAT_CON: playerStats.Con = reader.readInt16(); break;
      case CS_STAT_CHA: playerStats.Cha = reader.readInt16(); break;
      case CS_STAT_POW: playerStats.Pow = reader.readInt16(); break;
      case CS_STAT_LEVEL: playerStats.level = reader.readInt16(); break;
      case CS_STAT_WC: playerStats.wc = reader.readInt16(); break;
      case CS_STAT_AC: playerStats.ac = reader.readInt16(); break;
      case CS_STAT_DAM: playerStats.dam = reader.readInt16(); break;
      case CS_STAT_ARMOUR: playerStats.resists[0] = reader.readInt16(); break;
      case CS_STAT_SPEED: playerStats.speed = reader.readInt32(); break;
      case CS_STAT_FOOD: playerStats.food = reader.readInt16(); break;
      case CS_STAT_WEAP_SP: playerStats.weaponSp = reader.readInt32(); break;
      case CS_STAT_FLAGS: playerStats.flags = reader.readInt16(); break;
      case CS_STAT_WEIGHT_LIM: playerStats.weightLimit = reader.readInt32(); break;
      case CS_STAT_EXP64: playerStats.exp = reader.readInt64(); break;
      case CS_STAT_SPELL_ATTUNE: playerStats.attuned = reader.readInt32(); break;
      case CS_STAT_SPELL_REPEL: playerStats.repelled = reader.readInt32(); break;
      case CS_STAT_SPELL_DENY: playerStats.denied = reader.readInt32(); break;
      case CS_STAT_GOLEM_HP: playerStats.golemHp = reader.readInt32(); break;
      case CS_STAT_GOLEM_MAXHP: playerStats.golemMaxhp = reader.readInt32(); break;
      case CS_STAT_RANGE: {
        // 1-byte length prefix + string (not null-terminated)
        const rlen = reader.readUint8();
        playerStats.range = reader.readString(rlen);
        break;
      }
      case CS_STAT_TITLE: {
        // 1-byte length prefix + string (not null-terminated)
        const tlen = reader.readUint8();
        let titleStr = reader.readString(tlen);
        if (titleStr.startsWith('Player: ')) {
          titleStr = titleStr.slice('Player: '.length);
        }
        playerStats.title = titleStr;
        break;
      }
      default:
        if (stat >= CS_STAT_RESIST_START && stat <= CS_STAT_RESIST_END) {
          playerStats.resists[stat - CS_STAT_RESIST_START] = reader.readInt16();
          playerStats.resistChange = true;
        } else if (stat >= CS_STAT_SKILLINFO && stat < CS_STAT_SKILLINFO + CS_NUM_SKILLS) {
          const skillIdx = stat - CS_STAT_SKILLINFO;
          playerStats.skillLevel[skillIdx] = reader.readUint8();
          playerStats.skillExp[skillIdx] = reader.readInt64();
        } else {
          LOG(LogLevel.Warning, 'StatsCmd', `Unknown stat ${stat}`);
          return; // Can't continue - unknown length
        }
        break;
    }
  }
  gameEvents.emit('statsUpdate', playerStats);
}
