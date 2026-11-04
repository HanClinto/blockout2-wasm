/*
  webhost.cpp: see webhost.h. Each call goes to the page's `Module.blockout` object when it has one.

  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General
  Public License as published by the Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.
*/
#include "webhost.h"
#include <emscripten.h>

EM_JS(void, WebSetFullScreen, (int on), {
  var host = Module['blockout'];
  if (host && host.setFullScreen) host.setFullScreen(!!on);
});

EM_JS(void, WebQuit, (void), {
  var host = Module['blockout'];
  if (host && host.quit) host.quit();
});

// Several writes in a row (the game saves the setup and the scores together) make one sync
EM_JS(void, WebPersist, (void), {
  if (Module['persistTimer']) clearTimeout(Module['persistTimer']);
  Module['persistTimer'] = setTimeout(function() {
    Module['persistTimer'] = 0;
    FS.syncfs(false, function(err) {
      if (err) console.warn('BlockOut: could not keep the settings in this browser', err);
    });
  }, 100);
});

EM_JS(void, WebGameStarted, (void), {
  var host = Module['blockout'];
  if (host && host.gameStarted) host.gameStarted();
});

EM_JS(void, WebGameOver, (const char *json), {
  var host = Module['blockout'];
  if (host && host.gameOver) host.gameOver(JSON.parse(UTF8ToString(json)));
});

// The strings handed to the game stay valid until the next call of the same function (Module.webStrings)
EM_JS(const char *, WebPlayerName, (void), {
  var host = Module['blockout'];
  var name = (host && host.playerName) || '';
  if (typeof name === 'function') name = name() || '';
  var keep = Module['webStrings'] || (Module['webStrings'] = {});
  if (keep.name) _free(keep.name);
  keep.name = stringToNewUTF8(String(name).slice(0, 10));
  return keep.name;
});

// One request at a time: started by the first call, handed over (and forgotten) by the first call after it's done,
// so each visit to the page asks again.
EM_JS(char *, WebScoresGet, (int setupId, int *pending), {
  var host = Module['blockout'];
  var keep = Module['webStrings'] || (Module['webStrings'] = {});
  var req = Module['boardRequest'];
  if (!req) {
    req = Module['boardRequest'] = { done: false };
    if (!host || !host.boards) {
      req.done = true;
      req.error = 'No score service';
    } else {
      Promise.resolve().then(function () { return host.boards.get(setupId); }).then(
        function (text) { req.text = String(text); req.done = true; },
        function (err) { req.error = (err && err.message) || String(err) || 'Download error'; req.done = true; });
    }
  }
  if (!req.done) {
    HEAP32[pending >> 2] = 1;
    return 0;
  }
  HEAP32[pending >> 2] = 0;
  Module['boardRequest'] = null;
  if (keep.error) _free(keep.error);
  keep.error = stringToNewUTF8(req.error || '');
  if (req.text === undefined) return 0;
  if (keep.board) _free(keep.board);
  keep.board = stringToNewUTF8(req.text);
  return keep.board;
});

EM_JS(const char *, WebScoresError, (void), {
  var keep = Module['webStrings'] || (Module['webStrings'] = {});
  if (!keep.error) keep.error = stringToNewUTF8('');
  return keep.error;
});

EM_JS(const char *, WebScoresLabel, (void), {
  var host = Module['blockout'];
  var label = (host && host.boards && host.boards.label && host.boards.label()) || 'ONLINE';
  var keep = Module['webStrings'] || (Module['webStrings'] = {});
  if (keep.label) _free(keep.label);
  keep.label = stringToNewUTF8(String(label).toUpperCase().slice(0, 12));
  return keep.label;
});

EM_JS(int, WebScoresSwitch, (int dir), {
  var host = Module['blockout'];
  if (!host || !host.boards || !host.boards.switchTo) return 0;
  if (Module['boardRequest'] && !Module['boardRequest'].done) return 0;
  Module['boardRequest'] = null;
  return host.boards.switchTo(dir) ? 1 : 0;
});

EM_JS(int, WebWindowSize, (int current), {
  var host = Module['blockout'];
  if (!host || !host.windowSize) return current;
  var size = host.windowSize(current);
  return size >= 0 && size <= 4 ? size : current;
});

// Emscripten's SDL_mixer has no fades: the credits' music starts at full volume
#include <SDL_mixer.h>
extern "C" int Mix_FadeInMusic(Mix_Music *music, int loops, int ms) {
  return Mix_PlayMusic(music, loops);
}
