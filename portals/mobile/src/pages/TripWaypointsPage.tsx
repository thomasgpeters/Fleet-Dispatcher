import { useEffect, useState } from "react";
import { useParams } from "react-router-dom";
import {
  IonActionSheet,
  IonBackButton,
  IonButton,
  IonButtons,
  IonContent,
  IonHeader,
  IonIcon,
  IonItem,
  IonItemOption,
  IonItemOptions,
  IonItemSliding,
  IonLabel,
  IonList,
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import { addOutline, trashOutline } from "ionicons/icons";

import { api } from "../api/client";
import type { Waypoint } from "../api/types";

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

// Stop types a driver can add en route. Origin/destination are set at planning
// time; these are the mutable additions (kept off the map view to reduce clutter).
const ADDABLE: { id: number; label: string }[] = [
  { id: 4, label: "Fuel stop" },
  { id: 7, label: "Lunch stop" },
  { id: 8, label: "Load stop (multi-load)" },
  { id: 6, label: "Truck stop" },
  { id: 3, label: "Waypoint" },
];

/**
 * Edit waypoints — the mutable GPS route for a trip. Drilled into from the trip
 * overview so the map/overview stays uncluttered. Add a stop at the current
 * location (fuel/lunch/multi-load/…) or remove an intermediate one; origin and
 * destination are fixed.
 */
export function TripWaypointsPage() {
  const { tripId } = useParams<{ tripId: string }>();
  const [stops, setStops] = useState<Waypoint[]>([]);
  const [pickType, setPickType] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const fail = (e: unknown) =>
    setError(e instanceof Error ? e.message : String(e));

  const load = async () => {
    try {
      const w = await api.waypointsForTrip(tripId);
      setStops([...w].sort((a, b) => a.seq - b.seq));
    } catch (e) {
      fail(e);
    }
  };

  useEffect(() => {
    void load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tripId]);

  // Add a stop of the chosen type at the driver's current GPS location, inserted
  // before the destination so it stays last on the route.
  const addStop = (stopTypeId: number, label: string) => {
    if (!navigator.geolocation) {
      setError("Geolocation isn't available here.");
      return;
    }
    navigator.geolocation.getCurrentPosition(
      (pos) => {
        const dest = stops.find((s) => s.stop_type_id === 2);
        const seq = dest
          ? dest.seq
          : (stops.length ? stops[stops.length - 1].seq : 0) + 1;

        const doAdd = () =>
          api
            .addWaypoint({
              trip_id: tripId,
              seq,
              stop_type_id: stopTypeId,
              lat: pos.coords.latitude,
              lng: pos.coords.longitude,
              label,
            })
            .then(() => load())
            .catch(fail);

        if (dest) {
          // Bump destination (and anything after) down a slot — back to front to
          // respect UNIQUE (trip_id, seq).
          Promise.all(
            stops
              .filter((s) => s.seq >= seq)
              .sort((a, b) => b.seq - a.seq)
              .map((s) => api.updateWaypoint(s.id, { seq: s.seq + 1 })),
          )
            .then(doAdd)
            .catch(fail);
        } else {
          void doAdd();
        }
      },
      (err) => setError(`Location error: ${err.message}`),
      { enableHighAccuracy: true, timeout: 15000 },
    );
  };

  const removeStop = (w: Waypoint) => {
    api.deleteWaypoint(w.id).then(() => load()).catch(fail);
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton defaultHref={`/trips/${tripId}`} />
          </IonButtons>
          <IonTitle>Edit waypoints</IonTitle>
          <IonButtons slot="end">
            <IonButton onClick={() => setPickType(true)}>
              <IonIcon slot="start" icon={addOutline} />
              Add
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
            <IonItemSliding key={w.id}>
              <IonItem>
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
              {w.stop_type_id !== 1 && w.stop_type_id !== 2 && (
                <IonItemOptions slot="end">
                  <IonItemOption color="danger" onClick={() => removeStop(w)}>
                    <IonIcon slot="icon-only" icon={trashOutline} />
                  </IonItemOption>
                </IonItemOptions>
              )}
            </IonItemSliding>
          ))}
        </IonList>
        <IonNote className="ion-padding" style={{ display: "block" }}>
          Swipe a stop to remove it. Origin and destination are fixed.
        </IonNote>
      </IonContent>

      <IonActionSheet
        isOpen={pickType}
        header="Add a stop at your location"
        onDidDismiss={() => setPickType(false)}
        buttons={[
          ...ADDABLE.map((t) => ({
            text: t.label,
            handler: () => addStop(t.id, t.label),
          })),
          { text: "Cancel", role: "cancel" as const },
        ]}
      />
    </IonPage>
  );
}
