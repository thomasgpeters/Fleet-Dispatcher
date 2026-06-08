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
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import { addOutline } from "ionicons/icons";

import { api } from "../api/client";
import type { Trip, Waypoint } from "../api/types";

const STOP_TYPE = ["", "origin", "destination", "waypoint", "fuel", "rest", "truck stop"];
const STOP_TYPE_WAYPOINT = 3;

/** Trip detail: ordered waypoints + "add current location" as the next stop. */
export function TripDetailPage() {
  const { tripId } = useParams<{ tripId: string }>();
  const [trip, setTrip] = useState<Trip | null>(null);
  const [stops, setStops] = useState<Waypoint[]>([]);
  const [error, setError] = useState<string | null>(null);

  const fail = (e: unknown) =>
    setError(e instanceof Error ? e.message : String(e));

  const load = async () => {
    try {
      const [t, w] = await Promise.all([
        api.getTrip(tripId),
        api.waypointsForTrip(tripId),
      ]);
      setTrip(t);
      setStops([...w].sort((a, b) => a.seq - b.seq));
    } catch (e) {
      fail(e);
    }
  };

  useEffect(() => {
    void load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tripId]);

  const addHere = () => {
    if (!navigator.geolocation) {
      setError("Geolocation isn't available here.");
      return;
    }
    navigator.geolocation.getCurrentPosition(
      (pos) => {
        const nextSeq = (stops.length ? stops[stops.length - 1].seq : 0) + 1;
        api
          .addWaypoint({
            trip_id: tripId,
            seq: nextSeq,
            stop_type_id: STOP_TYPE_WAYPOINT,
            lat: pos.coords.latitude,
            lng: pos.coords.longitude,
            label: `Stop ${nextSeq}`,
          })
          .then(() => load())
          .catch(fail);
      },
      (err) => setError(`Location error: ${err.message}`),
      { enableHighAccuracy: true, timeout: 15000 },
    );
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton defaultHref="/trips" />
          </IonButtons>
          <IonTitle>{trip?.name ?? "Trip"}</IonTitle>
          <IonButtons slot="end">
            <IonButton onClick={addHere}>
              <IonIcon slot="start" icon={addOutline} />
              Add stop
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
        <IonList>
          {stops.map((w) => (
            <IonItem key={w.id}>
              <IonLabel>
                <h2>
                  {w.seq}. {w.label ?? STOP_TYPE[w.stop_type_id]}
                </h2>
                <IonNote>
                  {STOP_TYPE[w.stop_type_id]} · {w.lat.toFixed(4)},{" "}
                  {w.lng.toFixed(4)}
                </IonNote>
              </IonLabel>
            </IonItem>
          ))}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
