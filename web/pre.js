// pre.js: runs before the game starts (emcc --pre-js).
//
// - The game reads its art and sounds from $BL2_HOME (preloaded at /bl2) and keeps its settings and high scores in
//   ~/.bl2 (setup.dat, hscore.dat), which is mounted on the browser's IndexedDB so they survive a reload.
// - The game draws the menu and the pit's background once and then only what changes, so the WebGL canvas has to
//   keep its picture between frames (preserveDrawingBuffer), as a desktop window does.

Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  ENV['BL2_HOME'] = '/bl2';
  ENV['HOME'] = '/home/web_user';
  var dir = '/home/web_user/.bl2';
  try { FS.mkdir(dir); } catch (e) { /* already there */ }
  FS.mount(IDBFS, {}, dir);
  addRunDependency('bl2-storage');
  FS.syncfs(true, function (err) {
    if (err) console.warn('BlockOut: settings from this browser could not be read', err);
    removeRunDependency('bl2-storage');
  });
});

(function () {
  if (typeof HTMLCanvasElement === 'undefined' || HTMLCanvasElement.prototype.__blockoutPatched) return;
  var getContext = HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.getContext = function (type, attributes) {
    if (type === 'webgl' || type === 'webgl2' || type === 'experimental-webgl') {
      attributes = Object.assign({}, attributes || {}, { preserveDrawingBuffer: true });
    }
    return getContext.call(this, type, attributes);
  };
  HTMLCanvasElement.prototype.__blockoutPatched = true;
})();

// Sounds: Emscripten's SDL_mixer reads each file and decodes it with Web Audio as the game loads it (not while
// preloading). Browsers keep audio paused until the first key or click, so every audio context is resumed then.
Module['noAudioDecoding'] = true;
(function () {
  var Ctx = typeof window !== 'undefined' && (window.AudioContext || window.webkitAudioContext);
  if (!Ctx || Ctx.__blockoutPatched) return;
  var contexts = [];
  var Patched = function (opts) { var c = new Ctx(opts); contexts.push(c); return c; };
  Patched.prototype = Ctx.prototype;
  Patched.__blockoutPatched = true;
  window.AudioContext = Patched;
  if (window.webkitAudioContext) window.webkitAudioContext = Patched;
  Module['audioContexts'] = contexts;
  var resume = function () {
    contexts.forEach(function (c) { if (c.state === 'suspended') c.resume(); });
  };
  ['keydown', 'pointerdown', 'touchend'].forEach(function (t) { window.addEventListener(t, resume, true); });
})();
