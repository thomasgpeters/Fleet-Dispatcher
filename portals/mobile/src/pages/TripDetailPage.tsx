import { useEffect, useState } from "react";
import { useParams } from "react-router-dom";
import {
  IonBackButton,
  IonButton,
  IonButtons,
  IonContent,
  IonHeader,
  IonIcon,
  IonItem,
  IonLabel,
  IonList,
  IonListHeader,
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import { createOutline, navigateOutline } from "ionicons/icons";

import { api } from "../api/client";
import type { Route, Trip, Waypoint } from "../api/types";

const STOP_TYPE = [
  "",
  "origin",
  "destination",
  "waypoint",
  "fuel",
  "rest",
  "truck stop",
  "lunch",
  "load stop",
];

/**
 * Trip overview: the route summary + a read-only list of stops, kept uncluttered.
 * Editing the route (add/remove waypoints) is a drill-in — "Edit waypoints" →
 * /trips/:tripId/waypoints.
 */
export function TripDetailPage() {
  const { tripId } = useParams<{ tripId: string }>();
  const [trip, setTrip] = useState<Trip | null>(null);
  const [stops, setStops] = useState<Waypoint[]>([]);
  const [route, setRoute] = useState<Route | null>(null);
  const [error, setError] = useState<string | null>(null);

  const load = async () => {
    try {
      const [t, w, r] = await Promise.all([
        api.getTrip(tripId),
        api.waypointsForTrip(tripId),
        api.routeForTrip(tripId).catch(() => null),
      ]);
      setTrip(t);
      setStops([...w].sort((a, b) => a.seq - b.seq));
      setRoute(r);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  useEffect(() => {
    void load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tripId]);

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton defaultHref="/trips" />
          </IonButtons>
          <IonTitle>{trip?.name ?? "Trip"}</IonTitle>
          <IonButtons slot="end">
            <IonButton routerLink={`/trips/${tripId}/waypoints`}>
              <IonIcon slot="start" icon={createOutline} />
              Edit waypoints
            </IonButton>
          </IonButtons>
        </IonToolbar>
      </IonHeader>
      <IonContent>
        {error && (
          <IonText color="danger">
            <p className="ion-padding">{error}</p>
          </IonText>
        )}

        {route && (route.distance_mi != null || route.drive_minutes != null) && (
          <IonItem lines="full">
            <IonIcon slot="start" icon={navigateOutline} color="primary" />
            <IonLabel>
              <h2>Route</h2>
              <IonNote>
                {route.distance_mi != null && `${route.distance_mi.toFixed(0)} mi`}
                {route.distance_mi != null && route.drive_minutes != null && " · "}
                {route.drive_minutes != null &&
                  `${Math.floor(route.drive_minutes / 60)}h ${route.drive_minutes % 60}m`}
              </IonNote>
            </IonLabel>
          </IonItem>
        )}

        <IonList>
          <IonListHeader>
            <IonLabel>Stops ({stops.length})</IonLabel>
          </IonListHeader>
          {stops.map((w) => (
            <IonItem key={w.id}>
              <IonLabel>
                <h2>
                  {w.seq}. {w.label ?? STOP_TYPE[w.stop_type_id]}
                </h2>
                <IonNote>
                  {STOP_TYPE[w.stop_type_id]} · {w.lat.toFixed(4)},{" "}
                  {w.lng.toFixed(4)}
                  {w.arrived_at ? " · arrived" : ""}
                </IonNote>
              </IonLabel>
            </IonItem>
          ))}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
