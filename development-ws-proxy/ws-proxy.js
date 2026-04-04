const WebSocket = require('ws');
const net = require('net');

// --- Argument parsing ---

const DEFAULT_HOST = 'birdsnest.sytes.net';
const DEFAULT_PORT = 13327;
const DEFAULT_LISTEN_PORT = 13327;

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

for (let i = 0; i < args.length; i++) {
  const arg = args[i];
  if (arg === '--host') {
    host = args[++i];
  } else if (arg === '--port') {
    port = parseInt(args[++i], 10);
  } else if (arg === '--listen-port') {
    listenPort = parseInt(args[++i], 10);
  } else if (arg === '--log') {
    logFrames = true;
  } else if (arg === '--max-length') {
    maxLength = parseInt(args[++i], 10);
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
    const byte = payload[i];
    if (byte >= 0x20 && byte <= 0x7e) {
      result += String.fromCharCode(byte);
    } else {
      result += `\\x${byte.toString(16).padStart(2, '0')}`;
    }
    if (result.length >= maxLength) {
      result = result.slice(0, maxLength) + '...';
      break;
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
