import React from "react";
import { createRoot } from "react-dom/client";
import { setupIonicReact } from "@ionic/react";

import App from "./App";

/* Core Ionic CSS — required for components to render correctly. */
import "@ionic/react/css/core.css";
import "@ionic/react/css/normalize.css";
import "@ionic/react/css/structure.css";
import "@ionic/react/css/typography.css";

setupIonicReact();

const container = document.getElementById("root");
if (!container) {
  throw new Error("Root container #root not found");
}

createRoot(container).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
