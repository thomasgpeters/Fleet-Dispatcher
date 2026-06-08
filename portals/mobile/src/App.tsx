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
import {
  chatbubblesOutline,
  listOutline,
  locateOutline,
  navigateOutline,
} from "ionicons/icons";

import { LoadsPage } from "./pages/LoadsPage";
import { LoadDetailPage } from "./pages/LoadDetailPage";
import { ChannelsPage } from "./pages/ChannelsPage";
import { ChannelPage } from "./pages/ChannelPage";
import { LocatePage } from "./pages/LocatePage";
import { TripsPage } from "./pages/TripsPage";
import { TripDetailPage } from "./pages/TripDetailPage";

/* Ionic router add-on CSS. */
import "@ionic/react/css/padding.css";

/**
 * Root of the Driver / Updater mobile app: tabbed nav, each tab a list → detail
 * stack with native transitions and back buttons (KISS). Add more tabs
 * (schedule, navigation) as the portal grows.
 */
export default function App() {
  return (
    <IonApp>
      <IonReactRouter>
        <IonTabs>
          <IonRouterOutlet>
            <Route exact path="/loads" component={LoadsPage} />
            <Route exact path="/loads/:loadId" component={LoadDetailPage} />
            <Route exact path="/trips" component={TripsPage} />
            <Route exact path="/trips/:tripId" component={TripDetailPage} />
            <Route exact path="/messages" component={ChannelsPage} />
            <Route exact path="/messages/:channelId" component={ChannelPage} />
            <Route exact path="/locate" component={LocatePage} />
            <Route exact path="/">
              <Redirect to="/loads" />
            </Route>
          </IonRouterOutlet>
          <IonTabBar slot="bottom">
            <IonTabButton tab="loads" href="/loads">
              <IonIcon icon={listOutline} />
              <IonLabel>Loads</IonLabel>
            </IonTabButton>
            <IonTabButton tab="trips" href="/trips">
              <IonIcon icon={navigateOutline} />
              <IonLabel>Trips</IonLabel>
            </IonTabButton>
            <IonTabButton tab="messages" href="/messages">
              <IonIcon icon={chatbubblesOutline} />
              <IonLabel>Messages</IonLabel>
            </IonTabButton>
            <IonTabButton tab="locate" href="/locate">
              <IonIcon icon={locateOutline} />
              <IonLabel>Locate</IonLabel>
            </IonTabButton>
          </IonTabBar>
        </IonTabs>
      </IonReactRouter>
    </IonApp>
  );
}
