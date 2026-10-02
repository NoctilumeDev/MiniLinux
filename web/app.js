"use strict";
const $ = id => document.getElementById(id);
const calls = ["pid", "write", "ticks", "report", "exit", "open", "read", "close", "program", "spawn", "wait", "input", "process", "file", "memory", "inspect"];
let cursor = 0, epoch = "", state = null, selectedPid = 2, events = [], paused = false;
let consoleStart = 0, traceStart = 0, eventStart = 0, lastCommand = "", requestBusy = false;
let view = "console", connectionFailed = false;
let selectedEvent = null, eventDetailKey = "";
let counterSnapshot = null, inspectorSnapshot = null;
const processRows = new Map(), callRows = new Map(), eventRows = new Map();
const manuals = {
  help: ["HELP(1)", "NAME", "help — list the shell commands", "EXAMPLE", "help\nls\ncat hello.txt\nps\nrun hello\nrun reader\nrun counter-a counter-b"],
  cat: ["CAT(1)", "NAME", "cat — read bytes from RamFS", "MECHANISM", "user shell → open → read → write → close\nPointers are checked against all page-table levels.\nTwo files: /hello.txt and /empty.txt."],
  run: ["RUN(1)", "NAME", "run — start one or two embedded user programs", "MECHANISM", "spawn → private address space → timer preemption → exit → wait\n\nrun counter-a counter-b\nBoth use data VA 0x600000, with different physical pages.\n\nrun fault\nA forbidden read stops that process; the shell continues."],
  ps: ["PS(1)", "NAME", "ps — observe the process slots", "MECHANISM", "PID 1/init waits for the shell.\nThe shell waits for programs it creates.\nThe timer chooses the next ready slot in a fixed array.\n\nSelect a PID in the inspector to see its actual CR3 and pages."],
  ls: ["LS(1)", "NAME", "ls — enumerate the fixed RamFS files", "SCOPE", "File names and byte sizes come from the kernel.\nThere is no directory tree or disk filesystem."],
  mem: ["MEM(1)", "NAME", "mem — observe managed frames and free pages", "MECHANISM", "After wait returns, child user pages and page tables are reclaimed.\nThe free-page count should return to its pre-run value."],
  check: ["CHECK(1)", "NAME", "check — try meaningful syscall counterexamples", "MECHANISM", "Read-only destinations, kernel pointers, invalid program IDs and waits are rejected.\nA rejected file read leaves the offset unchanged."],
};
function manual(topic = "help") {
  const content = manuals[topic] || manuals.help;
  $("manual-topic").textContent = content[0];
  const fragment = document.createDocumentFragment();
  for (let i = 1; i < content.length; i += 2) {
    const heading = document.createElement("h3"), paragraph = document.createElement("p");
    heading.textContent = content[i]; paragraph.textContent = content[i + 1];
    fragment.append(heading, paragraph);
  }
  $("manual").replaceChildren(fragment);
  $("manual").parentElement.scrollTop = 0;
}
function error(message = "") { $("error").hidden = !message; $("error").textContent = message; }
async function post(path, body) {
  return laboratory.post(path, body);
}
function row(values) {
  const tr = document.createElement("tr");
  values.forEach(value => { const td = document.createElement("td"); td.textContent = value; tr.append(td); });
  return tr;
}
function renderProcesses() {
  const observed = inspectorSnapshot?.state || state;
  const body = $("processes");
  if (!observed.processes.some(p => p.pid === selectedPid)) selectedPid = observed.processes.find(p => p.program === "shell")?.pid || observed.processes[0]?.pid;
  for (const process of observed.processes) {
    let tr = processRows.get(process.pid);
    if (!tr) {
      tr = row(["", "", "", process.program]);
      const button = document.createElement("button");
      button.textContent = process.pid; button.setAttribute("aria-label", `Inspect PID ${process.pid}, ${process.program}`);
      button.onclick = () => { selectedPid = process.pid; renderProcesses(); renderMaps(); };
      tr.firstChild.append(button); body.append(tr); processRows.set(process.pid, tr);
    }
    tr.children[1].textContent = process.parent;
    tr.children[2].textContent = process.current ? "current" : process.state;
    tr.className = process.pid === selectedPid ? "selected" : "";
  }
  for (const [pid, tr] of processRows) {
    if (!observed.processes.some(p => p.pid === pid)) { tr.remove(); processRows.delete(pid); }
  }
  $("empty-processes").hidden = !!observed.processes.length;
}
function renderMaps() {
  const observed = inspectorSnapshot?.state || state;
  const process = observed.processes.find(p => p.pid === selectedPid);
  $("map-pid").textContent = process ? `(pid=${selectedPid})` : "";
  $("map-root").textContent = "CR3 " + (process?.root || "—");
  const fragment = document.createDocumentFragment();
  Object.values(observed.maps[selectedPid] || {}).sort((a, b) => a.va.localeCompare(b.va)).forEach(map => {
    const perm = "r" + ((map.flags & 1) ? "w" : "-") + ((map.flags & 4) ? "x" : "-");
    fragment.append(row([map.va, map.pa, perm, map.name]));
  });
  $("mappings").replaceChildren(fragment);
  const data = observed.data[selectedPid];
  $("data-marker").textContent = data ? `data marker ${data.marker}` : "Waiting for mapped-page observations.";
}
function describe(event) {
  const f = event.fields;
  switch (event.kind) {
    case "boot": return "kernel booted · x86_64 · one CPU";
    case "spawn": return `spawn pid=${f[0]} parent=${f[1]} program=${f[2]}`;
    case "schedule": return `timer CPL${f[3]} · PID ${f[0]} → ${f[1]}`;
    case "exit": return `exit PID ${f[0]} · status ${f[1]}`;
    case "wait": return `wait pid=${f[0]} for child=${f[1]}`;
    case "fault": return `USER FAULT PID ${f[0]} · vector ${f[1]}`;
    case "memory": return `frames=${f[0]} free=${f[1]}`;
    default: return "";
  }
}
function renderEventInfo(event) {
  const key = event ? `${selectedEvent ? "selected" : "latest"}:${event.id}` : "empty";
  if (key === eventDetailKey) return;
  eventDetailKey = key;
  $("follow-events").setAttribute("aria-pressed", String(!selectedEvent));
  $("event-meta").textContent = event ? `${selectedEvent ? "Selected" : "Latest"} #${event.id} · tick ${event.tick} · ${event.kind}` : "No event in this view.";
  const fragment = document.createDocumentFragment();
  function field(label, value) {
    const term = document.createElement("dt"), data = document.createElement("dd");
    term.textContent = label; data.textContent = value;
    fragment.append(term, data);
  }
  if (event) {
    const f = event.fields;
    switch (event.kind) {
      case "schedule": field("From PID", f[0]); field("To PID", f[1]); field("Next CR3", f[2]); field("Saved CPL", f[3]); break;
      case "fault": field("PID", f[0]); field("Vector", f[1]); field("Error", f[2]); field("Address", f[3]); field("RIP", f[4]); field("Saved CPL", f[5]); break;
      case "spawn": field("PID", f[0]); field("Parent PID", f[1]); field("Program ID", f[2]); break;
      case "wait": field("Parent PID", f[0]); field("Child PID", f[1]); break;
      case "exit": field("PID", f[0]); field("Status", f[1]); break;
      case "memory": field("Frames", f[0]); field("Free pages", f[1]); break;
      case "boot": field("Mode", f[0]); field("Architecture", f[1]); field("Timer target", `${f[2]} Hz`); field("Slots", f[3]); break;
    }
  }
  $("event-fields").replaceChildren(fragment);
}
function renderEvents() {
  const relevant = events.filter(e => e.id > eventStart && describe(e) && (view !== "fault" || e.kind === "fault")).slice(-80);
  const log = $("events"), follow = log.scrollTop + log.clientHeight >= log.scrollHeight - 30;
  relevant.forEach(event => {
    let line = eventRows.get(event.id);
    if (!line) {
      const tick = document.createElement("span"), message = document.createElement("span");
      line = document.createElement("button");
      tick.textContent = String(event.tick).padStart(7); message.textContent = describe(event);
      line.setAttribute("aria-label", `Inspect ${event.kind} event ${event.id} at tick ${event.tick}`);
      line.onclick = () => { selectedEvent = event; renderEvents(); };
      line.append(tick, message); log.append(line); eventRows.set(event.id, line);
    }
    const selected = selectedEvent?.id === event.id;
    line.className = selected ? "event-row selected" : "event-row";
    line.setAttribute("aria-pressed", String(selected));
  });
  for (const [id, line] of eventRows) {
    if (!relevant.some(event => event.id === id)) {
      if (line.contains(document.activeElement)) log.focus();
      line.remove(); eventRows.delete(id);
    }
  }
  log.querySelector("p")?.remove();
  // Returning from FAULT must restore time order without replacing focused rows.
  relevant.forEach((event, index) => {
    const line = eventRows.get(event.id), next = log.children[index];
    if (line !== next) log.insertBefore(line, next || null);
  });
  if (!relevant.length) {
    const empty = document.createElement("p");
    empty.textContent = view === "fault" ? "No fault in this view. Try: run fault" : "No events in this view.";
    log.append(empty);
  }
  if (follow) log.scrollTop = log.scrollHeight;
  renderEventInfo(selectedEvent || relevant[relevant.length - 1]);
}
function renderCalls() {
  if (paused) return;
  const source = inspectorSnapshot?.events || events;
  const recent = source.filter(e => e.kind === "syscall" && e.id > traceStart).slice(-40);
  const body = $("syscalls");
  recent.forEach(event => {
    if (callRows.has(event.id)) return;
    const f = event.fields, name = calls[Number(f[1])] || "unknown";
    const tr = row([event.tick, f[0], "", f[5]]);
    const button = document.createElement("button"); button.textContent = name;
    button.setAttribute("aria-label", `Inspect ${name} call from PID ${f[0]} at tick ${event.tick}`);
    button.onclick = () => { $("syscall-detail").textContent = `PID ${f[0]} ${name}: a=${f[2]} b=${f[3]} c=${f[4]} → ${f[5]}`; };
    tr.children[2].append(button); body.prepend(tr); callRows.set(event.id, tr);
  });
  for (const [id, tr] of callRows) {
    if (!recent.some(e => e.id === id)) { tr.remove(); callRows.delete(id); }
  }
  const last = [...source].reverse().find(e => e.kind === "syscall");
  if (last) $("mode-witness").textContent = `${laboratory.mode === "replay" ? "Recorded" : "Observed"} CPL ${last.fields[6]} entry · PID ${last.fields[0]} · tick ${last.tick}`;
}
function renderInspector() {
  if (!state) return;
  const observed = inspectorSnapshot?.state || state;
  const source = laboratory.mode === "replay" ? "历史录制" : connectionFailed ? "客体状态未知" : state.status === "running" ? "客体继续运行" : "客体已停止";
  $("observation").textContent = inspectorSnapshot ? `快照 tick ${observed.ticks} · ${source}` : `${connectionFailed || state.status !== "running" ? "末次记录" : "as of"} tick ${observed.ticks}`;
  $("free-pages").textContent = observed.free ?? "—";
  $("counter-snapshot").disabled = !counterSnapshot;
  let returnLabel = "回到实时观察";
  if (laboratory.mode === "replay") returnLabel = "回到当前回放";
  else if (connectionFailed || state.status !== "running") returnLabel = "回到末次记录";
  $("counter-snapshot").textContent = inspectorSnapshot ? returnLabel : "查看最近 counter 快照";
  renderProcesses(); renderMaps(); renderCalls();
}
$("counter-snapshot").onclick = () => {
  inspectorSnapshot = inspectorSnapshot ? null : counterSnapshot;
  if (inspectorSnapshot) selectedPid = inspectorSnapshot.state.processes.find(p => p.program === "counter-a")?.pid || 2;
  paused = false; $("pause-trace").textContent = "pause";
  traceStart = 0; callRows.clear(); $("syscalls").replaceChildren();
  $("syscall-detail").textContent = "Select a call to inspect its arguments.";
  renderInspector();
};
function render() {
  const terminal = $("terminal"), follow = terminal.scrollTop + terminal.clientHeight >= terminal.scrollHeight - 35;
  terminal.textContent = state.console.slice(Math.max(0, consoleStart - state.console_base));
  if (follow) terminal.scrollTop = terminal.scrollHeight;
  const replay = laboratory.mode === "replay";
  $("connection").textContent = replay ? (state.status === "running" ? "REPLAY" : "REPLAY PAUSED") : state.status.toUpperCase(); $("connection").className = state.status;
  $("coordinates").textContent = replay ? `Recorded ${laboratory.recording.recorded_at.slice(0, 10)} · ${laboratory.recording.source.slice(0, 7)} · ${laboratory.clip.command} · frame ${laboratory.frame + 1}/${laboratory.clip.frames.length}` : `QEMU TCG · 1 CPU · ${state.guest_memory} MiB · COM1 / COM2`;
  $("footer-state").textContent = `latest event #${state.sequence} · observed, not instantaneous`;
  $("command").disabled = state.status !== "running" && !(replay && state.status === "paused");
  document.querySelector("#command-form button").disabled = $("command").disabled;
  $("restart").disabled = false;
  document.querySelectorAll("[data-command]").forEach(button => { button.disabled = $("command").disabled; });
  $("stop").disabled = !replay && state.status !== "running";
  if (replay) {
    $("stop").textContent = state.status === "paused" ? "resume replay" : "pause replay";
    const hints = {"cat hello.txt": "Find open/read/close in Syscall Trace; click read for its buffer and returned byte count.", "run counter-a counter-b": "Select their PIDs to compare VA 0x600000 and physical pages. Pause to inspect.", "run fault": "Use FAULT to inspect vector 14; the recorded shell returns after this process stops."};
    document.querySelector(".terminal-note").textContent = "Recorded clips only. " + (hints[laboratory.clip.command] || "Commands select a recording; no kernel runs here.");
  } else {
    document.querySelector(".terminal-note").textContent = state.status === "running" ? "实时 QEMU。命令在用户态执行；counter 结束后可查看最近快照。↑ 回忆上一条命令。" : "客体已停止；观察窗保留末次记录。点 restart guest 从启动重新运行。";
  }
  renderInspector(); renderEvents();
  if (state.error) error(state.error);
}
async function poll() {
  try {
    state = await laboratory.read(cursor);
    if (connectionFailed) { error(); connectionFailed = false; }
    if (state.epoch !== epoch) {
      const previousEpoch = epoch;
      epoch = state.epoch; cursor = 0; events = []; consoleStart = traceStart = eventStart = 0;
      selectedPid = 2;
      counterSnapshot = inspectorSnapshot = null;
      selectedEvent = null; eventDetailKey = ""; eventRows.clear(); $("events").replaceChildren();
      processRows.clear(); callRows.clear(); $("processes").replaceChildren(); $("syscalls").replaceChildren();
      $("syscall-detail").textContent = "Select a call to inspect its arguments.";
      $("mode-witness").textContent = "Awaiting a user entry.";
      error();
      // Any old cursor can hide the new guest's prefix, even if a suffix arrived.
      if (previousEpoch) { setTimeout(poll, 50); return; }
    }
    events.push(...state.events); events = events.slice(-2000); cursor = state.sequence;
    if (state.processes.some(p => p.program === "counter-a") && state.processes.some(p => p.program === "counter-b")) counterSnapshot = {state, events: events.slice()};
    render();
  } catch (exc) {
    connectionFailed = true;
    $("connection").textContent = "DISCONNECTED"; $("connection").className = "disconnected";
    $("command").disabled = true; error(`${laboratory.mode === "replay" ? "Cannot load recorded replay" : "Cannot reach the laboratory bridge"}: ${exc.message}`);
    document.querySelector("#command-form button").disabled = true; $("restart").disabled = true;
    document.querySelectorAll("[data-command]").forEach(button => { button.disabled = true; });
    $("stop").disabled = true;
    if (laboratory.mode !== "replay") document.querySelector(".terminal-note").textContent = "连接中断，客体当前状态未知。若已关闭启动窗口，请重开 LAB，并打开它显示的网址。";
    renderInspector();
  }
  setTimeout(poll, 350);
}
async function command(value) {
  if (connectionFailed || requestBusy || !state || (state.status !== "running" && !(laboratory.mode === "replay" && state.status === "paused"))) return;
  if (!/^[\x20-\x7e\t]*$/.test(value)) { error("The teaching shell accepts ASCII commands."); return; }
  requestBusy = true;
  try { error(); await post("/api/input", {text: value + "\r"}); lastCommand = value; manual(value.trim().split(/\s+/)[0]); $("command").value = ""; }
  catch (exc) { error(exc.message); }
  finally { requestBusy = false; $("command").focus(); }
}
$("command-form").onsubmit = event => { event.preventDefault(); command($("command").value); };
$("command").onkeydown = event => { if (event.key === "ArrowUp") { event.preventDefault(); $("command").value = lastCommand; } };
document.querySelectorAll("[data-command]").forEach(button => { button.onclick = () => command(button.dataset.command); });
$("clear-console").onclick = () => { consoleStart = state ? state.console_base + state.console.length : 0; $("terminal").textContent = ""; };
$("clear-trace").onclick = () => { traceStart = cursor; callRows.clear(); $("syscalls").replaceChildren(); };
$("clear-events").onclick = () => { eventStart = cursor; selectedEvent = null; renderEvents(); };
$("follow-events").onclick = () => { selectedEvent = null; renderEvents(); $("events").scrollTop = $("events").scrollHeight; };
$("pause-trace").onclick = () => { paused = !paused; $("pause-trace").textContent = paused ? "resume" : "pause"; if (!paused) renderCalls(); };
$("restart").onclick = async () => {
  $("restart").disabled = true; $("command").disabled = true; $("connection").textContent = "RESTARTING";
  try { await post("/api/restart", {}); error(); }
  catch (exc) { error(exc.message); }
  finally { $("restart").disabled = false; }
};
$("stop").onclick = async () => {
  try { await post("/api/stop", {}); error(); }
  catch (exc) { error(exc.message); }
};
const panels = {console: "console-panel", process: "process-panel", memory: "memory-panel", syscall: "syscall-panel", trace: "trace-panel", fault: "trace-panel", manual: "manual-panel"};
document.querySelectorAll("[data-view]").forEach(button => {
  button.onclick = () => {
    view = button.dataset.view;
    if (view === "fault" && selectedEvent?.kind !== "fault") selectedEvent = null;
    document.querySelectorAll("[data-view]").forEach(item => item.classList.toggle("active", item === button));
    document.querySelectorAll(".focused").forEach(item => item.classList.remove("focused"));
    const panel = $(panels[button.dataset.view]); panel.classList.add("focused");
    if (matchMedia("(max-width: 999px)").matches) panel.scrollIntoView({behavior: "smooth", block: "nearest"});
    if (button.dataset.view === "console") $("command").focus();
    renderEvents();
  };
});
$("about").onclick = () => $("about-dialog").showModal();
$("close-about").onclick = () => $("about-dialog").close();
if (laboratory.mode === "replay") {
  $("download-lab").hidden = false;
  $("manual-actions").textContent = "按钮选择实录";
  $("stop").textContent = "pause replay"; $("restart").textContent = "reset replay";
  document.querySelector(".terminal-note").textContent = "Recorded clips only. Commands select a recording; no kernel runs in this browser.";
  $("about-dialog").querySelectorAll("p")[1].textContent = "This is an interactive replay of real QEMU runs. Console, processes, mappings and events come from recorded snapshots. Commands select clips; they do not execute a new guest. The Windows LAB download runs the actual kernel.";
  document.querySelector("footer > span").textContent = "MiniLinux / REPLAY · actual recorded mechanisms";
}
else document.querySelector(".event-info").setAttribute("aria-label", "Observed event details");
manual(); poll();
