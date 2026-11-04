/*
  webhost.h: what the game asks of the page around it in the web build (web/shell.html, web/blockout.js).

  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General
  Public License as published by the Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.
*/
#ifndef BLOCKOUT_WEBHOST_H
#define BLOCKOUT_WEBHOST_H

#ifdef __cplusplus
extern "C" {
#endif

// Full screen on or off (the menu's Full screen option and F1). The browser only allows it from a key or click
// handler, so the page does it then or on the next key or click.
void WebSetFullScreen(int on);

// The main menu's Exit: the page decides (back to where the player came from).
void WebQuit(void);

// The game wrote a file in its home directory (setup.dat, hscore.dat): keep it in the browser's storage.
void WebPersist(void);

#ifdef __cplusplus
}
#endif

#endif
