import { mount } from 'svelte'
import './app.css'
import App from './App.svelte'

const app = mount(App, {
  target: document.getElementById('app')!,
})

document.getElementById('no-js-message')!.style.display = 'none'

export default app
