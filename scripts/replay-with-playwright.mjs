#!/usr/bin/env node

import fs from "node:fs/promises";
import path from "node:path";
import { spawn } from "node:child_process";
import process from "node:process";
import { fileURLToPath } from "node:url";
import { WebSocketServer } from "ws";
import { chromium } from "playwright";

function usage() {
  console.log(`Usage: npm run replay:playwright -- --log <path> [options]

Required:
  --log <path>                   Replay logfile (compact or converted format)

Optional:
  --instructions <path>          JSON instruction file
  --mark-regex <pattern>         Regex for MARK screenshot labels (default: .*)
  --mark-delay-ms <n>            Delay before MARK screenshots (default: 1200)
  --output-dir <path>            Screenshot output directory (default: screenshots/replay-playwright)
  --tx-timeout-ms <n>            Wait timeout for each TX match (default: 5000)
  --tx-continue-delay-ms <n>     Delay after TX timeout/match (default: 120)
  --ws-port <n>                  Replay WebSocket server port (default: 13390)
  --vite-port <n>                Vite dev server port (default: 4173)
  --host <host>                  Host for Vite and replay WS server (default: 127.0.0.1)
  --headless <true|false>        Run browser headless (default: true)
  --help                         Show this help message
`);
}

function parseArgs(argv) {
  const opts = {
    log: "",
    instructions: "",
    markRegex: ".*",
    markDelayMs: 1200,
    outputDir: "screenshots/replay-playwright",
    txTimeoutMs: 5000,
    txContinueDelayMs: 120,
    wsPort: 13390,
    vitePort: 4173,
    host: "127.0.0.1",
    headless: true,
  };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const next = argv[i + 1];
    if (arg === "--help") {
      usage();
      process.exit(0);
    } else if (arg === "--log") {
      opts.log = requireValue(arg, next);
      i++;
    } else if (arg === "--instructions") {
      opts.instructions = requireValue(arg, next);
      i++;
    } else if (arg === "--mark-regex") {
      opts.markRegex = requireValue(arg, next);
      i++;
    } else if (arg === "--mark-delay-ms") {
      opts.markDelayMs = requireInt(arg, next);
      i++;
    } else if (arg === "--output-dir") {
      opts.outputDir = requireValue(arg, next);
      i++;
    } else if (arg === "--tx-timeout-ms") {
      opts.txTimeoutMs = requireInt(arg, next);
      i++;
    } else if (arg === "--tx-continue-delay-ms") {
      opts.txContinueDelayMs = requireInt(arg, next);
      i++;
    } else if (arg === "--ws-port") {
      opts.wsPort = requireInt(arg, next);
      i++;
    } else if (arg === "--vite-port") {
      opts.vitePort = requireInt(arg, next);
      i++;
    } else if (arg === "--host") {
      opts.host = requireValue(arg, next);
      i++;
    } else if (arg === "--headless") {
      opts.headless = parseBoolean(requireValue(arg, next));
      i++;
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }
  if (!opts.log) {
    throw new Error("Missing required argument: --log");
  }
  return opts;
}

function requireValue(arg, value) {
  if (value === undefined) {
    throw new Error(`${arg} requires a value`);
  }
  return value;
}

function requireInt(arg, value) {
  const n = Number.parseInt(requireValue(arg, value), 10);
  if (!Number.isFinite(n)) {
    throw new Error(`${arg} requires an integer value`);
  }
  return n;
}

function parseBoolean(value) {
  if (value === "true") return true;
  if (value === "false") return false;
  throw new Error(`Invalid boolean value: ${value}`);
}

function parseReplayLogFile(text) {
  const lines = text.split(/\r?\n/);
  const entries = [];
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (!line.trim()) continue;
    const lineNumber = i + 1;
    if (line.includes("\t")) {
      entries.push(parseCompactReplayLine(line, lineNumber));
    } else {
      entries.push(parseConvertedReplayLine(line, lineNumber));
    }
  }
  return entries;
}

