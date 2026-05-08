# Protocol Implementation Plan

This document compares the Crossfire protocol specification (`docs/protocol.txt`,
targeting CS version 1023 and SC version 1030) against the current web-client
implementation and lists what is missing or incomplete.

---

## 1. Missing S→C (server-to-client) command handlers

These commands are defined in the protocol but are absent from the dispatch
table in `src/lib/commands.ts`.

| Command        | Protocol section | Notes                                                                                    |
| -------------- | ---------------- | ---------------------------------------------------------------------------------------- |
| `addquest`     | Knowledge/Quests | Sent when `notifications ≥ 1` is set in setup; notifies the client of new/ongoing quests |
| `updquest`     | Knowledge/Quests | Sent when a quest step changes (also requires `notifications ≥ 1`)                       |
| `addknowledge` | Knowledge/Quests | Sent when `notifications ≥ 2`; notifies the client of newly learnt knowledge items       |

All three require the `notifications` setup option to be negotiated first
(see §3 below).

---

## 2. Missing C→S (client-to-server) commands

### 2a. Account-based login

The current login flow uses the legacy `addme` path. The account-based login
system (`loginmethod 1/2`) is not implemented.

| Command            | Direction | Notes                                                                           |
| ------------------ | --------- | ------------------------------------------------------------------------------- |
| `accountlogin`     | C→S       | Log in with an existing account (`<name><password>` as lstrings)                |
| `accountnew`       | C→S       | Create a new account (`<name><password>` as lstrings)                           |
| `accountplay`      | C→S       | Select a character to play from the account's character list                    |
| `accountaddplayer` | C→S       | Associate an existing character with the logged-in account                      |
| `accountpw`        | C→S       | Change the account password                                                     |
| `createplayer`     | C→S       | Create a new character (requires race/class/stat data gathered via requestinfo) |

Implementing the account login flow also requires:

- Sending `loginmethod 1` (or `2` for full character creation) in the `setup` command.
- Correctly parsing the binary `accountplayers` response — see §5 below.

### 2d. Image / smoothing requests

| Command     | Direction | Notes                                                                                                                                       |
| ----------- | --------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `askface`   | C→S       | Request the server send face data for a specific face number; needed when `facecache 1` is set and a face is not found in the browser cache |
| `asksmooth` | C→S       | Request smoothing data for a specific face number                                                                                           |

`Face2Cmd` currently loads from the browser cache but silently drops the face
if the cache misses — it should fall back to sending `askface` to the server.

---

## 3. Setup options not negotiated

`clientNegotiate()` in `src/lib/client.ts` does not send several options that
unlock additional server features.

| Option             | Value            | Effect                                                                                                                      |
| ------------------ | ---------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `loginmethod`      | `1` or `2`       | Enables account-based login (`1`) and full character creation (`2`)                                                         |
| `notifications`    | `1`, `2`, or `3` | Level 1 enables quest notifications; level 2 adds knowledge; level 3 adds character-status flags                            |
| `extended_stats`   | `1`              | Makes the server send `CS_STAT_RACE_*`, `CS_STAT_BASE_*`, and `CS_STAT_APPLIED_*` values (already defined in `protocol.ts`) |
| `num_look_objects` | integer          | Controls the maximum number of objects shown in the ground view                                                             |

---

## 4. Stats not parsed in `StatsCmd`

`src/lib/cmd_stats.ts` does not handle the extended stat IDs that are sent
when `extended_stats 1` is negotiated. The constants are already defined in
`src/lib/protocol.ts`.

| Stat range | Constants                                     | Description                    |
| ---------- | --------------------------------------------- | ------------------------------ |
| 32–38      | `CS_STAT_RACE_STR` … `CS_STAT_RACE_POW`       | Per-race stat modifiers        |
| 39–45      | `CS_STAT_BASE_STR` … `CS_STAT_BASE_POW`       | Base (unmodified) stat values  |
| 46–52      | `CS_STAT_APPLIED_STR` … `CS_STAT_APPLIED_POW` | Applied (gear/skill) modifiers |

All of these are int16 values like the primary stats. The `Stats` type and
display layer would also need updating to expose them.

