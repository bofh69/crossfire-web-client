# Installing the Crossfire Web Client

This guide shows how to set up the web client for your own Crossfire
server on Debian, Ubuntu and Red Hat/Fedora systems. You need basic Linux
skills: using a shell, editing files as root and installing packages.

The [README](README.md#installation) has a short summary of these steps.

## Overview

A working setup has three parts:

1. **A Crossfire server with WebSocket support.** Web pages can't open
   plain TCP connections, so the server must accept WebSocket
   connections. The upstream server doesn't support this yet, so it must
   be patched with
   [patch #471](https://sourceforge.net/p/crossfire/patches/471/).
2. **A web server**, here nginx. It serves the client's files and
   forwards WebSocket connections on the `/ws` path to the Crossfire
   server.
3. **A TLS certificate.** Browsers block unencrypted WebSockets (`ws://`)
   from pages loaded over HTTPS, so the page and the WebSocket should both
   use HTTPS/`wss://`. nginx handles TLS, so the Crossfire server doesn't
   have to.

```
browser ──https──▶ nginx :443 ──┬─ /     → client files (/var/www/crossfire)
                                └─ /ws   → Crossfire server, ws port 8080
```

In the examples below, the server's name is `crossfire.example.com`.
Replace it with your own name. The name must resolve to your server in
DNS, or Let's Encrypt can't issue a certificate.

## 1. Build the Crossfire server with WebSocket support

If you already run a Crossfire server built from source, you only need to
apply the patch and rebuild. Otherwise, first follow the server's own
build instructions, then come back here.

### Install the build tools

Debian and Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake git pkg-config flex check \
    libcurl4-openssl-dev libsqlite3-dev python3-dev
```

Fedora:

```bash
sudo dnf install gcc-c++ make cmake git pkgconf flex check-devel \
    libcurl-devel sqlite-devel python3-devel
```

Red Hat Enterprise Linux, Rocky Linux and AlmaLinux need the CodeReady
Builder (CRB) repository and EPEL for some of these packages:

```bash
sudo dnf install epel-release
sudo dnf config-manager --set-enabled crb
```

Then use the same `dnf install` command as for Fedora. The exact list of
packages the server needs can change over time. If the build complains
about something missing, install the matching `-dev` (Debian/Ubuntu) or
`-devel` (Red Hat/Fedora) package.

### Get the source and apply the patch

Download the latest `websocket-support.patch` from
[patch #471](https://sourceforge.net/p/crossfire/patches/471/). The patch
is a series of git commits, so apply it with `git am` in a checkout of
the server:

```bash
git clone https://git.code.sf.net/p/crossfire/crossfire-server
cd crossfire-server
git am /path/to/websocket-support.patch
```

If `git am` fails, the server code has changed since the patch was
written. Run `git am --abort`, then check out an older server version
or ask for an updated patch on the patch's page.

Build and install the server as described in its own documentation. With
CMake it usually looks like this:

```bash
cmake -B build
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

### Start the server with a WebSocket port

The patch adds the `-ws-port` option. Without it, no WebSocket port is
opened. Start the server with:

```bash
crossfire-server -ws-port 8080
```

The normal game port (13327) keeps working for the other clients.

If the server runs from a systemd unit, add `-ws-port 8080` to the
`ExecStart=` line, then run `sudo systemctl daemon-reload` and restart
the service.

The WebSocket port speaks plain, unencrypted WebSocket. Only nginx needs
to reach it, so don't open it in the firewall (see step 4).

## 2. Install the client files

Download `dist.tar.gz` from the
[latest release](https://github.com/bofh69/crossfire-web-client/releases/latest)
and unpack it into the web server's directory:

```bash
sudo mkdir -p /var/www/crossfire
sudo tar -xzf dist.tar.gz -C /var/www/crossfire --strip-components=1
```

`/var/www/crossfire` should now contain `index.html`, `assets/` and
`sounds/`.

### Sound files

The client loads sound effects and music from the `sounds/` directory
next to `index.html`:

- `sounds/sounds.conf` maps sound names to files.
- Sound effects are in `sounds/` itself.
- Music is in `sounds/music/`, as `<name>.ogg` or `<name>.mp3`.

`dist.tar.gz` already includes the sound files from
[crossfire-sounds](https://git.code.sf.net/p/crossfire/crossfire-sounds crossfire-crossfire-sounds).
If you use the single-file `index.html` from the release instead, copy
the contents of the crossfire-sounds repository into
`/var/www/crossfire/sounds/` next to it. Without the sound files the
client still works, but it plays no sound.

## 3. Configure nginx

### Install nginx and Certbot

Debian and Ubuntu:

```bash
sudo apt install nginx certbot python3-certbot-nginx
```

Red Hat and Fedora (on Red Hat, enable EPEL first as shown above):

```bash
sudo dnf install nginx certbot python3-certbot-nginx
sudo systemctl enable --now nginx
```

### Add the site

Create the site configuration:

- On Debian and Ubuntu: `/etc/nginx/sites-available/crossfire`
- On Red Hat and Fedora: `/etc/nginx/conf.d/crossfire.conf`

```nginx
server {
    listen 80;
    listen [::]:80;
    server_name crossfire.example.com;

    root /var/www/crossfire;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }

    location /ws {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $remote_addr;
        proxy_read_timeout 1h;
        proxy_send_timeout 1h;
    }
}
```

The `X-Forwarded-For` header passes the player's real address to the
Crossfire server, so its ban list still works. The long timeouts keep idle
players from being disconnected.

On Debian and Ubuntu, enable the site:

```bash
sudo ln -s /etc/nginx/sites-available/crossfire /etc/nginx/sites-enabled/
```

On Red Hat and Fedora, SELinux blocks nginx from connecting to the
Crossfire server until you allow it. Also give the client files the
right SELinux label:

```bash
sudo setsebool -P httpd_can_network_connect 1
sudo restorecon -R /var/www/crossfire
```

Check the configuration and reload nginx:

```bash
sudo nginx -t
sudo systemctl reload nginx
```

### Get a certificate

Certbot gets a certificate from Let's Encrypt and adds the HTTPS settings
to the site for you:

```bash
sudo certbot --nginx -d crossfire.example.com
```

Certificates are renewed automatically. To test that renewal works, run:

```bash
sudo certbot renew --dry-run
```

## 4. Open the firewall

Open HTTP and HTTPS, but not the WebSocket port. If players could reach
port 8080 directly, they could fake the `X-Forwarded-For` header and get
around bans.

Debian and Ubuntu with ufw:

```bash
sudo ufw allow 'Nginx Full'
sudo ufw deny 8080/tcp
```

Red Hat and Fedora with firewalld (ports that aren't listed stay closed):

```bash
sudo firewall-cmd --permanent --add-service=http --add-service=https
sudo firewall-cmd --reload
```

## 5. Test it

Open `https://crossfire.example.com/` in a browser. When the page is
served on the standard port, the client connects to `/ws` on the same host
by itself, so the server address field is hidden.

If it doesn't connect:

- Open the browser's developer tools (F12) and look at the console.
- Check nginx's error log, `/var/log/nginx/error.log`.
- Check that the Crossfire server is listening on the WebSocket port:
  `sudo ss -ltnp | grep 8080`.
- On Red Hat and Fedora, check for SELinux denials:
  `sudo ausearch -m avc -ts recent`.

## Upgrading the client

Download the new `dist.tar.gz`, then replace the old files:

```bash
sudo rm -rf /var/www/crossfire/*
sudo tar -xzf dist.tar.gz -C /var/www/crossfire --strip-components=1
```

On Red Hat and Fedora, run `sudo restorecon -R /var/www/crossfire`
afterwards. Players may need to reload the page to get the new version.

## Connecting to a server without WebSocket support

For local installs, `scripts/development-ws-proxy/` in this repository has a
small proxy that lets the client talk to an unpatched server. See
[DEVELOPMENT.md](DEVELOPMENT.md). Don't make this public as all players will
look like they come from your IP address and they will all get banned if any
of them is banned.
