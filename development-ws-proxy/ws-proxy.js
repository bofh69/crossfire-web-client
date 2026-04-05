const WebSocket = require('ws');
const net = require('net');

// --- Argument parsing ---

const DEFAULT_HOST = 'birdsnest.sytes.net';
const DEFAULT_PORT = 13327;
const DEFAULT_LISTEN_PORT = 13328;

function printUsage() {
  console.log(`Usage: node ws-proxy.js [options]

Options:
  --host <host>          Target TCP host (default: ${DEFAULT_HOST})
  --port <port>          Target TCP port (default: ${DEFAULT_PORT})
  --listen-port <port>   WebSocket server listen port (default: ${DEFAULT_LISTEN_PORT})
  --log                  Log frames passing through the proxy
  --max-length <n>       Maximum characters to log per frame (default: unlimited)
  --help                 Show this help message
`);
}

const args = process.argv.slice(2);
let host = DEFAULT_HOST;
let port = DEFAULT_PORT;
let listenPort = DEFAULT_LISTEN_PORT;
let logFrames = false;
let maxLength = Infinity;

function requireValue(arg, value) {
  if (value === undefined) {
    console.error(`Error: ${arg} requires a value`);
    printUsage();
    process.exit(1);
  }
  return value;
}

function requireInt(arg, value) {
  const str = requireValue(arg, value);
  const n = parseInt(str, 10);
  if (isNaN(n)) {
    console.error(`Error: ${arg} requires an integer value, got: ${str}`);
    printUsage();
    process.exit(1);
  }
  return n;
}

for (let i = 0; i < args.length; i++) {
  const arg = args[i];
  if (arg === '--host') {
    host = requireValue(arg, args[++i]);
  } else if (arg === '--port') {
    port = requireInt(arg, args[++i]);
  } else if (arg === '--listen-port') {
    listenPort = requireInt(arg, args[++i]);
  } else if (arg === '--log') {
    logFrames = true;
  } else if (arg === '--max-length') {
    maxLength = requireInt(arg, args[++i]);
  } else if (arg === '--help') {
    printUsage();
    process.exit(0);
  } else {
    console.error(`Unknown argument: ${arg}`);
    printUsage();
    process.exit(1);
  }
}

// --- Frame logging ---

function formatPayload(payload) {
  let result = '';
  for (let i = 0; i < payload.length; i++) {
    if (result.length >= maxLength) {
      result += '...';
      break;
    }
    const byte = payload[i];
    if (byte >= 0x20 && byte <= 0x7e) {
      result += String.fromCharCode(byte);
    } else {
      result += `\\x${byte.toString(16).padStart(2, '0')}`;
    }
  }
  return result;
}

function logFrame(direction, payload) {
  if (!logFrames) return;
  console.log(`${direction} [${payload.length} bytes] ${formatPayload(payload)}`);
}

// --- Proxy server ---

const wss = new WebSocket.Server({ port: listenPort });

wss.on('connection', (ws) => {
  const socket = net.createConnection({ host, port });
  let buffer = Buffer.alloc(0);

  // TCP → WebSocket: strip length header, send as frames
  socket.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);

    while (buffer.length >= 2) {
      const len = buffer.readUInt16BE(0);
      if (buffer.length < len + 2) break;

      const payload = buffer.slice(2, len + 2);
      logFrame('←', payload);
      ws.send(payload);
      buffer = buffer.slice(len + 2);
    }
  });

  // WebSocket → TCP: add length header, send as frames
  ws.on('message', (msg) => {
    const payload = Buffer.isBuffer(msg) ? msg : Buffer.from(msg);
    logFrame('→', payload);
    const frame = Buffer.alloc(payload.length + 2);
    frame.writeUInt16BE(payload.length, 0);
    payload.copy(frame, 2);
    socket.write(frame);
  });

  socket.on('close', () => ws.close());
  ws.on('close', () => socket.destroy());
  socket.on('error', (err) => console.error(err));
});

console.log(`Proxy listening on ws://localhost:${listenPort} → ${host}:${port}`);
