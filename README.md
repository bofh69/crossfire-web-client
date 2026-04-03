# Crossfire Web Client

This is a port of the crossfire gtk-v2 client to TypeScript and Svelte.

Vite is used for the build system.

Almost everything was done in less than a day by GitHub Copilot with
Claude Opus or Sonnet, and me just testing the different versions.

## UX differences compared to GTK client.

A web page can't override all the browser's built in hot keys,
so alt is used instead of ctrl for running. That way it hopefully
leads to fewer conflicts with the browser.

There is a "Keyboard" menu for handling key bindings.

Key bindings are stored locally in the browser. If playing
from different computers/browsers, the bindings will have to
be redone.

Right clicking on items and skills brings up a menu.

Clicking on a skill selects it.


## Protocol changes

The web page can't use raw TCP sockets, so WebSockets has to be used.

The public crossfire clients don't support WebSockets, so a WebSocket
proxy has to be used when connecting to them. There is one included in
the repo.

The [crossfire-server](https://github.com/bofh69/crossfire-server) fork
has built in support for WebSockets.