function parseCompactReplayLine(line, lineNumber) {
  const parts = line.split("\t");
  if (parts.length < 3) {
    throw new Error(`Line ${lineNumber}: malformed tab-separated replay line`);
  }
  const timestamp = parseIntStrict(
    parts[0],
    `Line ${lineNumber}: bad timestamp`,
  );
  const type = parts[1];
  if (type === "MARK") {
    const marker = JSON.parse(parts.slice(2).join("\t"));
    if (typeof marker !== "string") {
      throw new Error(`Line ${lineNumber}: MARK payload must be a string`);
    }
    return {
      type: "MARK",
      timestamp,
      lineNumber,
      markerText: marker,
      payload: null,
      keyData: null,
      uiData: null,
    };
  }
  if (type === "KEY") {
    const keyData = parseJsonObject(
      parts.slice(2).join("\t"),
      lineNumber,
      "KEY",
    );
    if (typeof keyData.key !== "string" || !keyData.key) {
      throw new Error(
        `Line ${lineNumber}: KEY payload must have a non-empty "key" string field`,
      );
    }
    const validEvents = ["down", "press", "up"];
    const event = keyData.event ?? "press";
    if (!validEvents.includes(event)) {
      throw new Error(
        `Line ${lineNumber}: KEY "event" must be one of: ${validEvents.join(", ")}`,
      );
    }
    return {
      type: "KEY",
      timestamp,
      lineNumber,
      markerText: null,
      payload: null,
      keyData: { ...keyData, event },
      uiData: null,
    };
  }
  if (type === "UI") {
    const uiData = parseJsonObject(parts.slice(2).join("\t"), lineNumber, "UI");
    if (typeof uiData.action !== "string" || !uiData.action.trim()) {
      throw new Error(
        `Line ${lineNumber}: UI payload must have a non-empty "action" string field`,
      );
    }
    return {
      type: "UI",
      timestamp,
      lineNumber,
      markerText: null,
      payload: null,
      keyData: null,
      uiData,
    };
  }
  if (type !== "TX" && type !== "RX") {
    throw new Error(`Line ${lineNumber}: unknown type "${type}"`);
  }
  const len = parseIntStrict(parts[2], `Line ${lineNumber}: bad byte length`);
  const payload = Buffer.from(parts.slice(3).join("\t"), "base64");
  if (payload.length !== len) {
    throw new Error(
      `Line ${lineNumber}: byte length mismatch (expected ${len}, got ${payload.length})`,
    );
  }
  return {
    type,
    timestamp,
    lineNumber,
    markerText: null,
    payload: Uint8Array.from(payload),
    keyData: null,
    uiData: null,
  };
}

function parseConvertedReplayLine(line, lineNumber) {
  const match = /^(\d+)\s+(TX|RX|MARK|KEY|UI)(?:\s(.*))?$/.exec(line);
  if (!match) {
    throw new Error(`Line ${lineNumber}: unsupported replay line`);
  }
  const timestamp = parseIntStrict(
    match[1],
    `Line ${lineNumber}: bad timestamp`,
  );
  const type = match[2];
  const rest = match[3] ?? "";
  if (type === "MARK") {
    return {
      type: "MARK",
      timestamp,
      lineNumber,
      markerText: new TextDecoder().decode(parseCString(rest, lineNumber)),
      payload: null,
      keyData: null,
      uiData: null,
    };
  }
  if (type === "KEY") {
    const keyData = parseJsonObject(
      new TextDecoder().decode(parseCString(rest, lineNumber)),
      lineNumber,
      "KEY",
    );
    if (typeof keyData.key !== "string" || !keyData.key) {
      throw new Error(
        `Line ${lineNumber}: KEY payload must have a non-empty "key" string field`,
      );
    }
    const validEvents = ["down", "press", "up"];
    const event = keyData.event ?? "press";
    if (!validEvents.includes(event)) {
      throw new Error(
        `Line ${lineNumber}: KEY "event" must be one of: ${validEvents.join(", ")}`,
      );
    }
    return {
      type: "KEY",
      timestamp,
      lineNumber,
      markerText: null,
      payload: null,
      keyData: { ...keyData, event },
      uiData: null,
    };
  }
  if (type === "UI") {
    const uiData = parseJsonObject(
      new TextDecoder().decode(parseCString(rest, lineNumber)),
      lineNumber,
      "UI",
    );
    if (typeof uiData.action !== "string" || !uiData.action.trim()) {
      throw new Error(
        `Line ${lineNumber}: UI payload must have a non-empty "action" string field`,
      );
    }
    return {
      type: "UI",
      timestamp,
      lineNumber,
      markerText: null,
      payload: null,
      keyData: null,
      uiData,
    };
  }
  return {
    type,
    timestamp,
    lineNumber,
    markerText: null,
    payload: parseCString(rest, lineNumber),
    keyData: null,
    uiData: null,
  };
}

