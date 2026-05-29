import path from "node:path";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { defineConfig } from "vite";
import { viteSingleFile } from "vite-plugin-singlefile";

// https://vite.dev/config/
export default defineConfig(() => {
  const now = new Date();
  const yy = String(now.getUTCFullYear()).slice(-2);
  const mm = String(now.getUTCMonth() + 1).padStart(2, "0");
  const dd = String(now.getUTCDate()).padStart(2, "0");
  const hh = String(now.getUTCHours()).padStart(2, "0");
  const min = String(now.getUTCMinutes()).padStart(2, "0");
  const buildTime = `${yy}${mm}${dd}-${hh}:${min}`;

  return {
    plugins: [svelte(), viteSingleFile({ removeViteModuleLoader: true })],
    define: {
      __BUILD_TIME__: JSON.stringify(buildTime),
    },
    build: {
      outDir: "dist-singlefile",
      rollupOptions: {
        input: {
          main: path.resolve(__dirname, "index.html"),
          replay: path.resolve(__dirname, "replay.html"),
        },
      },
    },
  };
});
