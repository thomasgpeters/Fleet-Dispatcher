import { Redirect, Route } from "react-router-dom";
import {
  IonApp,
  IonIcon,
  IonLabel,
  IonRouterOutlet,
  IonSpinner,
  IonTabBar,
  IonTabButton,
  IonTabs,
} from "@ionic/react";
import { IonReactRouter } from "@ionic/react-router";
import {
  chatbubblesOutline,
  listOutline,
  locateOutline,
  micOutline,
  navigateOutline,
} from "ionicons/icons";

import { LoadsPage } from "./pages/LoadsPage";
import { LoadDetailPage } from "./pages/LoadDetailPage";
import { ChannelsPage } from "./pages/ChannelsPage";
import { ChannelPage } from "./pages/ChannelPage";
import { SavedPage } from "./pages/SavedPage";
import { LocatePage } from "./pages/LocatePage";
import { TripsPage } from "./pages/TripsPage";
import { TripDetailPage } from "./pages/TripDetailPage";
import { TripWaypointsPage } from "./pages/TripWaypointsPage";
import { AssistantPage } from "./pages/AssistantPage";
import { LoginPage } from "./pages/LoginPage";
import { ProfilePage } from "./pages/ProfilePage";
import { AuthProvider, useAuth } from "./auth/AuthContext";
import { RealtimeProvider } from "./realtime/RealtimeContext";

/* Ionic router add-on CSS. */
import "@ionic/react/css/padding.css";

/**
 * Root: wrap everything in the auth provider, then gate on a session — show the
 * login screen until authenticated, otherwise the tabbed app. Each tab is a
 * list → detail stack with native transitions and back buttons (KISS).
 */
export default function App() {
  return (
    <IonApp>
      <AuthProvider>
        <Gate />
      </AuthProvider>
    </IonApp>
  );
}

/** Decide between the login screen and the authenticated app. */
function Gate() {
  const { ready, token, user } = useAuth();

  if (!ready) {
    return (
      <div style={{ display: "grid", placeItems: "center", height: "100%" }}>
        <IonSpinner name="dots" />
      </div>
    );
  }
  if (!token || !user) return <LoginPage />;
  return <AuthedApp />;
}

function AuthedApp() {
  return (
    <RealtimeProvider>
    <IonReactRouter>
      <IonTabs>
        <IonRouterOutlet>
          <Route exact path="/loads" component={LoadsPage} />
          <Route exact path="/loads/:loadId" component={LoadDetailPage} />
          <Route exact path="/trips" component={TripsPage} />
          <Route exact path="/trips/:tripId" component={TripDetailPage} />
          <Route exact path="/trips/:tripId/waypoints" component={TripWaypointsPage} />
          <Route exact path="/messages" component={ChannelsPage} />
          <Route exact path="/saved" component={SavedPage} />
          <Route exact path="/messages/:channelId" component={ChannelPage} />
          <Route exact path="/locate" component={LocatePage} />
          <Route exact path="/assistant" component={AssistantPage} />
          <Route exact path="/profile" component={ProfilePage} />
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
          <IonTabButton tab="assistant" href="/assistant">
            <IonIcon icon={micOutline} />
            <IonLabel>Dispatch</IonLabel>
          </IonTabButton>
        </IonTabBar>
      </IonTabs>
    </IonReactRouter>
    </RealtimeProvider>
  );
}
