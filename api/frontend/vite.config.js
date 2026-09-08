import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Atlas V4 frontend build config.
//
// The dev server proxies every /api/* request to the Node/Express backend
// so React code can always call fetch("/api/...") without knowing the
// backend's host or port. This keeps the frontend portable between local
// development and future deployments.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      "/api": {
        target: "http://localhost:3000",
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: "dist",
  },
});
