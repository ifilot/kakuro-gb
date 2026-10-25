// SPDX-License-Identifier: GPL-3.0-only
// Kakuro in the browser: kakuro.gb in binjgb (vendor/binjgb, MIT), driven
// the way binjgb's docs/simple.js does it, with the keyboard, the on-screen
// buttons and a gamepad.
"use strict";

(() => {
  const ROM_URL = "kakuro.gb";
  const CPU_HZ = 4194304;
  const AUDIO_FRAMES = 4096;
  const AUDIO_LATENCY = 0.1;              // seconds of sound queued ahead
  const MAX_STEP = 5 / 60;                // at most 5 frames per animation frame
  const MIN_HOLD = 2;                     // frames a button stays down, however short the tap
  const EVENT_NEW_FRAME = 1, EVENT_AUDIO_BUFFER_FULL = 2, EVENT_UNTIL_TICKS = 4;
  const DMG_SHADES = [0xe0f8d0, 0x88c070, 0x346856, 0x081820];   // white .. black

  const SETTERS = {
    up: "_set_joyp_up", down: "_set_joyp_down", left: "_set_joyp_left", right: "_set_joyp_right",
    a: "_set_joyp_A", b: "_set_joyp_B", select: "_set_joyp_select", start: "_set_joyp_start",
  };
  const KEYS = {
    ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right",
    KeyX: "a", KeyZ: "b", Enter: "start", ShiftLeft: "select", ShiftRight: "select", Backspace: "select",
  };
  // The standard gamepad layout: buttons, then the left stick.
  const PAD_BUTTONS = { 0: "a", 1: "b", 2: "b", 3: "a", 8: "select", 9: "start", 12: "up", 13: "down", 14: "left", 15: "right" };

  // The texts the page changes as it goes.
  const TEXT = {
    soundOn: "SOUND ON", soundOff: "SOUND OFF",
    hint: "Click the Game Boy to play with the keyboard.",
    hintTouch: "Tap the buttons to play.",
    focused: "Keyboard: arrows, X = A, Z = B, Enter = START, Shift = SELECT.",
    loading: "LOADING…", switchOn: "CLICK OR PRESS A KEY TO SWITCH ON",
    failed: "The emulator could not start: {error}",
  };

  const $ = selector => document.querySelector(selector);
  const consoleEl = $("#console"), canvas = $("#screen"), overlay = $("#overlay");
  const statusEl = $("#status"), soundButton = $("#sound"), power = $("#power");

  function load(key, fallback) {
    try { return localStorage.getItem(key) || fallback; } catch { return fallback; }
  }
  function store(key, value) {
    try { localStorage.setItem(key, value); } catch { /* private mode: not remembered */ }
  }

  // --- texts --------------------------------------------------------------

  const HINT = matchMedia("(pointer: coarse)").matches ? "hintTouch" : "hint";

  function text(key, values = {}) {
    return TEXT[key].replace(/\{(\w+)\}/g, (_, name) => values[name] ?? "");
  }
  function setStatus(key, values) {
    statusEl.textContent = text(key, values);
  }
  function setOverlay(key, values) {
    $("#overlay-text").textContent = text(key, values);
  }

  // --- sound ----------------------------------------------------------------

  const audio = new AudioContext();
  const gain = audio.createGain();
  gain.connect(audio.destination);
  let soundOn = load("kakuro-sound", "on") === "on";
  let audioStart = 0;

  function setSound(on) {
    soundOn = on;
    gain.gain.value = on ? 0.5 : 0;
    soundButton.setAttribute("aria-pressed", String(on));
    soundButton.textContent = text(on ? "soundOn" : "soundOff");
    store("kakuro-sound", on ? "on" : "off");
  }
  // Browsers only start sound after a click or key.
  const unlock = () => { if (audio.state !== "running") audio.resume(); };
  for (const type of ["pointerdown", "keydown"]) window.addEventListener(type, unlock, true);

  // --- the picture: pixel perfect ---------------------------------------------

  // The screen is a whole multiple of 160 x 144 device pixels and the frame
  // is drawn into it unsmoothed: no pixel is blurred or wider than another.
  // The canvas takes the device pixels it is painted on (the observer's
  // device-pixel-content-box; at a ratio such as 1.25 that can differ from
  // its CSS size times the ratio), so the browser shows it 1:1. The rest of the Game Boy is laid out in millimetres from it
  // (style.css: the screen is SCREEN_MM wide on a body 90 x 148 mm).
  const SCREEN_MM = 56, BODY_W = 90, BODY_H = 148;
  const MIN_SCREEN = 300;                 // CSS px: grow past the window's height for this
  const frameCanvas = document.createElement("canvas");
  frameCanvas.width = 160;
  frameCanvas.height = 144;
  const frameCtx = frameCanvas.getContext("2d");
  const picture = frameCtx.createImageData(160, 144);
  const screenCtx = canvas.getContext("2d");
  const screenEl = canvas.parentElement;
  let wantW = 160, wantH = 144;           // the device pixels fit() means the screen to have

  function present() {
    frameCtx.putImageData(picture, 0, 0);
    const k = Math.max(1, Math.floor(Math.min(canvas.width / 160, canvas.height / 144)));
    screenCtx.imageSmoothingEnabled = false;
    if (canvas.width !== 160 * k || canvas.height !== 144 * k) {
      screenCtx.fillStyle = "#0c0f1c";      // a device pixel to spare: the bezel's dark
      screenCtx.fillRect(0, 0, canvas.width, canvas.height);
    }
    screenCtx.drawImage(frameCanvas, 0, 0, 160 * k, 144 * k);
  }
  try {
    new ResizeObserver(([entry]) => {
      const box = entry.devicePixelContentBoxSize?.[0];
      if (!box || (canvas.width === box.inlineSize && canvas.height === box.blockSize)) return;
      // only the rounding by a pixel; anything else is not to be trusted
      if (Math.abs(box.inlineSize - wantW) > 2 || Math.abs(box.blockSize - wantH) > 2) return;
      canvas.width = box.inlineSize;
      canvas.height = box.blockSize;
      present();
    }).observe(canvas, { box: "device-pixel-content-box" });
  } catch { /* not supported: the CSS size times the ratio, as set by fit() */ }

  // The browser lays out in 1/64 px and paints on whole device pixels: a
  // CSS size rounded up to the next 1/64 px paints as the device size meant
  // (rounded down, 432 device px at a ratio of 1.25 would paint as 431).
  const cssPx = device => Math.ceil(device / (window.devicePixelRatio || 1) * 64) / 64;

  // The largest scale whose Game Boy fits the window's width and height; or,
  // while the screen would stay small, the largest that fits its width. The
  // body then grows to the room there is, up to the window's height, with
  // the screen centred in the bezel (at least MIN_SCREEN_MM).
  const MIN_SCREEN_MM = 36;
  function fit() {
    const dpr = window.devicePixelRatio || 1;
    const availW = Math.min(document.documentElement.clientWidth - 16, 760);
    const availH = window.innerHeight - 24;
    const size = k => {
      const css = 160 * k / dpr, mm = css / SCREEN_MM;
      return { k, css, h: 144 * k / dpr, fitsW: BODY_W * mm <= availW, fitsH: BODY_H * mm <= availH };
    };
    let best = size(1);
    for (let k = 2; k <= 24; k++) {
      const s = size(k);
      if (!s.fitsW) break;
      if (s.fitsH || best.css < MIN_SCREEN) best = s;
    }
    const mm = Math.min(Math.max(best.css / SCREEN_MM, Math.min(availW / BODY_W, availH / BODY_H)),
                        best.css / MIN_SCREEN_MM);
    consoleEl.style.setProperty("--mm", `${mm}px`);
    screenEl.style.setProperty("--w", best.css / mm);
    screenEl.style.setProperty("--h", best.h / mm);
    screenEl.style.width = `${cssPx(160 * best.k)}px`;
    screenEl.style.height = `${cssPx(144 * best.k)}px`;
    canvas.width = wantW = 160 * best.k;
    canvas.height = wantH = 144 * best.k;
    present();
  }
  window.addEventListener("resize", fit);

  // --- the emulator ---------------------------------------------------------

  let module = null, rom = null, machine = null;
  let model = load("kakuro-model", "cgb") === "dmg" ? "dmg" : "cgb";

  // A Game Boy runs it in shades when the header does not ask for the Color.
  function asDmg(data) {
    data[0x143] = 0x00;
    let sum = 0;
    for (let i = 0x134; i <= 0x14c; i++) sum = (sum - data[i] - 1) & 0xff;
    data[0x14d] = sum;
  }
  const rgba = c => ((0xff << 24) | ((c & 0xff) << 16) | (c & 0xff00) | (c >> 16)) >>> 0;

  class Machine {
    constructor(romData) {
      const m = module;
      const data = new Uint8Array(romData.slice(0));
      if (model === "dmg") asDmg(data);
      const size = (data.length + 0x7fff) & ~0x7fff;
      this.romPtr = m._malloc(size);
      this.heap(this.romPtr, size).fill(0).set(data);
      this.e = m._emulator_new_simple(this.romPtr, size, audio.sampleRate, AUDIO_FRAMES, 0);
      if (!this.e) throw new Error("not a Game Boy ROM");
      this.joypad = m._joypad_new();
      m._emulator_set_default_joypad_callback(this.e, this.joypad);
      if (model === "dmg")
        for (let type = 0; type < 3; type++)
          m._emulator_set_bw_palette_simple(this.e, type, ...DMG_SHADES.map(rgba));
      this.frame = [m._get_frame_buffer_ptr(this.e), m._get_frame_buffer_size(this.e)];
      this.sound = [m._get_audio_buffer_ptr(this.e), m._get_audio_buffer_capacity(this.e)];
      this.lastSec = 0;
      this.leftover = 0;
      this.raf = 0;
      this.frames = 0;
    }
    heap(ptr, size) {                     // memory may grow: a fresh view each time
      return new Uint8Array(module.HEAP8.buffer, ptr, size);
    }
    set(button, on) {
      module[SETTERS[button]](this.e, on ? 1 : 0);
    }
    get ticks() {
      return module._emulator_get_ticks_f64(this.e);
    }
    runUntil(ticks) {
      for (;;) {
        const event = module._emulator_run_until_f64(this.e, ticks);
        if (event & EVENT_NEW_FRAME) {
          picture.data.set(this.heap(...this.frame));
          this.frames++;
        }
        if (event & EVENT_AUDIO_BUFFER_FULL) this.pushSound();
        if (event & EVENT_UNTIL_TICKS) break;
      }
    }
    pushSound() {
      if (audio.state !== "running") return;
      const now = audio.currentTime;
      if (audioStart < now) audioStart = now + AUDIO_LATENCY;   // fell behind: start afresh
      const samples = this.heap(...this.sound);
      const buffer = audio.createBuffer(2, AUDIO_FRAMES, audio.sampleRate);
      const left = buffer.getChannelData(0), right = buffer.getChannelData(1);
      for (let i = 0; i < AUDIO_FRAMES; i++) {
        left[i] = samples[2 * i] / 255;
        right[i] = samples[2 * i + 1] / 255;
      }
      const source = audio.createBufferSource();
      source.buffer = buffer;
      source.connect(gain);
      source.start(audioStart);
      audioStart += AUDIO_FRAMES / audio.sampleRate;
    }
    step(ms) {
      this.raf = requestAnimationFrame(t => this.step(t));
      pollGamepads();
      const sec = ms / 1000;
      const delta = Math.min(Math.max(sec - (this.lastSec || sec), 0), MAX_STEP);
      const until = this.ticks + delta * CPU_HZ - this.leftover;
      this.runUntil(until);
      this.leftover = (this.ticks - until) | 0;
      this.lastSec = sec;
      present();
      if (held.size) update();            // releases held back for MIN_HOLD
    }
    start() {
      if (!this.raf) this.raf = requestAnimationFrame(t => this.step(t));
    }
    stop() {
      cancelAnimationFrame(this.raf);
      this.raf = 0;
      this.lastSec = 0;
    }
    destroy() {
      this.stop();
      module._joypad_delete(this.joypad);
      module._emulator_delete(this.e);
      module._free(this.romPtr);
    }
  }

  function showModel() {
    consoleEl.classList.toggle("dmg", model === "dmg");
    consoleEl.classList.toggle("cgb", model === "cgb");
    for (const el of document.querySelectorAll("[data-model]"))
      el.setAttribute("aria-checked", String(el.dataset.model === model));
  }

  // Switches the Game Boy on (again): the page waits for a click or a key.
  function boot() {
    if (!rom) return;
    if (machine) machine.destroy();
    machine = new Machine(rom);
    held.clear();
    for (const button of Object.keys(SETTERS)) if (pressed.has(button)) machine.set(button, true);
    power.classList.add("on");
    overlay.hidden = true;
    if (!document.hidden) machine.start();
  }

  // --- input: keyboard, on-screen buttons, gamepad --------------------------

  // Each source holds its own set of buttons down; a button is down while
  // any source holds it, and for at least MIN_HOLD frames, so that the game
  // (which reads the buttons once a frame) sees even the shortest tap.
  const sources = { keys: new Set(), touch: new Map(), pad: new Set() };
  const pressed = new Set();
  const held = new Map();                 // button -> the frame it went down

  function update() {
    const now = new Set([...sources.keys, ...sources.touch.values(), ...sources.pad]);
    const frame = machine ? machine.frames : 0;
    for (const button of Object.keys(SETTERS)) {
      let on = now.has(button);
      if (!on && held.has(button) && frame - held.get(button) < MIN_HOLD) on = true;
      if (on === pressed.has(button)) continue;
      if (on) {
        pressed.add(button);
        held.set(button, frame);
      } else {
        pressed.delete(button);
        held.delete(button);
      }
      if (machine) machine.set(button, on);
      for (const el of consoleEl.querySelectorAll(`[data-button="${button}"]`)) el.classList.toggle("pressed", on);
    }
  }

  consoleEl.addEventListener("keydown", event => {
    const button = KEYS[event.code];
    if (!machine) {                       // the first key switches it on
      if (rom && !event.ctrlKey && !event.metaKey && !event.altKey && event.code !== "Tab") {
        event.preventDefault();
        boot();
      }
      return;
    }
    if (!button || event.ctrlKey || event.metaKey || event.altKey) return;
    event.preventDefault();
    sources.keys.add(button);
    update();
  });
  consoleEl.addEventListener("keyup", event => {
    const button = KEYS[event.code];
    if (!button) return;
    event.preventDefault();
    sources.keys.delete(button);
    update();
  });
  consoleEl.addEventListener("focus", () => { if (HINT === "hint") setStatus("focused"); });
  consoleEl.addEventListener("blur", () => {
    sources.keys.clear();
    update();
    setStatus(HINT);
  });

  for (const el of consoleEl.querySelectorAll("[data-button]")) {
    el.tabIndex = -1;                     // the console itself takes the keyboard
    if (el.closest(".dpad")) continue;    // the D-pad is one surface, below
    const release = event => {
      if (sources.touch.delete(event.pointerId)) update();
    };
    el.addEventListener("pointerdown", event => {
      event.preventDefault();
      consoleEl.focus({ preventScroll: true });
      if (!machine) return boot();        // the first press switches it on
      el.setPointerCapture(event.pointerId);
      sources.touch.set(event.pointerId, el.dataset.button);
      update();
    });
    el.addEventListener("pointerup", release);
    el.addEventListener("pointercancel", release);
    el.addEventListener("lostpointercapture", release);
  }
  // The D-pad as a whole: the direction is where the pointer is from its
  // centre (the larger of across and up/down, none on the hub), and follows
  // it while it is held, so a thumb rolls from one arm to the next.
  const dpad = consoleEl.querySelector(".dpad");
  function dpadAt(event) {
    const r = dpad.getBoundingClientRect();
    const x = (event.clientX - r.left) / r.width * 2 - 1, y = (event.clientY - r.top) / r.height * 2 - 1;
    if (Math.max(Math.abs(x), Math.abs(y)) < 0.15) return null;
    return Math.abs(x) > Math.abs(y) ? (x < 0 ? "left" : "right") : (y < 0 ? "up" : "down");
  }
  function dpadSet(event) {
    const direction = dpadAt(event);
    if (direction) sources.touch.set(event.pointerId, direction);
    else sources.touch.delete(event.pointerId);
    update();
  }
  dpad.addEventListener("pointerdown", event => {
    event.preventDefault();
    consoleEl.focus({ preventScroll: true });
    if (!machine) return boot();          // the first press switches it on
    dpad.setPointerCapture(event.pointerId);
    dpadSet(event);
  });
  dpad.addEventListener("pointermove", event => {
    if (dpad.hasPointerCapture(event.pointerId)) dpadSet(event);
  });
  for (const type of ["pointerup", "pointercancel", "lostpointercapture"])
    dpad.addEventListener(type, event => {
      if (sources.touch.delete(event.pointerId)) update();
    });

  consoleEl.addEventListener("pointerdown", () => {
    consoleEl.focus({ preventScroll: true });
    if (!machine) boot();
  });
  consoleEl.addEventListener("contextmenu", event => event.preventDefault());

  function pollGamepads() {
    const held = new Set();
    for (const pad of navigator.getGamepads ? navigator.getGamepads() : []) {
      if (!pad) continue;
      pad.buttons.forEach((b, i) => { if (b.pressed && PAD_BUTTONS[i]) held.add(PAD_BUTTONS[i]); });
      const [x = 0, y = 0] = pad.axes;
      if (x < -0.5) held.add("left");
      if (x > 0.5) held.add("right");
      if (y < -0.5) held.add("up");
      if (y > 0.5) held.add("down");
    }
    if (held.size !== sources.pad.size || [...held].some(b => !sources.pad.has(b))) {
      sources.pad = held;
      update();
    }
  }

  // --- settings -------------------------------------------------------------

  for (const el of document.querySelectorAll("[data-model]"))
    el.addEventListener("click", () => {
      if (el.dataset.model === model) return;
      model = el.dataset.model;
      store("kakuro-model", model);
      showModel();
      if (machine) boot();                // a switched-on Game Boy restarts as the other
      consoleEl.focus({ preventScroll: true });
    });
  soundButton.addEventListener("click", () => setSound(!soundOn));
  $("#reset").addEventListener("click", () => {
    if (!rom) return;
    boot();
    consoleEl.focus({ preventScroll: true });
  });
  for (const link of document.querySelectorAll('a[href="#play"]'))
    link.addEventListener("click", () => setTimeout(() => {
      consoleEl.focus({ preventScroll: true });
      if (!machine) boot();
    }, 0));
  document.addEventListener("visibilitychange", () => {
    if (!machine) return;
    if (document.hidden) machine.stop(); else machine.start();
  });

  // --- start ----------------------------------------------------------------

  async function start() {
    setOverlay("loading");
    try {
      if (typeof Binjgb !== "function") throw new Error("binjgb.js did not load");
      const [m, response] = await Promise.all([Binjgb(), fetch(ROM_URL)]);
      if (!response.ok) throw new Error(`${ROM_URL}: ${response.status}`);
      module = m;
      rom = await response.arrayBuffer();
      setOverlay("switchOn");
      consoleEl.classList.add("ready");
    } catch (error) {
      setOverlay("failed", { error: error.message });
      setStatus("failed", { error: error.message });
      console.error(error);
    }
  }

  setSound(soundOn);
  setStatus(HINT);
  showModel();
  fit();
  start();
})();
