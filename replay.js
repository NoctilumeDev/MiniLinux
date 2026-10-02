"use strict";
// Select a recorded clip, then advance its actual snapshots. No command executes here.
const laboratory = {
  mode: "replay", recording: null, clip: null, frame: 0, started: 0, epoch: 0, stopped: false,
  async load() {
    if (this.recording) return;
    const response = await fetch("recording.json");
    if (!response.ok) throw new Error("Recording unavailable");
    this.recording = await response.json();
    if (this.recording.format !== 1 || this.recording.mode !== "recorded-replay") throw new Error("Unsupported recording");
    this.select(this.recording.clips[0]);
  },
  select(clip) {
    this.clip = clip; this.frame = 0; this.started = performance.now(); this.epoch++;
    this.stopped = false;
  },
  async read(after) {
    await this.load();
    if (!this.stopped) {
      while (this.frame + 1 < this.clip.frames.length && this.clip.frames[this.frame + 1].at <= performance.now() - this.started) this.frame++;
    }
    const state = {...this.clip.frames[this.frame].state};
    const retained = this.clip.frames.slice(0, this.frame + 1).flatMap(frame => frame.state.events).slice(-2000);
    state.events = retained.filter(event => event.id > after);
    state.epoch = `replay-${this.epoch}`;
    state.status = this.stopped ? "stopped" : "running";
    return state;
  },
  async post(path, body) {
    await this.load();
    if (path === "/api/stop") this.stopped = true;
    else if (path === "/api/restart") this.select(this.recording.clips[0]);
    else if (path === "/api/input") {
      const command = body.text.trim().replace(/\s+/g, " ");
      const clip = this.recording.clips.find(item => item.command === command);
      if (!clip) throw new Error("This replay contains: help, ls, cat hello.txt, ps, run hello, run reader, run counter-a counter-b, run fault, check, mem. Download LAB to execute other commands.");
      this.select(clip);
    } else throw new Error("Unknown replay control");
    return {okay: true};
  }
};
