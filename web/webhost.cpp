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

// Emscripten's SDL_mixer has no fades: the credits' music starts at full volume
#include <SDL_mixer.h>
extern "C" int Mix_FadeInMusic(Mix_Music *music, int loops, int ms) {
  return Mix_PlayMusic(music, loops);
}
