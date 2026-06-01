import { IonApp } from "@ionic/react";

import { LoadsPage } from "./pages/LoadsPage";

/**
 * Root of the Driver / Updater mobile app. Single page for now; add
 * IonReactRouter and more pages (schedule, post-trip inspection, fuel/DEF log)
 * as the portal grows.
 */
export default function App() {
  return (
    <IonApp>
      <LoadsPage />
    </IonApp>
  );
}
