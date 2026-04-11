import { mount } from 'svelte'
import './app.css'
import App from './App.svelte'

declare const __BUILD_TIME__: string;
document.title = `Crossfire RPG ${__BUILD_TIME__}`;

const app = mount(App, {
  target: document.getElementById('app')!,
})

document.getElementById('no-js-message')!.style.display = 'none'

export default app
