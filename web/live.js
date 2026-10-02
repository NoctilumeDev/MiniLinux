"use strict";
const laboratory = {
  mode: "live",
  async read(after) {
    const response = await fetch(`/api/state?after=${after}`);
    if (!response.ok) throw new Error("Bridge unavailable");
    return response.json();
  },
  async post(path, body) {
    const response = await fetch(path, {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body)});
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || "Laboratory request failed");
    return data;
  }
};
