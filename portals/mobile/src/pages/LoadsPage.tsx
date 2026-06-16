import { useEffect, useState } from "react";
import {
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
import { personCircleOutline } from "ionicons/icons";

import { api, API_BASE_URL } from "../api/client";
import type { Load } from "../api/types";

/**
 * The driver's board: this fetches loads from the shared JSON:API and lists
 * them. Connectivity errors are surfaced inline so it's obvious when the
 * middleware isn't reachable.
 */
export function LoadsPage() {
  const [loads, setLoads] = useState<Load[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api
      .listLoads()
      .then(setLoads)
      .catch((e: unknown) =>
        setError(e instanceof Error ? e.message : String(e)),
      )
      .finally(() => setLoading(false));
  }, []);

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>Fleet Dispatcher — Loads</IonTitle>
          <IonButtons slot="end">
            <IonButton routerLink="/profile" aria-label="Profile">
              <IonIcon slot="icon-only" icon={personCircleOutline} />
            </IonButton>
          </IonButtons>
        </IonToolbar>
      </IonHeader>
      <IonContent className="ion-padding">
        <IonNote>API: {API_BASE_URL}</IonNote>

        {loading && <IonText> Loading…</IonText>}

        {error && (
          <IonText color="danger">
            <p>Could not reach the middleware: {error}</p>
          </IonText>
        )}

        {!loading && !error && loads.length === 0 && (
          <IonText>
            <p>No loads found.</p>
          </IonText>
        )}

        <IonList>
          {loads.map((load) => (
            <IonItem key={load.id} routerLink={`/loads/${load.id}`} detail>
              <IonLabel>
                <h2>
                  {load.currency} {load.rate.toFixed(2)}
                </h2>
                <p>
                  {(load.deadhead_miles + load.loaded_miles).toFixed(1)} mi
                  total &middot; status #{load.load_status_id}
                </p>
              </IonLabel>
            </IonItem>
          ))}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
