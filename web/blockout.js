// blockout.js: the page around BlockOut II in the browser (web/shell.html): what the game asks of the page
// (web/webhost.h, as Module.blockout) and the page's own controls.
//
// Set up by attributes on <body> (all optional; a site adds them to the built index.html):
//   data-scores-api="/scores/api"     a score service with the ScoresApi interface (scores.js, README.md): every game
//   data-scores-label="Family"        that counts is sent to it, and its board is the Online Score page's first
//   data-world="/blockout/world"      a same-site address passed on to https://www.blockout.net/blockout2/: the
//                                     world's board, as the Online Score page's second (or only) board
//   data-leaderboard="/scores/blockout"  a "Leaderboard" link in the top bar
//   data-back="/" data-back-label="Games"  where "‹ Games" (and the game's Quit) go
//   data-home="https://..." data-home-label="Home"
//
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any
// later version.
(function () {
  'use strict';
  var S = window.BlockOutScores;
  var cfg = document.body.dataset;
  var $ = function (id) { return document.getElementById(id); };
  var stage = $('stage'), canvas = $('canvas'), statusEl = $('status'), card = $('card');

  // ---- Top bar ---------------------------------------------------------------------------------------------------
  function link(id, href, label) {
    var a = $(id);
    if (!href) return;
    a.href = href;
    if (label) a.textContent = label;
    a.hidden = false;
  }
  link('back', cfg.back, cfg.backLabel && '‹ ' + cfg.backLabel);
  link('leaderboard', cfg.leaderboard);
  link('home', cfg.home, cfg.homeLabel);

  var statusTimer = 0;
  function status(text, kind, ms) {
    statusEl.textContent = text || '';
    statusEl.className = 'status' + (kind ? ' ' + kind : '');
    clearTimeout(statusTimer);
    if (ms) statusTimer = setTimeout(function () { status(idleStatus()); }, ms);
  }

  // ---- Score services ----------------------------------------------------------------------------------------------
  var boards = [];
  var family = cfg.scoresApi ? S.ScoresApi(cfg.scoresApi, { label: cfg.scoresLabel || 'Family' }) : null;
  if (family) boards.push(family);
  if (cfg.world) boards.push(S.BlockOutNet(cfg.world));
  var current = 0;
  var outbox = S.Outbox();
  var player = '';

  function idleStatus() {
    var pending = outbox.list().length;
    if (pending) return pending + ' game' + (pending > 1 ? 's' : '') + ' waiting to be sent';
    if (family && player) return 'Playing as ' + player;
    return '';
  }

  if (family) {
    family.me().then(function (me) { player = me.name || ''; status(idleStatus()); }, function () { /* signed out */ });
  }

  function newId() {
    var a = new Uint8Array(8);
    crypto.getRandomValues(a);
    return Array.prototype.map.call(a, function (b) { return (b + 256).toString(16).slice(1); }).join('');
  }

  // Each game goes out as it ends; one that can't (no connection) waits in this browser for the next chance
  var sending = false;
  function flush() {
    if (!family || sending) return Promise.resolve();
    var list = outbox.list();
    if (!list.length) return Promise.resolve();
    sending = true;
    var g = list[0];
    return family.submit(g).then(function (result) {
      outbox.remove(g.game_id);
      sending = false;
      if (!g.late) showResult(g, result);
      return flush();
    }, function (err) {
      sending = false;
      if (err && err.status >= 400 && err.status < 500 && err.status !== 401 && err.status !== 403 && err.status !== 429) {
        outbox.remove(g.game_id);  // the service won't take it (malformed); don't keep trying
        status('The score service turned the game down: ' + err.message, 'warn', 8000);
      } else {
        status(idleStatus() + ' (no connection to the score service)', 'warn');
      }
    });
  }
  window.addEventListener('online', flush);
  setInterval(flush, 60000);
  // Games from an earlier visit go quietly (no card for them)
  outbox.list().forEach(function (g) { g.late = true; });
  setTimeout(flush, 3000);

  // ---- The card after a game ----------------------------------------------------------------------------------------
  var cardTimer = 0;
  function showResult(g, r) {
    var name = S.setupName(g.pit, g.blockSet);
    var lines = [];
    if (r.new_best && r.previous_best != null) lines.push('<b>New personal best on ' + esc(name) + '!</b>');
    else if (r.new_best) lines.push('<b>Your first score on ' + esc(name) + '.</b>');
    else lines.push(esc(name) + ': ' + num(g.score) + (r.best != null ? ' (your best: ' + num(r.best) + ')' : ''));
    if (r.passed && r.passed.length) lines.push('You passed ' + esc(listOf(r.passed)) + '.');
    if (r.rank && r.players > 1) lines.push('#' + r.rank + ' of ' + r.players + ' in the ' + esc((family.label || '').toLowerCase()) + '.');
    var html = '<div class="card-head">' + lines.join(' ') + '</div>';
    // The hardest first (services list them easiest first); a first game can earn many, so three and a count
    var unlocked = (r.unlocked || []).slice().reverse();
    unlocked.slice(0, 3).forEach(function (a) {
      html += '<div class="unlock"><span class="icon">' + esc(a.icon || '🏆') + '</span><span><b>' + esc(a.title) +
        '</b><br>' + esc(a.text || '') + '</span></div>';
    });
    if (unlocked.length > 3) {
      var more = unlocked.length - 3;
      html += '<div class="unlock more">and ' + more + ' more achievement' + (more > 1 ? 's' : '') +
        (cfg.leaderboard ? ' (<a href="' + esc(cfg.leaderboard) + '" target="_blank">see them all</a>)' : '') + '</div>';
    }
    card.innerHTML = html;
    card.hidden = false;
    card.classList.remove('gone');
    clearTimeout(cardTimer);
    cardTimer = setTimeout(hideCard, 9000 + 3000 * Math.min(unlocked.length, 3));
    status('Sent to the ' + (family.label || 'score').toLowerCase() + ' board ✓', 'ok', 6000);
  }
  function hideCard() { card.classList.add('gone'); setTimeout(function () { card.hidden = true; }, 400); }
  card.addEventListener('click', hideCard);
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function num(n) { return Number(n).toLocaleString('en-US'); }
  function listOf(a) { return a.length < 2 ? a.join('') : a.slice(0, -1).join(', ') + ' and ' + a[a.length - 1]; }

  // ---- Full screen ----------------------------------------------------------------------------------------------------
  // The browser only allows it from a key or click handler; the game asks from its frame loop (F1, its Options), so a
  // request that's turned down waits for the next key or click.
  var wantFull = null;
  function applyFull() {
    if (wantFull === null) return;
    var on = wantFull;
    wantFull = null;
    if (on && !document.fullscreenElement && stage.requestFullscreen) {
      stage.requestFullscreen({ navigationUI: 'hide' }).catch(function () { wantFull = true; });
    } else if (!on && document.fullscreenElement) {
      document.exitFullscreen().catch(function () {});
    }
  }
  ['keyup', 'pointerup'].forEach(function (t) { window.addEventListener(t, applyFull, true); });
  $('fullscreen').addEventListener('click', function () { wantFull = !document.fullscreenElement; applyFull(); canvas.focus(); });
  if (!document.fullscreenEnabled) $('fullscreen').disabled = true;

  // ---- How to play ------------------------------------------------------------------------------------------------------
  var help = $('help');
  function toggleHelp(show) {
    help.hidden = show === undefined ? !help.hidden : !show;
    if (help.hidden) canvas.focus();
  }
  $('howto').addEventListener('click', function () { toggleHelp(); });
  help.addEventListener('click', function (e) { if (e.target === help || e.target.closest('.close')) toggleHelp(false); });
  try { if (!localStorage.getItem('blockout.helped')) { toggleHelp(true); localStorage.setItem('blockout.helped', '1'); } } catch (e) { /* storage blocked */ }

  // ---- Keys and focus --------------------------------------------------------------------------------------------------
  // The game listens to the whole page; clicking the top bar mustn't leave a button holding Space or Enter.
  document.querySelectorAll('.bar button').forEach(function (b) { b.addEventListener('mouseup', function () { b.blur(); }); });
  stage.addEventListener('pointerdown', function () { canvas.focus(); });
  // A game left running in a hidden tab would play on by itself when it came back: pause it (P resumes)
  document.addEventListener('visibilitychange', function () {
    if (document.hidden && running) Module._web_pause();
  });

  // ---- The game ---------------------------------------------------------------------------------------------------------
  var SIZES = [640, 800, 1024, 1280, 1600];
  var running = false;
  window.Module = {
    canvas: canvas,
    print: function (t) { console.log(t); },
    printErr: function (t) { console.warn(t); },
    setStatus: function (t) {
      var m = /\((\d+(?:\.\d+)?)\/(\d+)\)/.exec(t || '');
      if (m) status('Loading… ' + Math.round(100 * m[1] / m[2]) + '%');
      else if (t) status(t.replace(/\.\.\./, '…'));
    },
    onRuntimeInitialized: function () { running = true; status(idleStatus()); canvas.focus(); },
    onAbort: function (what) { status('The game stopped: ' + what, 'warn'); },
    blockout: {
      playerName: function () { return player; },
      // The largest of the game's resolutions that the stage shows at full sharpness
      windowSize: function () {
        var px = Math.min(stage.clientWidth, stage.clientHeight * 4 / 3) * (window.devicePixelRatio || 1);
        var size = 0;
        SIZES.forEach(function (w, i) { if (w <= px * 1.05) size = i; });
        return size;
      },
      setFullScreen: function (on) { wantFull = !!on; applyFull(); },
      quit: function () {
        if (cfg.back) location.href = cfg.back;
        else if (history.length > 1) history.back();
      },
      gameStarted: function () { hideCard(); },
      gameOver: function (g) {
        if (!family) return;
        g.game_id = newId();
        outbox.add(g);
        flush();
      },
      boards: {
        label: function () { return boards.length ? boards[current].label : ''; },
        switchTo: function (dir) {
          if (boards.length < 2) return false;
          current = (current + dir + boards.length) % boards.length;
          return true;
        },
        get: function (setupId) {
          if (!boards.length) return Promise.reject(new Error('No score service on this page'));
          return boards[current].board(setupId).then(S.boardText);
        },
      },
    },
  };
  status('Loading…');
})();
