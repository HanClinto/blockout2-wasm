// cdp.mjs: headless Chrome over the DevTools protocol, no packages needed (Node 22: fetch and WebSocket built in).
//   const b = await launch({ port: 9495, width: 1024, height: 768 });
//   await b.goto(url); await b.key('Enter'); await b.shot('menu.png'); b.close();
import { spawn } from 'node:child_process';
import { rmSync, writeFileSync } from 'node:fs';

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const CHROME = process.env.CHROME_BIN || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

// DOM key names and codes for the keys BlockOut uses
const KEYS = {
  Enter: [13, 'Enter'], Escape: [27, 'Escape'], ' ': [32, 'Space'], Backspace: [8, 'Backspace'], Tab: [9, 'Tab'],
  ArrowLeft: [37, 'ArrowLeft'], ArrowUp: [38, 'ArrowUp'], ArrowRight: [39, 'ArrowRight'], ArrowDown: [40, 'ArrowDown'],
  PageUp: [33, 'PageUp'], PageDown: [34, 'PageDown'], Home: [36, 'Home'], End: [35, 'End'],
  F1: [112, 'F1'], F2: [113, 'F2'],
};

export async function launch({ port = 9495, width = 1024, height = 768, headers, mobile = false, scale = 1 } = {}) {
  const dir = `/tmp/blockout-test-chrome-${port}`;
  rmSync(dir, { recursive: true, force: true });
  const chrome = spawn(CHROME, ['--headless=new', `--remote-debugging-port=${port}`, '--no-first-run',
    '--autoplay-policy=no-user-gesture-required', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
    `--user-data-dir=${dir}`, 'about:blank'], { stdio: 'ignore' });
  process.on('exit', () => chrome.kill());
  let targets;
  for (let i = 0; i < 100 && !targets; i++) {
    await sleep(200);
    targets = await fetch(`http://127.0.0.1:${port}/json`).then((r) => r.json()).catch(() => null);
  }
  const ws = new WebSocket(targets.find((t) => t.type === 'page').webSocketDebuggerUrl);
  await new Promise((r) => ws.addEventListener('open', r));
  let id = 0;
  const pending = new Map();
  const logs = [];
  ws.addEventListener('message', (e) => {
    const m = JSON.parse(e.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
    if (m.method === 'Runtime.consoleAPICalled') logs.push(m.params.args.map((a) => a.value ?? a.description).join(' '));
    if (m.method === 'Runtime.exceptionThrown') logs.push('EXCEPTION ' + (m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text));
  });
  const send = (method, params = {}) => new Promise((r) => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
  await send('Runtime.enable');
  await send('Network.enable');
  if (headers) await send('Network.setExtraHTTPHeaders', { headers });
  await send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: scale, mobile });

  const b = {
    send, logs,
    evaluate: async (expression) => {
      const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
      if (r.result?.exceptionDetails) throw new Error(r.result.exceptionDetails.exception?.description || 'evaluate failed');
      return r.result?.result?.value;
    },
    goto: async (url) => { await send('Page.enable'); await send('Page.navigate', { url }); },
    waitFor: async (expression, ms = 30000) => {
      for (let t = 0; t < ms; t += 100) { if (await b.evaluate(expression).catch(() => false)) return true; await sleep(100); }
      return false;
    },
    // A key pressed and released, as a person would (hold: how long it stays down)
    key: async (k, { hold = 50, after = 120 } = {}) => {
      const [vk, code] = KEYS[k] || [k.toUpperCase().charCodeAt(0), /[0-9]/.test(k) ? 'Digit' + k : 'Key' + k.toUpperCase()];
      const text = k.length === 1 ? k : k === 'Enter' ? '\r' : undefined;
      await send('Input.dispatchKeyEvent', { type: text ? 'keyDown' : 'rawKeyDown', key: k, code, windowsVirtualKeyCode: vk, text });
      await sleep(hold);
      await send('Input.dispatchKeyEvent', { type: 'keyUp', key: k, code, windowsVirtualKeyCode: vk });
      await sleep(after);
    },
    type: async (s) => { for (const ch of s) await b.key(ch); },
    tap: async (x, y) => {
      await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x, y }] });
      await sleep(50);
      await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    },
    click: async (x, y) => {
      for (const type of ['mousePressed', 'mouseReleased']) await send('Input.dispatchMouseEvent', { type, x, y, button: 'left', clickCount: 1 });
    },
    shot: async (path) => {
      const r = await send('Page.captureScreenshot', { format: 'png' });
      writeFileSync(path, Buffer.from(r.result.data, 'base64'));
    },
    close: () => { ws.close(); chrome.kill(); },
  };
  return b;
}
