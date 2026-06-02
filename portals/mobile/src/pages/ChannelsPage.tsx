import { useEffect, useState } from "react";
import {
  IonContent,
  IonHeader,
  IonItem,
  IonLabel,
  IonList,
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";

import { api } from "../api/client";
import type { Channel } from "../api/types";

/**
 * Message board — channel list. Tapping a channel pushes its detail page
 * (`/messages/:channelId`) with a native transition + back button.
 */
export function ChannelsPage() {
  const [channels, setChannels] = useState<Channel[]>([]);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    api
      .listChannels()
      .then(setChannels)
      .catch((e: unknown) =>
        setError(e instanceof Error ? e.message : String(e)),
      );
  }, []);

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>Message Board</IonTitle>
        </IonToolbar>
      </IonHeader>
      <IonContent>
        {error && (
          <IonText color="danger">
            <p className="ion-padding">Could not reach the middleware: {error}</p>
          </IonText>
        )}
        <IonList>
          {channels.map((ch) => (
            <IonItem key={ch.id} routerLink={`/messages/${ch.id}`} detail>
              <IonLabel>
                <h2>{ch.name}</h2>
                <IonNote>channel #{ch.channel_type_id}</IonNote>
              </IonLabel>
            </IonItem>
          ))}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
