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

// A game that counts started; and ended, with what the game knows about it as JSON:
//   {"setupId", "pit": [w, h, d], "blockSet", "score", "cubes", "lines": [1 to 5 layers at once], "emptyPits",
//    "startLevel", "level", "seconds", "date"}
void WebGameStarted(void);
void WebGameOver(const char *json);

// Who's playing, for the Hall of Fame's name ("" if the page doesn't know). Up to 10 characters are used.
const char *WebPlayerName(void);

// Online Score: the board for a setup, in getscore.php's text format, from the page's chosen score service.
// Starts the request or checks on it: *pending is 1 until it's done; then the text, or NULL (WebScoresError says why).
char *WebScoresGet(int setupId, int *pending);
const char *WebScoresError(void);
// The chosen service's name for the title ("FAMILY", "WORLD"), and switching (1 next, -1 previous; 0 if there's no other)
const char *WebScoresLabel(void);
int WebScoresSwitch(int dir);

// The resolution to start in (the game's window sizes: 0 640x480, 1 800x600, 2 1024x768, 3 1280x1024,
// 4 1600x1200), given the one in the settings
int WebWindowSize(int current);

#ifdef __cplusplus
}
#endif

#endif
