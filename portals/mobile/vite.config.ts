import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Vite 5 config for the Ionic/React mobile portal.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
  },
});
