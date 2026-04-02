# Crossfire Web Client - Instructions

## Overview

This project is a web-based Crossfire game client, converted from a C/GTK application
to a TypeScript/Svelte web application using Vite as the build tool.

## Prerequisites

- Node.js 18+ (recommended: 20+)
- npm 9+

## Development

### Install dependencies

```bash
npm install
```

### Run development server

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

This runs the Svelte type checker. For additional type checking:

```bash
npx tsc --noEmit
```

## Deployment

1. Run `npm run build` to produce the `dist/` directory.
2. Serve the contents of `dist/` with any static file server (nginx, Apache, Caddy, GitHub Pages, Netlify, Vercel, etc.).
3. Ensure the Crossfire server allows WebSocket connections from the client's origin (CORS).

## Project Structure

```
├── src/                  # Source code
│   ├── lib/              # Core library (converted from C common code)
│   │   ├── client.ts     # Main client logic
│   │   ├── commands.ts   # Server command handlers
│   │   ├── image.ts      # Image processing
│   │   ├── init.ts       # Initialization
│   │   ├── item.ts       # Item management
│   │   ├── mapdata.ts    # Map data handling
│   │   ├── misc.ts       # Utilities
│   │   ├── newsocket.ts  # WebSocket communication
│   │   ├── p_cmd.ts      # Player commands
│   │   ├── player.ts     # Player operations
│   │   ├── keys.ts       # Key bindings (def-keys)
│   │   ├── protocol.ts   # Protocol constants
│   │   └── storage.ts    # Client-side storage
│   ├── components/       # Svelte UI components
│   │   ├── App.svelte
│   │   ├── Map.svelte
│   │   ├── Inventory.svelte
│   │   ├── Stats.svelte
│   │   ├── Info.svelte
│   │   ├── Spells.svelte
│   │   ├── Skills.svelte
│   │   ├── Login.svelte
│   │   ├── CreateChar.svelte
│   │   └── MenuBar.svelte
│   ├── assets/           # Static assets (pixmaps)
│   ├── main.ts           # Entry point
│   └── app.css           # Global styles
├── public/               # Public static files
├── old/                  # Original C/GTK source (for reference)
├── AUTHORS               # Project contributors
├── INSTRUCTIONS.md       # This file
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
- **Keybindings** are available via the `getDefaultKeyBindings()` function.
- The client connects to a fixed server address (no metaserver).
