import { resolve } from "node:path";
import { VitePWA } from "vite-plugin-pwa";
import { defineConfig } from "vitest/config";

export default defineConfig({
  base: "./",
  plugins: [
    VitePWA({
      registerType: "autoUpdate",
      includeAssets: ["favicon.png"],
      manifest: {
        name: "PetBits",
        short_name: "PetBits",
        description: "Criaturas de datos: cada una nace de una semilla de 64 bits.",
        lang: "es-AR",
        theme_color: "#0a0e0a",
        background_color: "#0a0e0a",
        display: "standalone",
        orientation: "portrait",
        start_url: "./",
        scope: "./",
        icons: [
          { src: "icon-192.png", sizes: "192x192", type: "image/png" },
          { src: "icon-512.png", sizes: "512x512", type: "image/png" },
          {
            src: "icon-512-maskable.png",
            sizes: "512x512",
            type: "image/png",
            purpose: "maskable",
          },
        ],
      },
      workbox: {
        globPatterns: ["**/*.{js,css,html,png,svg,woff2}"],
        runtimeCaching: [
          {
            // Las tipografías vienen de Google Fonts. Sin cachearlas, el juego
            // instalado se ve con la fuente por defecto apenas se corta internet
            // — que es justo el escenario que la PWA tiene que cubrir.
            urlPattern: /^https:\/\/fonts\.googleapis\.com\//,
            handler: "StaleWhileRevalidate",
            options: { cacheName: "google-fonts-css" },
          },
          {
            urlPattern: /^https:\/\/fonts\.gstatic\.com\//,
            handler: "CacheFirst",
            options: {
              cacheName: "google-fonts-files",
              expiration: { maxEntries: 20, maxAgeSeconds: 60 * 60 * 24 * 365 },
              cacheableResponse: { statuses: [0, 200] },
            },
          },
        ],
      },
    }),
  ],
  build: {
    target: "es2022",
    outDir: "dist",
    rollupOptions: {
      // Dos entradas: el juego y el laboratorio genético.
      input: {
        main: resolve(__dirname, "index.html"),
        lab: resolve(__dirname, "lab.html"),
      },
    },
  },
  test: {
    environment: "node",
    include: ["test/**/*.test.ts"],
    /**
     * 30 s en vez de los 5 por defecto.
     *
     * Varios tests no son unitarios sino de propiedad: generan 1200 sprites y
     * los comparan píxel a píxel, o miden la distribución de rarezas sobre
     * 60.000 genomas con un Miller-Rabin cada uno. Eso son segundos de cómputo
     * real, no una espera.
     *
     * Con el límite por defecto pasaban en una máquina descargada y fallaban en
     * la misma máquina con otras cosas corriendo, que es exactamente el
     * comportamiento intermitente que vuelve inútil una suite. Sigue siendo un
     * techo que atrapa un bucle infinito.
     */
    testTimeout: 30_000,
  },
});