---

## 5. `accountplayers` parsing bug

`AccountPlayersCmd` in `src/lib/commands.ts` parses the payload as newline-
and colon-delimited text. The protocol specifies a binary format:

```
<num_characters: int8>
  for each character:
    repeated: <len: int8> <type: int8(ACL_*)> <value>
    terminated by: <len=0>
```

Where `ACL_LEVEL` and `ACL_FACE_NUM` are 16-bit binary values and all other
values are strings. The current parser would work only if the server happens
to format the payload as text (which some server versions may do), but it
should be replaced with a correct binary parser to conform to the specification.

---

## 6. `requestinfo` types not requested or handled

`clientNegotiate()` requests `skill_info`, `exp_table`, `motd`, `news`, and
`rules`. Several other info types are never requested, and `ReplyInfoCmd`
does not handle their responses.

### 6a. Image pre-loading

| Type                        | Notes                                                                                                                           |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| `image_info`                | Already handled in `ReplyInfoCmd` → `getImageInfo()` but never requested during negotiation                                     |
| `image_sums <start> <stop>` | Already handled in `ReplyInfoCmd` → `getImageSums()` but never requested; blocks of ≤ 1000 images must be requested iteratively |

### 6b. Gameplay metadata

| Type             | Notes                                                                                                           |
| ---------------- | --------------------------------------------------------------------------------------------------------------- |
| `spell_paths`    | List of spell path names and their bitmask values; needed to display attuned/repelled/denied paths meaningfully |
| `knowledge_info` | List of knowledge types; needed before `addknowledge` can be displayed correctly                                |
| `skill_extra`    | Extended skill descriptions (level `1`); optional but useful for the skill list UI                              |

### 6c. Character creation (requires `loginmethod 2`)

| Type                 | Notes                                                                                       |
| -------------------- | ------------------------------------------------------------------------------------------- |
| `newcharinfo`        | Describes what fields are required in `createplayer` and how many stat points are available |
| `race_list`          | Returns the list of playable race archetype names                                           |
| `race_info <race>`   | Returns name, description, stat adjustments, and choices for a specific race                |
| `class_list`         | Returns the list of playable class archetype names                                          |
| `class_info <class>` | Same structure as `race_info` but for classes                                               |
| `startingmap`        | Returns available starting map choices                                                      |

---

## 7. `AccountPlayersCmd` event not wired into the login UI

`AccountPlayersCmd` emits an `accountPlayers` event, but `Login.svelte` does
not subscribe to it. When account-based login is implemented, the login
screen must:

1. Display the character list received in `accountplayers`.
2. Allow the player to select a character and send `accountplay`.
3. Optionally offer "new character" and "add existing character" flows.

---

## 8. Deprecated commands still sent (minor)

The following options are sent in `clientNegotiate()` but are marked as
deprecated in the protocol and ignored by current servers. They are harmless
but could be removed for cleanliness.

| Option                | Reason deprecated                |
| --------------------- | -------------------------------- |
| `extendedTextInfos 1` | Server always uses `drawextinfo` |
| `newmapcmd 1`         | Server always supports `newmap`  |

---

## Summary / Suggested implementation order

2. **`askface` fallback** — complete the face-caching loop so missing faces
   are fetched from the server instead of silently dropped.
3. **Extended stats** — negotiate `extended_stats 1`, parse stat IDs 32–52,
   and expose them in the UI (ProtectionList / StatsPanel).
4. **`notifications` + quest/knowledge commands** — negotiate `notifications 3`,
   add `addquest`, `updquest`, `addknowledge` handlers and a quest-log UI.
5. **`inscribe`** — small command, completes item-manipulation coverage.
6. **`asksmooth`** — small command for smooth-data requests.
7. **`spell_paths` / `knowledge_info` requestinfo** — improve existing UI panels.
8. **Account-based login** — larger feature; fix `accountplayers` binary parser,
   implement `accountlogin`, `accountnew`, `accountplay`, character-selection
   screen, `accountpw`, and ultimately `createplayer` with the full
   race/class/stat selection flow.
