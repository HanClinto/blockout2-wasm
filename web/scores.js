// scores.js: score services for BlockOut II's web build, the page side of the game's online features.
//
// The game (web/webhost.cpp) reports each game that counts as it ends, and its Online Score page asks for a setup's
// board in getscore.php's text format. A score service is an object with:
//
//   label                     "Family", "World": the board's name in the game ("FAMILY SCORE [Flat Fun]")
//   board(setupId)            Promise of the board for a setup, best first: [{ id, name, score, cubes, emptyPits,
//                             lines: [1..5 layers at once], startLevel, date (unix seconds), seconds }]
//   submit(game)              Promise of what became of a finished game: { new_best, previous_best, best, rank,
//   (optional)                players, passed: [names], unlocked: [{ id, title, text, icon }] } (README.md). A service
//                             without it only shows its board.
//   achievements() (optional) Promise of [{ id, title, text, icon, unlocked (unix seconds or null), holders }]
//   me() (optional)           Promise of { name }: who's playing, for the Hall of Fame
//
// Two come with it:
//   ScoresApi(base)  a small HTTP API (README.md, "Score service API"): boards, games, achievements. web/server.py
//                    is a small one.
//   BlockOutNet(base) the world's scores at www.blockout.net, read-only (the site takes scores with a replay, which
//                    this version doesn't record). blockout.net doesn't allow pages on other sites to read it, so base
//                    is a same-site address that passes requests on to https://www.blockout.net/blockout2/.
//
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any
// later version.
(function (global) {
  'use strict';

  var BLOCK_SETS = ['Flat', 'Basic', 'Extended'];

  // The board as the game's Online Score page reads it (getscore.php's format)
  function boardText(entries) {
    var clean = function (s) { return String(s == null ? '' : s).replace(/[\[\]\r\n]/g, ' ').slice(0, 10) || '?'; };
    var two = function (n) { return (n < 10 ? '0' : '') + n; };
    var lines = entries.slice(0, 99).map(function (e) {
      var t = Math.max(0, Math.round(e.seconds || 0));
      var l = e.lines || [];
      return ['[' + (e.id | 0) + ']', '[' + clean(e.name) + ']', '[' + (e.score | 0) + ']', '[' + (e.cubes | 0) + ']',
        '[' + (e.emptyPits | 0) + ']', '[' + (l[0] | 0) + ']', '[' + (l[1] | 0) + ']', '[' + (l[2] | 0) + ']',
        '[' + (l[3] | 0) + ']', '[' + (l[4] | 0) + ']', '[' + (e.startLevel | 0) + ']', '[' + (e.date | 0) + ']',
        '[' + two(Math.floor(t / 3600)) + ':' + two(Math.floor(t / 60) % 60) + ':' + two(t % 60) + ']'].join(' ');
    });
    return 'NbScore:' + lines.length + '\n' + lines.join('\n') + (lines.length ? '\n' : '');
  }

  function parseBoardText(text) {
    if (String(text).indexOf('NbScore:') !== 0) throw new Error('Unexpected answer from the score service');
    return String(text).split('\n').slice(1).filter(Boolean).map(function (line) {
      var f = (line.match(/\[[^\]]*\]/g) || []).map(function (s) { return s.slice(1, -1); });
      var hms = (f[12] || '0:0:0').split(':').map(Number);
      return { id: +f[0], name: (f[1] || '').trim(), score: +f[2], cubes: +f[3], emptyPits: +f[4],
        lines: [+f[5], +f[6], +f[7], +f[8], +f[9]], startLevel: +f[10], date: +f[11],
        seconds: hms[0] * 3600 + hms[1] * 60 + hms[2] };
    });
  }

  function setupName(pit, blockSet) {
    var w = Math.min(pit[0], pit[1]), h = Math.max(pit[0], pit[1]), d = pit[2];
    if (blockSet === 0 && w === 5 && h === 5 && d === 12) return 'Flat Fun';
    if (blockSet === 1 && w === 3 && h === 3 && d === 10) return '3D Mania';
    if (blockSet === 2 && w === 5 && h === 5 && d === 10) return 'Out of Control';
    return w + '×' + h + '×' + d + ' ' + (BLOCK_SETS[blockSet] || '');
  }

  async function json(res) {
    if (!res.ok) {
      var why = res.status === 401 || res.status === 403 ? 'Not signed in' : 'Score service error ' + res.status;
      try { why = (await res.json()).error || why; } catch (e) { /* not JSON */ }
      var err = new Error(why);
      err.status = res.status;
      throw err;
    }
    return res.json();
  }

  function ScoresApi(base, opts) {
    base = String(base).replace(/\/$/, '');
    opts = opts || {};
    var game = opts.game || 'blockout';
    return {
      label: opts.label || 'Family',
      board: function (setupId) {
        return fetch(base + '/' + game + '/boards/' + (setupId | 0) + '?limit=99', { credentials: 'same-origin' })
          .then(json).then(function (j) { return j.entries || []; });
      },
      submit: function (g) {
        return fetch(base + '/' + game + '/games', {
          method: 'POST', credentials: 'same-origin', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(g),
        }).then(json);
      },
      achievements: function () {
        return fetch(base + '/' + game + '/achievements', { credentials: 'same-origin' })
          .then(json).then(function (j) { return j.achievements || []; });
      },
      me: function () {
        return fetch(base + '/me', { credentials: 'same-origin' }).then(json)
          .then(function (j) { return { name: j.first_name || j.name || '' }; });
      },
    };
  }

  function BlockOutNet(base, opts) {
    base = String(base).replace(/\/$/, '');
    opts = opts || {};
    return {
      label: opts.label || 'World',
      board: function (setupId) {
        return fetch(base + '/getscore.php?setupid=' + (setupId | 0) + '&max=99')
          .then(function (res) {
            if (!res.ok) throw new Error('blockout.net answered ' + res.status);
            return res.text();
          }).then(parseBoardText);
      },
    };
  }

  // Finished games not yet sent (no connection, the service down): kept in this browser and sent on the next chance,
  // oldest first. Each has a game_id, so a game sent twice counts once.
  function Outbox(key) {
    key = key || 'blockout.outbox';
    var read = function () { try { return JSON.parse(localStorage.getItem(key)) || []; } catch (e) { return []; } };
    var write = function (list) { try { localStorage.setItem(key, JSON.stringify(list.slice(-50))); } catch (e) { /* full or blocked */ } };
    return {
      add: function (g) { var l = read(); l.push(g); write(l); },
      remove: function (id) { write(read().filter(function (g) { return g.game_id !== id; })); },
      list: read,
    };
  }

  global.BlockOutScores = { ScoresApi: ScoresApi, BlockOutNet: BlockOutNet, Outbox: Outbox, boardText: boardText,
    parseBoardText: parseBoardText, setupName: setupName };
})(window);
