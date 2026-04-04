# ws-proxy for crossfire

This proxy translates the WebSocket protocol to the crossfire protocol,
allowing you to connect to crossfire servers that do not support WebSockets.

# Install & Run

```sh
npm install
node ws-proxy.js [options]
```

By default it maps ws://localhost:13327 to birdsnest.sytes.net:13327.

## Options

| Option | Description | Default |
|---|---|---|
| `--host <host>` | Target TCP host | `birdsnest.sytes.net` |
| `--port <port>` | Target TCP port | `13327` |
| `--listen-port <port>` | WebSocket server listen port | `13327` |
| `--log` | Log frames passing through the proxy | off |
| `--max-length <n>` | Maximum characters to display per logged frame | unlimited |
| `--help` | Show help and exit | |

## Examples

Connect to a local crossfire server and log all frames, capping output at 80 characters each:

```sh
node ws-proxy.js --host localhost --port 13327 --log --max-length 80
```

Listen on a different port:

```sh
node ws-proxy.js --listen-port 8080 --host myserver.example.com --port 13327
```

## Frame logging format

When `--log` is enabled, each frame is printed with:
- Direction: `→` for WebSocket → TCP, `←` for TCP → WebSocket
- Byte length of the payload
- Content: printable ASCII characters shown as-is; all other bytes shown as `\xHH`

Example output:
```
→ [12 bytes] version 1027
← [9 bytes] version\x0a
```
