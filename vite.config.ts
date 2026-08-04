import { resolve } from "node:path";
import { defineConfig } from "vitest/config";

export default defineConfig({
  base: "./",
  build: {
    target: "es2022",
    outDir: "dist",
    rollupOptions: {
      // Dos entradas mientras convive el juego viejo con el laboratorio nuevo.
      // En la Fase 4, index.html pasa a ser el juego nuevo y esto se simplifica.
      input: {
        main: resolve(__dirname, "index.html"),
        lab: resolve(__dirname, "lab.html"),
      },
    },
  },
  test: {
    environment: "node",
    include: ["test/**/*.test.ts"],
  },
});
