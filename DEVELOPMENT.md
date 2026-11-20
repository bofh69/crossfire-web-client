# Developing the Crossfire Web Client

## Overview

This project is a web-based Crossfire game client, converted from a C/GTK application
to a TypeScript/Svelte web application using Vite as the build tool.

## Prerequisites

- Node.js 20.19+ or 22.12+ (CI uses Node 22). Vite 8 and its Svelte
  plugin don't run on older versions.
- npm 10+ (comes with those Node.js versions)

## Development

### Get the source

The sound files are in a git submodule, so clone with `--recursive`:

```bash
git clone --recursive https://github.com/bofh69/crossfire-web-client.git
cd crossfire-web-client
```

In an existing clone, run `git submodule update --init` to get them.

### Install dependencies

```bash
npm install
```

### Running while developing

```bash
cd scripts/development-ws-proxy
npm install
node ws-proxy.js
```

That starts a proxy between a websocket on localhost and
a crossfire server.

In another shell run:

```bash
npm run dev
```

This starts a local development server (usually at `http://localhost:5173/`)
with hot module replacement (HMR).

### Build for production

```bash
npm run build
```

The output will be placed in the `dist/` directory.

### Preview production build

```bash
npm run preview
```

## Testing

```bash
npm run check
```

This runs the Svelte type checker, the TypeScript checks for the
scripts and the unit tests. See also
[Mapdata replay tests](#mapdata-replay-tests). For additional type
checking:

```bash
npx tsc --noEmit
```

## Deployment

1. Run `npm run build` to produce the `dist/` directory.
2. Serve the contents of `dist/` with any static file server (nginx, Apache, Caddy, GitHub Pages, Netlify, Vercel, etc.).

`npm run build:singlefile` builds `dist-singlefile/index.html`, with
everything except the sounds in one file.

See [INSTALL.md](INSTALL.md) for how to set up a server with nginx.

## Project Structure

```
├── src/                  # Source code
│   ├── lib/              # Core library (converted from C common code)
│   ├── components/       # Svelte UI components
│   ├── assets/           # Static assets (pixmaps)
│   ├── main.ts           # Entry point
│   └── app.css           # Global styles
├── public/               # Public static files
├── index.html            # HTML entry point
├── package.json
├── tsconfig.json
├── svelte.config.js
└── vite.config.ts
```

## Architecture Notes

- **WebSocket** is used instead of raw TCP sockets for server communication.
- **localStorage/IndexedDB** is used instead of file I/O for caching and configuration.
- **Console logging** replaces stdout/stderr output.

## WebSockets and the server

Web pages can't use raw TCP sockets so WebSockets has to be used.

The public crossfire servers don't support WebSockets, so a WebSocket
proxy has to be used when connecting to them. There is one included in
the repo (under scripts/) that also handles crossfire's protocol's
length header.

The [crossfire-server](https://github.com/bofh69/crossfire-server) fork
has a branch with built in support for WebSockets.
It is not thoroughly reviewed yet, but it works well. The same
changes are submitted upstream as
[patch #471](https://sourceforge.net/p/crossfire/patches/471/).

## Recording and replaying sessions

The client can record a game session: everything it sends to and receives
from the server, plus the keys pressed and some UI actions. A recording
can later be played back without a server. The client code processes the
recorded server messages again, so a bug that showed up while playing can
be reproduced and stepped through as many times as needed.

Recordings are used in three ways:

- to debug a problem on the [replay page](#the-replay-page)
- to take screenshots automatically with
  [Playwright](#automated-replay-with-playwright)
- as [mapdata replay tests](#mapdata-replay-tests), which check that the
  map looks right at a given point in a recording

### Recording a session

1. Add `?record` to the client's URL, e.g.
   `http://localhost:5173/?record`. With another query parameter, use
   `&record` instead.
2. Log in and play. Recording starts when the page loads.
3. When something interesting happens, choose **Connection → Add recording
   marker** and type a short description, e.g. `Dragon is in fog`. This
   adds a `MARK` line to the log, which the replay tools can jump to. Add
   the marker right when the problem is visible.
4. Optionally choose **Connection → Stop recording**.
5. Choose **Connection → Download recording log**. The browser saves a file
   called `crossfire-ws-recording-<date>_<time>.log`.

The recording menu items are only shown when `?record` is in the URL.

The log has everything the client sent, including the account password
from the login. Use a test account, or remove the login lines before
sharing a log.

### Log format

The downloaded logs are in a compact format, one entry per line:

```
<timestamp>\t<TX|RX>\t<byte_length>\t<base64_payload>
<timestamp>\t<MARK>\t<json_marker_text>
<timestamp>\t<KEY|UI>\t<json_payload>
```

`<timestamp>` is milliseconds since recording started. `TX` lines were
sent by the client and `RX` lines were received from the server. `KEY`
lines are key presses, and `UI` lines are UI actions, the browser window
size and a snapshot of the client's configuration.

The payloads are base64 encoded, so they are hard to read. Convert a log
to readable text with:

```bash
npm run recording:convert -- input.log output.txt
```

The converted format is:

```
timestamp TX/RX/MARK/KEY/UI text
```

where `text` is C-string escaped (`\n` for newline, `\xXX` for
non-printables). If `output.txt` is omitted, the converted output is
written to stdout.

All the replay tools accept both formats.

### The replay page

The replay page plays back a log in the browser, without a crossfire
server. Open `/replay.html`, e.g. `http://localhost:5173/replay.html`
with `npm run dev`, and pick a log file.

The page restores the configuration from the log, then lets you step
through it:

- **Step cmd** (`S`) plays the log up to the next command.
- **Next mark** (`N`) plays up to the next `MARK`, and the list of marks
  jumps straight to one.
- **Next map2** (`2`) plays up to the next map update from the server.
- **Start over** (`R`) goes back to the beginning.
- **Mark** (`M`) adds a `MARK` at the current position. Save the
  changed log with **Download log**.
- **Export cell state** and **Export selected cell state** save the map
  state as a state file for the
  [mapdata replay tests](#mapdata-replay-tests).

### Automated replay with Playwright

To replay a log in a real browser and take screenshots, first install
Playwright's Chromium once:

```bash
npx playwright install chromium
```

Then run:

```bash
npm run replay:playwright -- --log tests/replay-mapdata/logs/scorn.converted.log --mark-regex ".*"
```

This command starts:

- a local Vite dev server
- a replay WebSocket server that replays the logfile to the client
- a Playwright Chromium browser connected to that replay server

Behavior:

- For each `TX` line in the logfile, replay waits for the client to send a matching command (or timeout).
- Matching is command-only, except for `ncom` where the embedded command text is also matched (`ncom.north`, `ncom.apply`, etc.).
- The next replayed `RX comc` packet is sequence-patched to the sequence sent by the client for that matched `ncom`.
- `KEY` and `UI` lines replay recorded keyboard and UI interactions.
- Every `MARK` line whose label matches `--mark-regex` saves a screenshot in `screenshots/replay-playwright/`.

#### Optional JSON instructions

Use `--instructions` to apply Playwright actions synchronized to `TX` lines:

```bash
npm run replay:playwright -- \
  --log tests/replay-mapdata/logs/scorn.converted.log \
  --instructions scripts/replay-playwright.instructions.example.json \
  --mark-regex ".*"
```

Instruction format (`JSON`):

- top-level `tx` array
- each entry has:
  - `match`: regex string tested against replay command key
    - normal commands: `accountlogin`, `accountplay`
    - `ncom` commands: `ncom.<embedded command>`, e.g. `ncom.north`
  - `actions`: list of actions (`clickRole`, `clickText`, `fillRole`, `clickSelector`, `fillSelector`, `press`, `keyDown`, `keyUp`, `waitForTimeout`, `screenshot`)
    - Action string values support regex captures from `match` via `$1`, `$2`, etc.

Built-in actions already exist for `accountlogin` (click **Log In**) and
`accountplay` (click matching character button), and can be extended with
instruction rules.

### Mapdata replay tests

The mapdata replay tests replay a log up to a mark, then compare the map
state with a saved state file. Run the mapdata replay tests with:

```bash
npm run test:mapdata
```

Test cases are listed in:

```
tests/replay-mapdata/tests.json
```

Each case has:

- `name`: test name
- `log`: replay log file path (in compact or converted format)
- `mark`: MARK label to replay the log up to
- `state`: path to expected mapdata state file

Only cells listed in the state file are compared.

To add a test:

1. [Record a session](#recording-a-session) with a marker where the map
   should be checked, and save the log in `tests/replay-mapdata/logs/`.
2. Open the log on the [replay page](#the-replay-page) and step to the
   marker.
3. Check that the map looks right, then use **Export cell state**, or
   select a tile and use **Export selected cell state** to only check that
   cell. Save the file in `tests/replay-mapdata/states/`.
4. Add the case to `tests/replay-mapdata/tests.json`.
