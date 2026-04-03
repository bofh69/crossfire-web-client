const WebSocket = require('ws');
const net = require('net');

const wss = new WebSocket.Server({ port: 13327 });

wss.on('connection', (ws) => {
  const socket = net.createConnection({ host: 'birdsnest.sytes.net', port: 13327 });
  let buffer = Buffer.alloc(0);

  // TCP → WebSocket: strip length header, send as frames
  socket.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);
    
    while (buffer.length >= 2) {
      const len = buffer.readUInt16BE(0);
      if (buffer.length < len + 2) break;
      
      const payload = buffer.slice(2, len + 2);
      ws.send(payload);
      buffer = buffer.slice(len + 2);
    }
  });

  // WebSocket → TCP: add length header, send as frames
  ws.on('message', (msg) => {
    const payload = Buffer.isBuffer(msg) ? msg : Buffer.from(msg);
    const frame = Buffer.alloc(payload.length + 2);
    frame.writeUInt16BE(payload.length, 0);
    payload.copy(frame, 2);
    socket.write(frame);
  });

  socket.on('close', () => ws.close());
  ws.on('close', () => socket.destroy());
  socket.on('error', (err) => console.error(err));
});

console.log('Proxy listening on ws://localhost:13327');
