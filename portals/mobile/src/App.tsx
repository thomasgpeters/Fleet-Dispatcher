import { Redirect, Route } from "react-router-dom";
import {
  IonApp,
  IonIcon,
  IonLabel,
  IonRouterOutlet,
  IonTabBar,
  IonTabButton,
  IonTabs,
} from "@ionic/react";
import { IonReactRouter } from "@ionic/react-router";
import { chatbubblesOutline, listOutline } from "ionicons/icons";

import { LoadsPage } from "./pages/LoadsPage";
import { MessagesPage } from "./pages/MessagesPage";

/* Ionic router add-on CSS. */
import "@ionic/react/css/padding.css";

/**
 * Root of the Driver / Updater mobile app: tabbed navigation over the
 * shared JSON:API. Add more tabs (schedule, post-trip inspection, fuel/DEF log,
 * navigation) as the portal grows.
 */
export default function App() {
  return (
    <IonApp>
      <IonReactRouter>
        <IonTabs>
          <IonRouterOutlet>
            <Route exact path="/loads" component={LoadsPage} />
            <Route exact path="/messages" component={MessagesPage} />
            <Route exact path="/">
              <Redirect to="/loads" />
            </Route>
          </IonRouterOutlet>
          <IonTabBar slot="bottom">
            <IonTabButton tab="loads" href="/loads">
              <IonIcon icon={listOutline} />
              <IonLabel>Loads</IonLabel>
            </IonTabButton>
            <IonTabButton tab="messages" href="/messages">
              <IonIcon icon={chatbubblesOutline} />
              <IonLabel>Messages</IonLabel>
            </IonTabButton>
          </IonTabBar>
        </IonTabs>
      </IonReactRouter>
    </IonApp>
  );
}
