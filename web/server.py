"""A small score service for BlockOut II's web build (README.md, "Score service API"), and a server for the built
page: Python's standard library and SQLite, nothing to install. For trying the online scores out, or a small group
on a private network. No achievements (unlocked is always empty); a fuller service can add them.

    python3 web/server.py [--port 8080] [--db scores.db] [--world]      (after web/build.sh)
        then open http://localhost:8080/ (the page is set up for this service as it's served)

Who's playing comes from the Remote-User and Remote-Name headers when a sign-in proxy in front sets them, else the
player is "player". --world passes /world/getscore.php on to blockout.net, for the world board.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later
version.
"""
import argparse
import json
import os
import sqlite3
import time
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

HERE = os.path.dirname(os.path.abspath(__file__))
WEB = os.path.join(HERE, "..", "build", "web")
PER_PLAYER = 4  # games each person can have on a board

SCHEMA = """
CREATE TABLE IF NOT EXISTS games (
    id INTEGER PRIMARY KEY, user TEXT NOT NULL, name TEXT NOT NULL, at INTEGER NOT NULL, game_id TEXT,
    setup INTEGER NOT NULL, score INTEGER NOT NULL, cubes INTEGER NOT NULL, lines TEXT NOT NULL,
    empty_pits INTEGER NOT NULL, start_level INTEGER NOT NULL, level INTEGER NOT NULL, seconds REAL NOT NULL);
CREATE UNIQUE INDEX IF NOT EXISTS games_once ON games (user, game_id);
"""


def whole(v, lo, hi):
    return isinstance(v, int) and not isinstance(v, bool) and lo <= v <= hi


class Handler(SimpleHTTPRequestHandler):
    db = None
    world = False

    def __init__(self, *a, **kw):
        super().__init__(*a, directory=WEB, **kw)

    def who(self):
        user = self.headers.get("Remote-User") or "player"
        name = (self.headers.get("Remote-Name") or user).split()[0]
        return user, name[:1].upper() + name[1:]

    def send_json(self, obj, status=200):
        body = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def top(self, setup, per_player):
        return self.db.execute(
            f"""SELECT * FROM (SELECT *, ROW_NUMBER() OVER (PARTITION BY user ORDER BY score DESC, at) AS n
                               FROM games WHERE setup = ?) WHERE n <= {int(per_player)} ORDER BY score DESC, at""",
            (setup,)).fetchall()

    def do_GET(self):
        url = urlparse(self.path)
        user, name = self.who()
        if url.path == "/api/me":
            return self.send_json({"first_name": name})
        if url.path.startswith("/api/blockout/boards/"):
            setup = int(url.path.rsplit("/", 1)[1])
            limit = min(int(parse_qs(url.query).get("limit", ["99"])[0]), 99)
            return self.send_json({"entries": [
                {"id": r["id"], "name": r["name"], "score": r["score"], "cubes": r["cubes"], "lines": json.loads(r["lines"]),
                 "emptyPits": r["empty_pits"], "startLevel": r["start_level"], "level": r["level"], "date": r["at"],
                 "seconds": r["seconds"], "mine": r["user"] == user} for r in self.top(setup, PER_PLAYER)[:limit]]})
        if url.path == "/api/blockout/achievements":
            return self.send_json({"achievements": []})
        if url.path in ("/", "/index.html"):  # the page, set up for this service
            attrs = ' data-scores-api="/api"' + (' data-world="/world"' if self.world else "")
            with open(os.path.join(WEB, "index.html"), encoding="utf-8") as f:
                body = f.read().replace("<body>", f"<body{attrs}>", 1).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            return self.wfile.write(body)
        if url.path == "/world/getscore.php" and self.world:
            with urllib.request.urlopen("https://www.blockout.net/blockout2/getscore.php?" + url.query, timeout=15) as r:
                body = r.read()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            return self.wfile.write(body)
        return super().do_GET()

    def do_POST(self):
        if urlparse(self.path).path != "/api/blockout/games":
            return self.send_json({"error": "not found"}, 404)
        user, name = self.who()
        try:
            g = json.loads(self.rfile.read(min(int(self.headers.get("Content-Length", 0)), 4096)))
            pit, lines = g["pit"], g["lines"]
            assert len(pit) == 3 and whole(pit[0], 3, 7) and whole(pit[1], 3, 7) and whole(pit[2], 6, 18)
            assert len(lines) == 5 and all(whole(n, 0, 10**6) for n in lines)
            assert whole(g["setupId"], 0, 10000) and whole(g["score"], 0, 2**31 - 1) and whole(g["cubes"], 0, 10**7)
            assert whole(g["emptyPits"], 0, 10**5) and whole(g["startLevel"], 0, 10) and whole(g["level"], 0, 10)
            assert isinstance(g["seconds"], (int, float)) and 0 <= g["seconds"] <= 7 * 86400
        except (ValueError, KeyError, TypeError, AssertionError):
            return self.send_json({"error": "not a BlockOut game"}, 400)
        setup = g["setupId"]
        bests = self.top(setup, 1)
        previous = next((r["score"] for r in bests if r["user"] == user), None)
        better = previous is None or g["score"] > previous
        passed = [r["name"] for r in bests if r["user"] != user and (previous or 0) <= r["score"] < g["score"]] if better else []
        try:
            cur = self.db.execute(
                "INSERT INTO games (user, name, at, game_id, setup, score, cubes, lines, empty_pits, start_level, level, "
                "seconds) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                (user, name, int(time.time()), str(g.get("game_id") or "")[:64] or None, setup, g["score"], g["cubes"],
                 json.dumps(lines), g["emptyPits"], g["startLevel"], g["level"], float(g["seconds"])))
            self.db.commit()
            entry_id = cur.lastrowid
        except sqlite3.IntegrityError:  # sent before
            entry_id = self.db.execute("SELECT id FROM games WHERE user = ? AND game_id = ?", (user, g["game_id"])).fetchone()[0]
            better, passed = False, []
        bests = self.top(setup, 1)
        place = next((i + 1 for i, r in enumerate(bests) if r["user"] == user), None)
        rank = next((i + 1 for i, r in enumerate(self.top(setup, PER_PLAYER)) if r["id"] == entry_id), None)
        return self.send_json({"entry": {"id": entry_id, "score": g["score"], "rank": rank}, "new_best": better,
                               "previous_best": previous, "best": bests[place - 1]["score"] if place else None,
                               "rank": place, "players": len(bests), "passed": passed, "unlocked": []})


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--port", type=int, default=8080)
    p.add_argument("--db", default="scores.db")
    p.add_argument("--world", action="store_true", help="pass /world/getscore.php on to blockout.net")
    a = p.parse_args()
    Handler.db = sqlite3.connect(a.db, check_same_thread=False)
    Handler.db.row_factory = sqlite3.Row
    Handler.db.executescript(SCHEMA)
    Handler.world = a.world
    print(f"BlockOut II on http://localhost:{a.port}/ (scores in {a.db})")
    ThreadingHTTPServer(("127.0.0.1", a.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
