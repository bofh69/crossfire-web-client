# ws-proxy for crossfire

This proxy translates the WebSocket protocol to the crossfire protocol,
allowing you to connect to crossfire servers that do not support WebSockets.

# Install & Run

```sh
npm install
node ws-proxy.js
```

It will map ws://localhost:13327 to birdsnest.sytes.net:13327

Change the port and host as needed in the ws-proxy.js file.


