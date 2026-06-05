import { useEffect, useState } from "react";
import { useParams } from "react-router-dom";
import {
  IonBackButton,
  IonButtons,
  IonContent,
  IonHeader,
  IonItem,
  IonLabel,
  IonList,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";

import { api } from "../api/client";
import type { Load } from "../api/types";

/**
 * Load detail — reached by tapping a load in the list (native push + back
 * button). Read-only for now; add status actions and linked documents next.
 */
export function LoadDetailPage() {
  const { loadId } = useParams<{ loadId: string }>();
  const [load, setLoad] = useState<Load | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    api
      .getLoad(loadId)
      .then(setLoad)
      .catch((e: unknown) =>
        setError(e instanceof Error ? e.message : String(e)),
      );
  }, [loadId]);

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton defaultHref="/loads" />
          </IonButtons>
          <IonTitle>Load</IonTitle>
        </IonToolbar>
      </IonHeader>
      <IonContent className="ion-padding">
        {error && (
          <IonText color="danger">
            <p>Could not reach the middleware: {error}</p>
          </IonText>
        )}
        {load && (
          <IonList>
            <IonItem>
              <IonLabel>
                <h2>Rate</h2>
                <p>
                  {load.currency} {load.rate.toFixed(2)}
                </p>
              </IonLabel>
            </IonItem>
            <IonItem>
              <IonLabel>
                <h2>Miles</h2>
                <p>
                  {load.deadhead_miles.toFixed(1)} dead-head +{" "}
                  {load.loaded_miles.toFixed(1)} loaded
                </p>
              </IonLabel>
            </IonItem>
            <IonItem>
              <IonLabel>
                <h2>Run / status</h2>
                <p>
                  run #{load.run_type_id} &middot; status #{load.load_status_id}
                </p>
              </IonLabel>
            </IonItem>
          </IonList>
        )}
      </IonContent>
    </IonPage>
  );
}