function parseJsonObject(text, lineNumber, type) {
  let data;
  try {
    data = JSON.parse(text);
  } catch {
    throw new Error(`Line ${lineNumber}: invalid ${type} JSON payload`);
  }
  if (!data || typeof data !== "object") {
    throw new Error(
      `Line ${lineNumber}: ${type} payload must be a JSON object`,
    );
  }
  return data;
}

function parseIntStrict(value, errorMessage) {
  if (!/^\d+$/.test(value)) {
    throw new Error(errorMessage);
  }
  return Number.parseInt(value, 10);
}

function parseCString(text, lineNumber) {
  const bytes = [];
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch !== "\\") {
      bytes.push(ch.charCodeAt(0));
      continue;
    }
    i += 1;
    const next = text[i];
    if (next === undefined) {
      throw new Error(`Line ${lineNumber}: trailing backslash`);
    }
    if (next === "n") {
      bytes.push(0x0a);
      continue;
    }
    if (next === "\\" || next === '"') {
      bytes.push(next.charCodeAt(0));
      continue;
    }
    if (next === "x") {
      const hex = text.slice(i + 1, i + 3);
      if (!/^[0-9A-Fa-f]{2}$/.test(hex)) {
        throw new Error(`Line ${lineNumber}: invalid \\x escape`);
      }
      bytes.push(Number.parseInt(hex, 16));
      i += 2;
      continue;
    }
    bytes.push(next.charCodeAt(0));
  }
  return Uint8Array.from(bytes);
}

function parsePayloadInfo(payload) {
  const divider = payload.findIndex((b) => b === 0x20 || b === 0x0a);
  const commandBytes =
    divider === -1 ? payload : payload.subarray(0, Math.max(0, divider));
  const command = new TextDecoder().decode(commandBytes).trim();
  const hasDivider = divider !== -1 && divider < payload.length - 1;
  const data = hasDivider ? payload.subarray(divider + 1) : new Uint8Array();
  const dataText = new TextDecoder().decode(data);

  let ncomSeq = null;
  let ncomCommandText = null;
  if (command === "ncom" && data.length >= 6) {
    ncomSeq = new DataView(
      data.buffer,
      data.byteOffset,
      data.byteLength,
    ).getUint16(0, false);
    ncomCommandText = new TextDecoder().decode(data.subarray(6));
  }

  return {
    command,
    data,
    dataText,
    ncomSeq,
    ncomCommandText,
    commandKey:
      command === "ncom" && ncomCommandText !== null
        ? `ncom.${ncomCommandText}`
        : command,
  };
}

function sanitizeNamePart(value) {
  return value
    .trim()
    .replace(/[^a-z0-9._-]+/gi, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 120);
}

