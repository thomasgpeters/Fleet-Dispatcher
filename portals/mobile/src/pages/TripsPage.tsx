import { useEffect, useState } from "react";
import {
  IonButton,
  IonButtons,
  IonContent,
  IonHeader,
  IonItem,
  IonLabel,
  IonList,
  IonNote,
  IonPage,
  IonRefresher,
  IonRefresherContent,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import type { RefresherCustomEvent } from "@ionic/react";

import { api } from "../api/client";
import type { Trip } from "../api/types";
import { CURRENT_DRIVER_ID, CURRENT_EQUIPMENT_ID } from "../currentUser";

const TRIP_STATUS = ["", "planned", "active", "completed", "cancelled"];
const TRIP_STATUS_PLANNED = 1;

/** The driver's trips: list + start a new (planned) trip. */
export function TripsPage() {
  const [trips, setTrips] = useState<Trip[]>([]);
  const [error, setError] = useState<string | null>(null);

  const load = () =>
    api
      .tripsForDriver(CURRENT_DRIVER_ID)
      .then(setTrips)
      .catch((e: unknown) =>
        setError(e instanceof Error ? e.message : String(e)),
      );

  useEffect(() => {
    void load();
  }, []);

  const startTrip = async () => {
    try {
      await api.createTrip({
        driver_id: CURRENT_DRIVER_ID,
        equipment_id: CURRENT_EQUIPMENT_ID,
        trip_status_id: TRIP_STATUS_PLANNED,
        name: "New trip",
      });
      await load();
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  const refresh = async (e: RefresherCustomEvent) => {
    await load();
    await e.detail.complete();
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>Trips</IonTitle>
          <IonButtons slot="end">
            <IonButton onClick={() => void startTrip()}>Start trip</IonButton>
          </IonButtons>
        </IonToolbar>
      </IonHeader>
      <IonContent>
        <IonRefresher slot="fixed" onIonRefresh={(e) => void refresh(e)}>
          <IonRefresherContent />
        </IonRefresher>
        {error && (
          <IonText color="danger">
            <p className="ion-padding">Could not reach the middleware: {error}</p>
          </IonText>
        )}
        <IonList>
          {trips.map((t) => (
            <IonItem key={t.id} routerLink={`/trips/${t.id}`} detail>
              <IonLabel>
                <h2>{t.name ?? "Trip"}</h2>
                <IonNote>{TRIP_STATUS[t.trip_status_id] ?? "?"}</IonNote>
              </IonLabel>
            </IonItem>
          ))}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
