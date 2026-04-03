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
cd development-ws-proxy
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
│   ├── components/       # Svelte UI components
│   ├── assets/           # Static assets (pixmaps)
│   ├── main.ts           # Entry point
│   └── app.css           # Global styles
├── public/               # Public static files
├── old/                  # Original C/GTK source (for reference)
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
- The client connects to a fixed server address
