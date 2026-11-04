# BlockOut II in the browser

[BlockOut II](http://www.blockout.net/blockout2) by Jean-Luc Pons, the 3D falling-blocks game after California
Dreams' *BlockOut* (1989), compiled to WebAssembly so it runs in a web page, with online high scores you can host
yourself.

It's the 2.5 release (2014, [SourceForge](https://sourceforge.net/projects/blockout/)) and plays the same: the first
commit here is that release exactly as published, and every change to the game's own code since is inside
`#ifdef __EMSCRIPTEN__`, so the desktop build is untouched. The original README is [README.txt](README.txt).

**Play it: <https://hanclinto.github.io/blockout2-wasm/>** (built from `main` by GitHub Actions; scores stay in your
browser there, as no score service is attached).

*BlockOut® is a registered trademark of Kadon Enterprises, Inc., used by permission (see README.txt).*

## Building

```
web/build.sh          # in the emscripten/emsdk:3.1.74 container (Docker); output in build/web/
```

Serve `build/web/` from any web server and open `index.html`; or, with online scores,
`python3 web/server.py --world` and open <http://localhost:8080/>. With Emscripten installed instead of Docker:
`make -f web/Makefile -j8`.

## What changed for the browser

| | |
|---|---|
| Main loop | `GLApplication::Run()`'s loop body is `Step()`, called once per browser frame. The frame limiter and the menu's `SDL_Delay(50)` are skipped (the browser paces frames). OpenGL errors are reported, not fatal. |
| Graphics | Emscripten's legacy OpenGL emulation, plus [web/glcompat.cpp](web/glcompat.cpp) (force-included) for what it lacks: display lists (recorded and replayed), `GL_EMISSION` (folded into ambient, exact for this game's lighting), `glMaterialf`, OpenGL 1.0 texture formats, `glDrawPixels` (the level gauge, as a textured quad), and `gluLookAt`/`gluPerspective`, since Emscripten 3.1.74's `gluLookAt` passes its arguments to gl-matrix in the wrong order and leaves the camera facing away from the scene. The canvas keeps its picture between frames (`preserveDrawingBuffer`), as the game redraws only what changes. |
| Keys | Emscripten's SDL gives keys that aren't characters their key code as the character; ArrowLeft (`80 \| 1<<10`) became `P` and paused the game. Those keys get no character, as on a desktop. |
| Files | The art and sounds are preloaded at `/bl2` (`$BL2_HOME`); the settings and the Hall of Fame (`~/.bl2`) are kept in the browser's IndexedDB. |
| Sound | Decoded by Web Audio as the game loads it, resumed on the first key or click (browsers don't play before then). The credits' music starts without its fade. |
| The page | Full screen and Exit go to the page; the game starts at the largest of its resolutions that the screen shows sharply; hiding the tab pauses a game. |
| Online scores | Below. |

## The page and online scores

[web/shell.html](web/shell.html) is the page: a top bar (status, How to play, Full screen and optional links), the game,
and a card after each game that says what it did on the board. [web/blockout.js](web/blockout.js) runs it and
[web/scores.js](web/scores.js) talks to score services. A site sets it up with attributes on `<body>`:

| Attribute | |
|---|---|
| `data-scores-api="/scores/api"` | A score service with the API below: each game that counts (played from Start Game, not the Demo or a Practice) is sent to it as it ends, and its board is the Online Score page's first. A game that can't be sent waits in the browser and goes later. A new Hall of Fame score goes in under the signed-in person's name with nothing to type (`/me`); without one, the game asks for a name as before. |
| `data-scores-label="Family"` | That board's name in the game: "FAMILY SCORE [Flat Fun]". |
| `data-world="/blockout/world"` | blockout.net's world scores, read-only, as another board: an address on your own site that passes `getscore.php` on to `https://www.blockout.net/blockout2/` (blockout.net doesn't let pages on other sites read it). |
| `data-leaderboard`, `data-back` + `data-back-label`, `data-home` + `data-home-label` | Links in the top bar; Quit in the game goes to `data-back`. |

With none of them, the game plays with its own Hall of Fame (kept in the browser) and Online Score says there's no
score service. In the game, ← → on the Online Score page switch between the boards. Sending scores to blockout.net
isn't possible: it takes them with a replay, which this version doesn't record.

### Score service API

JSON over HTTP, as the signed-in person (the service knows who's asking, for example from a sign-in proxy). `setupId`
is the game's own number for a pit and block set, the one blockout.net uses; 3×5 and 5×3 are the same setup.

**`POST {api}/blockout/games`**: a finished game.

```json
{ "setupId": 98, "pit": [5, 5, 12], "blockSet": 0, "score": 12345, "cubes": 412, "lines": [20, 6, 1, 0, 0],
  "emptyPits": 1, "startLevel": 0, "level": 2, "seconds": 431.5, "date": 1793821544, "game_id": "0f42859222b1f4aa" }
```

`lines` counts layers cleared one at a time, two at once ... five at once. `game_id` is the page's: the same game sent
twice must count once. Answer:

```json
{ "entry": { "id": 7, "score": 12345, "rank": 2 }, "new_best": true, "previous_best": 9870, "best": 12345,
  "rank": 2, "players": 3, "passed": ["Sean"],
  "unlocked": [{ "id": "double", "icon": "✌️", "title": "Double", "text": "Clear two layers with one block." }] }
```

`rank` and `players`: the person's place among everyone's bests on that setup. `unlocked`: achievements this game
earned, easiest first (empty or missing if the service has none). A 4xx other than 401/403/429 means the game is
refused and won't be sent again.

**`GET {api}/blockout/boards/{setupId}?limit=99`**: `{ "entries": [{ "id", "name", "score", "cubes", "emptyPits",
"lines", "startLevel", "level", "date", "seconds" }] }`, best first.

**`GET {api}/blockout/achievements`** (optional): `{ "achievements": [{ "id", "icon", "title", "text", "unlocked":
unix time or null, "holders": number }] }`.

**`GET {api}/me`** (optional): `{ "first_name": "Sean" }`, for the Hall of Fame.

[web/server.py](web/server.py) is a small one (Python's standard library, no achievements), for trying it out or a
small group behind a sign-in proxy; it can also pass the world board on from blockout.net (`--world`).

## Tests

`web/tests/cdp.mjs` drives headless Chrome (Node 22, no packages); the end-to-end test of the page with a score
service is kept with the service that runs it.

## License

GPL version 2 or later, as the original (each source file's header). The web build's own files (`web/`) are under the
same license.