function escapeRegex(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

class MessageQueue {
  constructor() {
    this.items = [];
    this.waiters = [];
  }

  push(item) {
    this.items.push(item);
    this.pump();
  }

  async waitForMatch(predicate, timeoutMs) {
    const immediateIndex = this.items.findIndex(predicate);
    if (immediateIndex >= 0) {
      return this.items.splice(immediateIndex, 1)[0];
    }
    return new Promise((resolve) => {
      const waiter = { predicate, resolve };
      this.waiters.push(waiter);
      const timeout = setTimeout(() => {
        const idx = this.waiters.indexOf(waiter);
        if (idx >= 0) this.waiters.splice(idx, 1);
        resolve(null);
      }, timeoutMs);
      waiter.timeout = timeout;
    });
  }

  pump() {
    if (this.waiters.length === 0 || this.items.length === 0) {
      return;
    }
    for (let i = 0; i < this.waiters.length; i++) {
      const waiter = this.waiters[i];
      const itemIdx = this.items.findIndex(waiter.predicate);
      if (itemIdx < 0) {
        continue;
      }
      const item = this.items.splice(itemIdx, 1)[0];
      this.waiters.splice(i, 1);
      clearTimeout(waiter.timeout);
      waiter.resolve(item);
      i -= 1;
    }
  }
}

function buildClientMatcher(expected) {
  return (candidate) => {
    if (candidate.command !== expected.command) {
      return false;
    }
    if (expected.command !== "ncom") {
      return true;
    }
    return (
      candidate.ncomCommandText !== null &&
      expected.ncomCommandText !== null &&
      candidate.ncomCommandText === expected.ncomCommandText
    );
  };
}

async function waitForHttp(url, timeoutMs = 30000) {
  const started = Date.now();
  while (Date.now() - started < timeoutMs) {
    try {
      const res = await fetch(url);
      if (res.ok) {
        return;
      }
    } catch {
      // ignored
    }
    await delay(200);
  }
  throw new Error(`Timed out waiting for ${url}`);
}

function startVite(host, port) {
  const scriptDir = path.dirname(fileURLToPath(import.meta.url));
  const viteCli = path.resolve(scriptDir, "../node_modules/vite/bin/vite.js");
  const child = spawn(
    process.execPath,
    [viteCli, "--host", host, "--port", String(port), "--strictPort"],
    {
      stdio: ["ignore", "pipe", "pipe"],
      env: process.env,
    },
  );
  child.stdout.on("data", (chunk) => process.stdout.write(`[vite] ${chunk}`));
  child.stderr.on("data", (chunk) => process.stderr.write(`[vite] ${chunk}`));
  return child;
}

async function terminateChildProcess(child, timeoutMs = 5000) {
  if (!child || child.exitCode !== null || child.killed) {
    return;
  }

  const waitForExit = new Promise((resolve) => {
    child.once("exit", resolve);
  });

  child.kill("SIGTERM");
  const exitedOnSigterm = await Promise.race([
    waitForExit.then(() => true),
    delay(timeoutMs).then(() => false),
  ]);

  if (!exitedOnSigterm && child.exitCode === null) {
    child.kill("SIGKILL");
    await waitForExit;
  }
}

async function waitForVisualIdle(page, delayMs) {
  if (delayMs > 0) {
    await delay(delayMs);
  }
  await page.evaluate(async () => {
    await new Promise((resolve) => setTimeout(resolve, 0));
    await new Promise((resolve) => {
      if (typeof requestAnimationFrame !== "function") {
        resolve();
        return;
      }
      requestAnimationFrame(() => requestAnimationFrame(() => resolve()));
    });
  });
}

async function captureMarkScreenshot(
  page,
  outputDir,
  markDelayMs,
  entryIndex,
  label,
) {
  await waitForVisualIdle(page, markDelayMs);
  const screenshotPath = path.join(
    outputDir,
    `${String(entryIndex + 1).padStart(6, "0")}-${sanitizeNamePart(label) || "mark"}.png`,
  );
  await page.screenshot({ path: screenshotPath, fullPage: true });
  console.log(
    `[replay] screenshot for MARK "${label}" -> ${path.relative(process.cwd(), screenshotPath)}`,
  );
}

function loadInstructionRules(fileText) {
  const parsed = JSON.parse(fileText);
  if (!Array.isArray(parsed.tx)) {
    throw new Error('Instruction file must contain a top-level "tx" array');
  }
  return parsed.tx.map((rule, idx) => {
    if (typeof rule !== "object" || rule === null) {
      throw new Error(`Instruction tx[${idx}] must be an object`);
    }
    if (typeof rule.match !== "string" || !rule.match.trim()) {
      throw new Error(
        `Instruction tx[${idx}] requires non-empty string "match"`,
      );
    }
    if (!Array.isArray(rule.actions)) {
      throw new Error(`Instruction tx[${idx}] requires "actions" array`);
    }
    return {
      match: new RegExp(rule.match),
      actions: rule.actions,
      source: `instructions.tx[${idx}]`,
    };
  });
}

async function runAction(page, action, context, match) {
  if (!action || typeof action !== "object") {
    throw new Error(`Invalid action in ${context}`);
  }
  const resolveTemplateText = (rawValue, fieldName) => {
    if (rawValue === undefined || rawValue === null) {
      return "";
    }
    const text = String(rawValue);
    return text.replace(/\$(\d+)/g, (_, groupIndexText) => {
      const groupIndex = Number.parseInt(groupIndexText, 10);
      if (
        !match ||
        groupIndex >= match.length ||
        match[groupIndex] === undefined
      ) {
        throw new Error(
          `Missing regex group $${groupIndex} for "${fieldName}" in ${context}`,
        );
      }
      return match[groupIndex];
    });
  };
  switch (action.type) {
    case "clickRole": {
      const role = action.role ?? "button";
      const name = String(action.name ?? "");
      if (!name)
        throw new Error(`clickRole action missing "name" in ${context}`);
      await page
        .getByRole(role, {
          name: action.exact ? name : new RegExp(escapeRegex(name), "i"),
          exact: Boolean(action.exact),
        })
        .first()
        .click({ timeout: action.timeoutMs ?? 5000 });
      return;
    }
    case "clickText": {
      const text = String(action.text ?? "");
      if (!text)
        throw new Error(`clickText action missing "text" in ${context}`);
      await page
        .getByText(action.exact ? text : new RegExp(escapeRegex(text), "i"), {
          exact: Boolean(action.exact),
        })
        .first()
        .click({ timeout: action.timeoutMs ?? 5000 });
      return;
    }
    case "fillRole": {
      const role = action.role ?? "textbox";
      const name = String(action.name ?? "");
      const value = String(action.value ?? "");
      if (!name)
        throw new Error(`fillRole action missing "name" in ${context}`);
      await page
        .getByRole(role, {
          name: action.exact ? name : new RegExp(escapeRegex(name), "i"),
          exact: Boolean(action.exact),
        })
        .first()
        .fill(value);
      return;
    }
    case "press": {
      const key = resolveTemplateText(action.key, "key");
      if (!key) throw new Error(`press action missing "key" in ${context}`);
      await page.keyboard.press(key);
      return;
    }
    case "keyDown": {
      const key = resolveTemplateText(action.key, "key");
      if (!key) throw new Error(`keyDown action missing "key" in ${context}`);
      await page.keyboard.down(key);
      return;
    }
    case "keyUp": {
      const key = resolveTemplateText(action.key, "key");
      if (!key) throw new Error(`keyUp action missing "key" in ${context}`);
      await page.keyboard.up(key);
      return;
    }
    case "clickSelector": {
      const selector = String(action.selector ?? "").trim();
      if (!selector) {
        throw new Error(
          `clickSelector action missing "selector" in ${context}`,
        );
      }
      const options = {
        timeout: action.timeoutMs ?? 5000,
        button: action.button ?? "left",
      };
      await page.locator(selector).first().click(options);
      return;
    }
    case "fillSelector": {
      const selector = String(action.selector ?? "").trim();
      if (!selector) {
        throw new Error(`fillSelector action missing "selector" in ${context}`);
      }
      const value = resolveTemplateText(action.value, "value");
      await page
        .locator(selector)
        .first()
        .fill(value, {
          timeout: action.timeoutMs ?? 5000,
        });
      return;
    }
    case "waitForTimeout": {
      const ms = Number(action.ms ?? 0);
      await page.waitForTimeout(ms);
      return;
    }
    case "screenshot": {
      const filename = String(action.name ?? `tx-${context}.png`);
      await page.screenshot({
        path: filename,
        fullPage: Boolean(action.fullPage),
      });
      return;
    }
    default:
      throw new Error(`Unsupported action type "${action.type}" in ${context}`);
  }
}

async function applyBuiltInAutomation(page, expectedInfo) {
  if (expectedInfo.command === "accountlogin") {
    await page
      .getByRole("button", { name: /log in/i })
      .first()
      .click({ timeout: 5000 });
    return true;
  }
  if (expectedInfo.command === "accountplay") {
    const name = expectedInfo.dataText.trim();
    if (!name) return false;
    await page
      .getByRole("button", {
        name: new RegExp(`^\\s*${escapeRegex(name)}(?:\\s|$)`, "i"),
      })
      .first()
      .click({ timeout: 5000 });
    return true;
  }
  return false;
}

async function replayUiAction(page, uiData, lineNumber) {
  const action = String(uiData.action ?? "").trim();
  if (!action) return;
  if (action === "configSnapshot") {
    // Already applied via addInitScript before the browser launched.
    console.log(
      `[replay] UI line ${lineNumber}: configSnapshot already applied`,
    );
    return;
  }
  if (action === "menuSelect") {
    const menuId = String(uiData.menuId ?? "").trim();
    const entryId = String(uiData.entryId ?? "").trim();
    const menuLabel = String(uiData.menuLabel ?? "").trim();
    const entryLabel = String(uiData.entryLabel ?? "").trim();
    const isConnectionMenu =
      menuId === "ui-menu-connection" ||
      /^connection$/i.test(menuLabel) ||
      entryId.startsWith("ui-menu-connection-");
    if (isConnectionMenu) {
      console.log(
        `[replay] UI line ${lineNumber}: ignoring Connection menu action`,
      );
      return;
    }
    if (menuId) {
      await page.locator(`[data-ui-nav-id="${menuId}"]`).first().click();
    } else if (menuLabel) {
      await page
        .getByRole("button", {
          name: new RegExp(`^\\s*${escapeRegex(menuLabel)}\\s*$`, "i"),
        })
        .first()
        .click();
    } else {
      console.warn(
        `[replay] UI line ${lineNumber}: menuSelect missing menu target`,
      );
      return;
    }
    if (entryId) {
      await page.locator(`[data-ui-nav-id="${entryId}"]`).first().click();
      return;
    }
    if (entryLabel) {
      await page
        .getByRole("button", {
          name: new RegExp(`^\\s*${escapeRegex(entryLabel)}\\s*$`, "i"),
        })
        .first()
        .click();
      return;
    }
    console.warn(
      `[replay] UI line ${lineNumber}: menuSelect missing entry target`,
    );
    return;
  }
  if (action === "filterClick") {
    const selector = String(uiData.selector ?? "").trim();
    if (!selector) {
      console.warn(
        `[replay] UI line ${lineNumber}: filterClick missing selector`,
      );
      return;
    }
    await page.locator(selector).first().click();
    return;
  }
  if (action === "lookAtClick") {
    const x = Math.min(Math.max(Number(uiData.x), 0), 1);
    const y = Math.min(Math.max(Number(uiData.y), 0), 1);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      console.warn(`[replay] UI line ${lineNumber}: lookAtClick missing x/y`);
      return;
    }
    const canvas = page.locator(".game-map canvas").first();
    const box = await canvas.boundingBox();
    if (!box) {
      throw new Error(`UI line ${lineNumber}: game map canvas not visible`);
    }
    await page.mouse.click(box.x + box.width * x, box.y + box.height * y, {
      button: "left",
    });
    return;
  }
  if (action === "zoomCommand") {
    const direction = uiData.direction === "in" ? -120 : 120;
    await page.evaluate((deltaY) => {
      const canvas = document.querySelector(".game-map canvas");
      if (!canvas) return;
      canvas.dispatchEvent(
        new WheelEvent("wheel", {
          deltaY,
          ctrlKey: true,
          bubbles: true,
          cancelable: true,
        }),
      );
    }, direction);
    return;
  }
  console.warn(
    `[replay] UI line ${lineNumber}: unsupported action "${action}"`,
  );
}

