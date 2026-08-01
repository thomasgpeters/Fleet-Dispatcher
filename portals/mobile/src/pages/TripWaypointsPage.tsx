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
  IonLabel,
  IonList,
  IonNote,
  IonPage,
  IonReorder,
  IonReorderGroup,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import type { ItemReorderEventDetail } from "@ionic/react";
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

  // A manual edit (add/remove/reorder) locks the trip's order so the route
  // optimizer won't auto-reorder it. (Geometry recompute still runs server-side.)
  const lockOrder = () =>
    api.updateTrip(tripId, { route_locked: true }).catch(() => undefined);

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

  // Add a stop of the chosen type at the driver's current GPS location. We send
  // only a trivial append hint (max seq + 1) — the middleware repositions an
  // intermediate stop to just before the destination and shifts the rest
  // (route_governance.py). No client-side seq arithmetic / bump loop. Locks FIRST
  // (and waits) so the auto-optimizer — which reads route_locked from the DB —
  // won't reorder the manual change.
  const applyAdd = async (
    pos: GeolocationPosition,
    stopTypeId: number,
    label: string,
  ) => {
    try {
      await lockOrder();
      const seq = stops.length ? Math.max(...stops.map((s) => s.seq)) + 1 : 1;
      await api.addWaypoint({
        trip_id: tripId,
        seq,
        stop_type_id: stopTypeId,
        lat: pos.coords.latitude,
        lng: pos.coords.longitude,
        label,
      });
      await load();
    } catch (e) {
      fail(e);
    }
  };

  const addStop = (stopTypeId: number, label: string) => {
    if (!navigator.geolocation) {
      setError("Geolocation isn't available here.");
      return;
    }
    navigator.geolocation.getCurrentPosition(
      (pos) => void applyAdd(pos, stopTypeId, label),
      (err) => setError(`Location error: ${err.message}`),
      { enableHighAccuracy: true, timeout: 15000 },
    );
  };

  const removeStop = async (w: Waypoint) => {
    try {
      await lockOrder();
      await api.deleteWaypoint(w.id);
      await load();
    } catch (e) {
      fail(e);
    }
  };

  // Drag-reorder: a single gesture is a single move, so we PATCH only the moved
  // stop to its target slot (seq = destination index + 1). The middleware shifts
  // the crossed range to keep the order dense 1..N (route_governance.py) — no
  // client-side two-phase offset dance. Lock FIRST so the optimizer leaves it be.
  const moveStop = async (id: string, seq: number) => {
    try {
      await lockOrder();
      await api.updateWaypoint(id, { seq });
      await load();
    } catch (e) {
      fail(e);
    }
  };

  const handleReorder = (e: CustomEvent<ItemReorderEventDetail>) => {
    const { from, to } = e.detail;
    e.detail.complete(); // finish the DOM move
    const moved = stops[from];
    const reordered = [...stops];
    reordered.splice(from, 1);
    reordered.splice(to, 0, moved);
    setStops(reordered.map((w, i) => ({ ...w, seq: i + 1 })));
    void moveStop(moved.id, to + 1); // dense 1..N → target seq is the 1-based slot
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
          <IonReorderGroup disabled={false} onIonItemReorder={handleReorder}>
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
                {/* Origin/destination stay put; intermediate stops are removable. */}
                {w.stop_type_id !== 1 && w.stop_type_id !== 2 && (
                  <IonButton
                    slot="end"
                    fill="clear"
                    color="danger"
                    onClick={() => removeStop(w)}
                  >
                    <IonIcon slot="icon-only" icon={trashOutline} />
                  </IonButton>
                )}
                <IonReorder slot="end" />
              </IonItem>
            ))}
          </IonReorderGroup>
        </IonList>
        <IonNote className="ion-padding" style={{ display: "block" }}>
          Drag the handle to reorder stops; tap the trash to remove one. Origin and
          destination are fixed. The route recomputes automatically.
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
