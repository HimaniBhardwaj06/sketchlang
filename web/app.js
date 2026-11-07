// Sketchlang playground: wires the C++ (WebAssembly) engine to the page.
// The engine returns JSON; this file only displays it.
(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const LINE_H = 21;      // must match #editor line-height in style.css
  const PAD_TOP = 8;      // must match #src padding-top
  const MAX_RUN_STEPS = 50000000;

  function h(tag, cls, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined) e.textContent = text;
    return e;
  }

  const el = {
    src: $('src'), gutter: $('gutter'), hl: $('hl'), examples: $('examples'),
    run: $('btn-run'), step: $('btn-step'), play: $('btn-play'), reset: $('btn-reset'),
    link: $('btn-link'), speed: $('speed'), stats: $('stats'),
    bytecode: $('pane-bytecode'), ast: $('pane-ast'), tokens: $('pane-tokens'),
    cv: $('cv'), status: $('status'), stack: $('stack'), frames: $('frames'),
    globals: $('globals'), console: $('console'),
  };

  // ------------------------------------------------------------ editor

  function updateGutter() {
    const n = el.src.value.split('\n').length;
    let s = '';
    for (let i = 1; i <= n; i++) s += i + (i < n ? '\n' : '');
    el.gutter.textContent = s;
    el.gutter.scrollTop = el.src.scrollTop;
  }

  let hlLine = 0, hlKind = '';
  function setHighlight(line, kind) {
    hlLine = line; hlKind = kind;
    if (!line || line < 1 || !kind) { el.hl.className = ''; return; }
    el.hl.className = kind;
    el.hl.style.top = (PAD_TOP + (line - 1) * LINE_H - el.src.scrollTop) + 'px';
  }

  el.src.addEventListener('input', () => { updateGutter(); setHighlight(0, ''); });
  el.src.addEventListener('scroll', () => {
    el.gutter.scrollTop = el.src.scrollTop;
    setHighlight(hlLine, hlKind);
  });
  el.src.addEventListener('keydown', (e) => {
    if (e.key === 'Tab') {              // insert two spaces instead of leaving the editor
      e.preventDefault();
      const s = el.src.selectionStart, t = el.src.selectionEnd;
      el.src.value = el.src.value.slice(0, s) + '  ' + el.src.value.slice(t);
      el.src.selectionStart = el.src.selectionEnd = s + 2;
      updateGutter();
    } else if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
      e.preventDefault(); runProgram();
    } else if (e.key === 'Enter' && e.shiftKey) {
      e.preventDefault(); stepProgram();
    }
  });

  function setSource(code) {
    el.src.value = code;
    updateGutter();
    setHighlight(0, '');
  }

  // ---------------------------------------------------------- examples

  SK_EXAMPLES.forEach((ex, i) => {
    const o = h('option', '', ex.name);
    o.value = String(i);
    el.examples.appendChild(o);
  });

  // ------------------------------------------------------------- tabs

  document.querySelectorAll('.tab').forEach((btn) => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab').forEach((b) => b.classList.toggle('active', b === btn));
      ['bytecode', 'ast', 'tokens'].forEach((name) => { el[name].hidden = (name !== btn.dataset.tab); });
    });
  });

  // ------------------------------------------------------- rendering

  let insRows = [];       // insRows[functionIndex][ip] -> element
  let currentRow = null;

  function renderBytecode(bc) {
    el.bytecode.textContent = '';
    insRows = [];
    if (!bc) { el.bytecode.appendChild(h('div', 'empty', 'No bytecode: fix the errors in the console first.')); return; }
    bc.forEach((fn, fi) => {
      const box = h('div', 'fn');
      const params = fn.arity ? ' (' + fn.arity + ' parameter' + (fn.arity > 1 ? 's' : '') + ')' : '';
      box.appendChild(h('div', 'fn-title', fn.name + params));
      insRows[fi] = [];
      fn.code.forEach((ins, ip) => {
        const row = h('div', 'ins');
        row.appendChild(h('span', 'ip', String(ip).padStart(4, '0')));
        row.appendChild(h('span', 'ln', 'L' + ins.line));
        row.appendChild(h('span', 'op', ins.op));
        row.appendChild(h('span', 'operand', ins.operand));
        row.appendChild(h('span', 'note', ins.note ? '; ' + ins.note : ''));
        box.appendChild(row);
        insRows[fi][ip] = row;
      });
      el.bytecode.appendChild(box);
    });
  }

  function astNode(n) {
    const wrap = h('div', 'ast-node');
    const head = h('div', 'ast-head');
    head.appendChild(h('span', 'ast-type', n.type));
    if (n.label) head.appendChild(h('span', 'ast-label', n.label));
    wrap.appendChild(head);
    (n.children || []).forEach((c) => wrap.appendChild(astNode(c)));
    return wrap;
  }

  function renderAst(ast) {
    el.ast.textContent = '';
    if (!ast) { el.ast.appendChild(h('div', 'empty', 'No syntax tree: fix the errors in the console first.')); return; }
    el.ast.appendChild(astNode(ast));
  }

  function renderTokens(tokens) {
    el.tokens.textContent = '';
    const table = h('table', 'tokens');
    const head = h('tr');
    ['line:col', 'type', 'text'].forEach((t) => head.appendChild(h('th', '', t)));
    table.appendChild(head);
    tokens.forEach((t) => {
      const row = h('tr');
      row.appendChild(h('td', '', t.line + ':' + t.col));
      row.appendChild(h('td', '', t.type));
      row.appendChild(h('td', '', t.lexeme));
      table.appendChild(row);
    });
    el.tokens.appendChild(table);
  }

  function renderConsole(lines, errorText) {
    el.console.textContent = '';
    lines.forEach((l) => el.console.appendChild(h('div', '', l)));
    if (errorText) el.console.appendChild(h('div', 'err', errorText));
    if (!lines.length && !errorText) el.console.appendChild(h('div', 'info', 'Nothing printed yet.'));
    el.console.scrollTop = el.console.scrollHeight;
  }

  function cellText(v) { return v.t === 'string' ? '"' + v.v + '"' : v.v; }

  function renderState(s) {
    el.status.className = 'badge ' + s.status;
    const label = { none: 'not started', running: 'running', finished: 'finished', error: 'error' }[s.status] || s.status;
    el.status.textContent = label + (s.steps ? ' \u00b7 ' + s.steps.toLocaleString() + ' steps' : '');

    el.stack.textContent = '';
    if (!s.stack || !s.stack.length) el.stack.appendChild(h('div', 'muted', '(empty)'));
    else s.stack.slice().reverse().forEach((v) => el.stack.appendChild(h('div', 'cell ' + v.t, cellText(v))));

    el.frames.textContent = '';
    (s.frames || []).slice().reverse().forEach((f) => el.frames.appendChild(h('div', '', f.name)));

    el.globals.textContent = '';
    (s.globals || []).forEach((g) => {
      const row = h('tr');
      row.appendChild(h('td', '', g.name));
      row.appendChild(h('td', 'val', g.defined ? cellText(g) : '\u2013'));
      el.globals.appendChild(row);
    });

    // current instruction in the bytecode panel
    if (currentRow) currentRow.classList.remove('current');
    currentRow = null;
    if (s.status === 'running' && insRows[s.fn] && insRows[s.fn][s.ip]) {
      currentRow = insRows[s.fn][s.ip];
      currentRow.classList.add('current');
      const pane = el.bytecode;
      const top = currentRow.offsetTop - pane.offsetTop;
      if (top < pane.scrollTop + 20 || top > pane.scrollTop + pane.clientHeight - 40) {
        pane.scrollTop = Math.max(0, top - pane.clientHeight / 2);
      }
    }

    // source line
    if (s.status === 'running') setHighlight(s.line, 'run');
    else if (s.status === 'error') setHighlight(s.line, 'err');
    else setHighlight(0, '');

    renderConsole(s.output || [], s.status === 'error' ? 'Runtime error (line ' + s.errorLine + '): ' + s.error : '');
  }

  // ----------------------------------------------------------- canvas

  function resetCanvas() {
    const dpr = window.devicePixelRatio || 1;
    el.cv.width = Math.round(500 * dpr);
    el.cv.height = Math.round(500 * dpr);
    const ctx = el.cv.getContext('2d');
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, el.cv.width, el.cv.height);
  }

  function drawShapes(shapes) {
    resetCanvas();
    const ctx = el.cv.getContext('2d');
    const k = el.cv.width / 100;
    ctx.setTransform(k, 0, 0, k, 0, 0);
    ctx.lineWidth = 0.4;
    shapes.forEach((s) => {
      ctx.strokeStyle = 'rgb(' + s[5] + ',' + s[6] + ',' + s[7] + ')';
      ctx.beginPath();
      if (s[0] === 0) ctx.arc(s[1], s[2], Math.max(0, s[3]), 0, Math.PI * 2);
      else if (s[0] === 1) ctx.rect(s[1], s[2], s[3], s[4]);
      else { ctx.moveTo(s[1], s[2]); ctx.lineTo(s[3], s[4]); }
      ctx.stroke();
    });
  }

  // ------------------------------------------------- engine plumbing

  let Module = null;
  let lastCompile = null;
  let compiledSource = null;
  let lastState = null;
  let lastDrawCount = -1;
  let timer = null;

  function toJs(ptr) { return JSON.parse(Module.UTF8ToString(ptr)); }

  function compileSource(src) {
    const n = Module.lengthBytesUTF8(src) + 1;
    const ptr = Module._malloc(n);
    Module.stringToUTF8(src, ptr, n);
    const result = toJs(Module._sk_compile(ptr));
    Module._free(ptr);
    return result;
  }

  function refresh() {
    const s = toJs(Module._sk_state());
    lastState = s;
    renderState(s);
    if (s.drawCount !== lastDrawCount) {
      lastDrawCount = s.drawCount;
      drawShapes(toJs(Module._sk_draws()));
    }
  }

  function showCompileErrors(errors) {
    el.console.textContent = '';
    errors.forEach((e) => {
      const where = e.line ? 'Line ' + e.line + (e.col ? ':' + e.col : '') + ' \u2014 ' : '';
      el.console.appendChild(h('div', 'err', where + e.message));
    });
    const first = errors.find((e) => e.line > 0);
    setHighlight(first ? first.line : 0, first ? 'err' : '');
    el.status.className = 'badge error';
    el.status.textContent = 'compile error';
    el.stack.textContent = ''; el.frames.textContent = ''; el.globals.textContent = '';
  }

  // Compile the current source and prepare the VM. Returns true on success.
  function compileNow() {
    stopPlay();
    const src = el.src.value;
    const res = compileSource(src);
    lastCompile = res;
    compiledSource = src;
    el.stats.textContent = '';
    renderTokens(res.tokens);
    renderAst(res.ast);
    renderBytecode(res.bytecode);
    resetCanvas();
    lastDrawCount = -1;
    if (!res.ok) {
      lastState = null;
      showCompileErrors(res.errors);
      return false;
    }
    refresh();
    return true;
  }

  function needsCompile() {
    return !lastCompile || !lastCompile.ok || compiledSource !== el.src.value ||
           !lastState || lastState.status !== 'running';
  }

  // ---------------------------------------------------------- actions

  function runProgram() {
    if (!compileNow()) return;
    const t0 = performance.now();
    Module._sk_run(MAX_RUN_STEPS);
    const ms = performance.now() - t0;
    refresh();
    const steps = lastState.steps;
    const rate = ms > 0 ? (steps / ms / 1000).toFixed(1) : '\u2013';
    el.stats.textContent = steps.toLocaleString() + ' instructions in ' + ms.toFixed(1) + ' ms (' + rate + ' M/s)';
  }

  function stepProgram() {
    stopPlay();
    if (needsCompile() && !compileNow()) return;
    Module._sk_step();
    refresh();
  }

  function stopPlay() {
    if (timer) { clearInterval(timer); timer = null; }
    el.play.textContent = '\u25b6 Play';
  }

  function togglePlay() {
    if (timer) { stopPlay(); return; }
    if (needsCompile() && !compileNow()) return;
    el.play.textContent = '\u23f8 Pause';
    const rate = Number(el.speed.value);                       // steps per second
    const interval = rate < 10 ? Math.round(1000 / rate) : 100;  // slow: one step per tick
    const perTick = rate < 10 ? 1 : Math.round(rate / 10);
    timer = setInterval(() => {
      for (let i = 0; i < perTick; i++) {
        if (Module._sk_step() !== 0) { refresh(); stopPlay(); return; }
      }
      refresh();
    }, interval);
  }

  function setUrl(url) {
    try { history.replaceState(null, '', url); } catch (e) { /* some browsers refuse; the link still works */ }
  }

  function copyLink() {
    const bytes = new TextEncoder().encode(el.src.value);
    let bin = '';
    bytes.forEach((b) => { bin += String.fromCharCode(b); });
    const url = location.origin + location.pathname + '#c=' + encodeURIComponent(btoa(bin));
    setUrl(url);
    const done = () => { el.link.textContent = 'Link copied'; setTimeout(() => { el.link.textContent = 'Copy link'; }, 1500); };
    if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(url).then(done, () => window.prompt('Copy this link:', url));
    else window.prompt('Copy this link:', url);
  }

  function sourceFromHash() {
    const m = /^#c=(.+)$/.exec(location.hash);
    if (!m) return null;
    try {
      const bin = atob(decodeURIComponent(m[1]));
      const bytes = Uint8Array.from(bin, (c) => c.charCodeAt(0));
      return new TextDecoder().decode(bytes);
    } catch (e) { return null; }
  }

  // ------------------------------------------------------------ start

  function setEnabled(on) {
    [el.run, el.step, el.play, el.reset].forEach((b) => { b.disabled = !on; });
  }

  function showSetupHelp(detail) {
    setEnabled(false);
    el.console.textContent = '';
    el.console.appendChild(h('div', 'err', 'The WebAssembly engine did not load.'));
    el.console.appendChild(h('div', '', 'Build it with ./build-wasm.sh, then serve this folder with ./serve.sh and open http://localhost:8000.'));
    el.console.appendChild(h('div', 'info', 'Opening index.html directly from disk will not work: browsers block loading .wasm files from file:// URLs.'));
    if (detail) el.console.appendChild(h('div', 'info', detail));
  }

  function start() {
    setSource(sourceFromHash() || SK_EXAMPLES[0].code);
    resetCanvas();
    renderConsole([], '');
    renderBytecode(null); renderAst(null); renderTokens([]);
    el.bytecode.firstChild.textContent = 'Press Run or Step to compile.';

    el.examples.addEventListener('change', () => {
      setSource(SK_EXAMPLES[Number(el.examples.value)].code);
      setUrl(location.pathname);
      if (Module) compileNow();
    });
    el.run.addEventListener('click', runProgram);
    el.step.addEventListener('click', stepProgram);
    el.play.addEventListener('click', togglePlay);
    el.reset.addEventListener('click', compileNow);
    el.link.addEventListener('click', copyLink);

    if (typeof createSketchModule !== 'function') { showSetupHelp(); return; }
    setEnabled(false);
    createSketchModule().then((m) => {
      Module = m;
      setEnabled(true);
      compileNow();
    }).catch((err) => showSetupHelp(String(err)));
  }

  start();
})();
