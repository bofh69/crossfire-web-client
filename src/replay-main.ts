import { mount } from "svelte";
import "./app.css";
import ReplayApp from "./ReplayApp.svelte";

declare const __BUILD_TIME__: string;
document.title = `Crossfire Replay ${__BUILD_TIME__}`;

const app = mount(ReplayApp, {
  target: document.getElementById("app")!,
});

document.getElementById("no-js-message")!.style.display = "none";

export default app;