async function main() {
  const opts = parseArgs(process.argv.slice(2));

  const markRegex = new RegExp(opts.markRegex);
  const outputDir = path.resolve(opts.outputDir);
  await fs.mkdir(outputDir, { recursive: true });

  const replayPath = path.resolve(opts.log);
  const replayText = await fs.readFile(replayPath, "utf8");
  const entries = parseReplayLogFile(replayText);
  const configBackup =
    entries.find(
      (e) =>
        e.type === "UI" &&
        e.uiData !== null &&
        e.uiData.action === "configSnapshot",
    )?.uiData?.backup ?? null;
  const hasRxTickEntries = entries.some(
    (entry) =>
      entry.type === "RX" &&
      entry.payload !== null &&
      parsePayloadInfo(entry.payload).command === "tick",
  );

  const rules = [];
  if (opts.instructions) {
    const instructionText = await fs.readFile(
      path.resolve(opts.instructions),
      "utf8",
    );
    rules.push(...loadInstructionRules(instructionText));
  }

  let browser = null;
  let page = null;
  let wsClient = null;
  let wss = null;
  const seqPatchMap = new Map();
  const inboundQueue = new MessageQueue();
  const pendingMarkScreenshots = [];
  const vite = startVite(opts.host, opts.vitePort);
  let finished = false;

  const shutdown = async () => {
    if (finished) return;
    finished = true;
    if (wsClient && wsClient.readyState === wsClient.OPEN) {
      wsClient.close();
    }
    if (browser) {
      await browser.close();
    }
    if (wss) {
      await new Promise((resolve, reject) => {
        wss.close((error) => {
          if (error) {
            reject(error);
            return;
          }
          resolve();
        });
      });
      wss = null;
    }
    if (vite && !vite.killed) {
      await terminateChildProcess(vite);
    }
  };

  try {
    await waitForHttp(`http://${opts.host}:${opts.vitePort}/`);
    console.log(`[replay] Vite ready at http://${opts.host}:${opts.vitePort}/`);

    wss = new WebSocketServer({
      host: opts.host,
      port: opts.wsPort,
    });
    const wsConnected = new Promise((resolve) => {
      wss.on("connection", (ws) => {
        wsClient = ws;
        ws.on("message", (raw) => {
          const payload = Uint8Array.from(raw);
          const info = parsePayloadInfo(payload);
          inboundQueue.push({ payload, ...info });
        });
        ws.on("close", () => {
          wsClient = null;
        });
        resolve();
      });
    });

    console.log("[replay] Launching browser...");
    browser = await chromium.launch({ headless: opts.headless });
    const context = await browser.newContext();
    await context.addInitScript(
      (configValues) => {
        try {
          if (configValues !== null) {
            // Remove any existing crossfire config before applying the snapshot.
            const keysToRemove = [];
            for (let i = 0; i < localStorage.length; i++) {
              const key = localStorage.key(i);
              if (key !== null && key.startsWith("crossfire_")) {
                keysToRemove.push(key);
              }
            }
            for (const key of keysToRemove) {
              localStorage.removeItem(key);
            }
            for (const [key, value] of Object.entries(configValues)) {
              localStorage.setItem("crossfire_" + key, JSON.stringify(value));
            }
          }
          localStorage.setItem("config_server_ticks", "1");
        } catch {
          // ignore storage errors in non-standard contexts
        }
      },
      configBackup !== null &&
        typeof configBackup === "object" &&
        configBackup.values != null
        ? configBackup.values
        : null,
    );
    page = await context.newPage();

    page.on("console", (msg) => {
      console.log(`[browser] ${msg.type()}: ${msg.text()}`);
    });
    page.on("pageerror", (err) => {
      console.error(`[browser] page error: ${err.message}`);
    });

    const appUrl = `http://${opts.host}:${opts.vitePort}/?server=${encodeURIComponent(
      `ws://${opts.host}:${opts.wsPort}`,
    )}`;
    console.log(`[replay] Navigating to ${appUrl}`);
    await page.goto(appUrl, { waitUntil: "domcontentloaded" });
    console.log('[replay] Clicking client "Enter" button...');
    await page
      .getByRole("button", { name: /^enter$/i })
      .first()
      .click({ timeout: 5000 });

    console.log(
      `[replay] Waiting for client to connect to ws://${opts.host}:${opts.wsPort} ...`,
    );
    await wsConnected;
    console.log(
      `[replay] client connected to ws://${opts.host}:${opts.wsPort}`,
    );

    console.log(`[replay] Starting replay of ${entries.length} entries...`);
    for (let idx = 0; idx < entries.length; idx++) {
      const entry = entries[idx];
      if (entry.type === "MARK") {
        const label = entry.markerText ?? "";
        if (markRegex.test(label)) {
          if (hasRxTickEntries) {
            pendingMarkScreenshots.push({ idx, label });
          } else {
            await captureMarkScreenshot(
              page,
              outputDir,
              opts.markDelayMs,
              idx,
              label,
            );
          }
        }
        continue;
      }

      if (entry.type === "KEY") {
        const { key, event } = entry.keyData;
        console.log(
          `[replay] KEY line ${entry.lineNumber}: ${event} "${key}"` +
            (entry.keyData.command ? ` (${entry.keyData.command})` : ""),
        );
        if (event === "down") {
          await page.keyboard.down(key);
        } else if (event === "up") {
          await page.keyboard.up(key);
        } else {
          await page.keyboard.press(key);
        }
        continue;
      }

      if (entry.type === "UI") {
        console.log(
          `[replay] UI line ${entry.lineNumber}: ${entry.uiData.action}`,
        );
        await replayUiAction(page, entry.uiData, entry.lineNumber);
        continue;
      }

      if (!wsClient || wsClient.readyState !== wsClient.OPEN) {
        throw new Error("Client websocket disconnected");
      }

      const payload = entry.payload;
      const entryInfo = parsePayloadInfo(payload);

      if (entry.type === "RX") {
        let toSend = payload;
        if (entryInfo.command === "comc" && entryInfo.data.length >= 2) {
          const expectedSeq = new DataView(
            entryInfo.data.buffer,
            entryInfo.data.byteOffset,
            entryInfo.data.byteLength,
          ).getUint16(0, false);
          const patchSeq = seqPatchMap.get(expectedSeq);
          if (patchSeq !== undefined) {
            const patched = Uint8Array.from(payload);
            const divider = patched.indexOf(0x20);
            if (divider >= 0 && divider + 2 < patched.length) {
              patched[divider + 1] = (patchSeq >> 8) & 0xff;
              patched[divider + 2] = patchSeq & 0xff;
              toSend = patched;
              seqPatchMap.delete(expectedSeq);
              console.log(
                `[replay] patched comc seq ${expectedSeq} -> ${patchSeq} (line ${entry.lineNumber})`,
              );
            }
          }
        }
        wsClient.send(toSend);
        if (entryInfo.command === "newmap") {
          await delay(150);
        }
        if (entryInfo.command === "tick" && pendingMarkScreenshots.length > 0) {
          while (pendingMarkScreenshots.length > 0) {
            const mark = pendingMarkScreenshots.shift();
            await captureMarkScreenshot(
              page,
              outputDir,
              opts.markDelayMs,
              mark.idx,
              mark.label,
            );
          }
        }
        continue;
      }

      const txKey = entryInfo.commandKey;
      console.log(
        `[replay] TX line ${entry.lineNumber}: waiting for "${txKey}"`,
      );

      let builtInApplied = false;
      try {
        builtInApplied = await applyBuiltInAutomation(page, entryInfo);
      } catch (error) {
        console.warn(
          `[replay] built-in automation failed for "${txKey}": ${String(error)}`,
        );
      }
      if (builtInApplied) {
        console.log(`[replay] built-in action applied for "${txKey}"`);
      }

      for (const rule of rules) {
        const match = rule.match.exec(txKey);
        if (!match) {
          continue;
        }
        console.log(`[replay] applying ${rule.source} for "${txKey}"`);
        for (
          let actionIndex = 0;
          actionIndex < rule.actions.length;
          actionIndex++
        ) {
          await runAction(
            page,
            rule.actions[actionIndex],
            `${rule.source}[${actionIndex}]`,
            match,
          );
        }
      }

      const matched = await inboundQueue.waitForMatch(
        buildClientMatcher(entryInfo),
        opts.txTimeoutMs,
      );

      if (!matched) {
        console.warn(
          `[replay] TX timeout ${opts.txTimeoutMs}ms for "${txKey}" (line ${entry.lineNumber})`,
        );
      } else {
        console.log(
          `[replay] matched client "${matched.commandKey}" for TX line ${entry.lineNumber}`,
        );
        if (
          entryInfo.command === "ncom" &&
          entryInfo.ncomSeq !== null &&
          matched.ncomSeq !== null
        ) {
          seqPatchMap.set(entryInfo.ncomSeq, matched.ncomSeq);
        }
      }
      await delay(opts.txContinueDelayMs);
    }

    if (pendingMarkScreenshots.length > 0) {
      while (pendingMarkScreenshots.length > 0) {
        const mark = pendingMarkScreenshots.shift();
        await captureMarkScreenshot(
          page,
          outputDir,
          opts.markDelayMs,
          mark.idx,
          mark.label,
        );
      }
    }

    console.log("[replay] replay completed");
    await shutdown();
  } catch (error) {
    await shutdown();
    throw error;
  }
}

main().catch((error) => {
  const message =
    error instanceof Error ? error.stack || error.message : String(error);
  console.error(`[replay] failed: ${message}`);
  process.exit(1);
});
