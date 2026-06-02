import { useEffect, useState } from "react";
import {
  IonButton,
  IonButtons,
  IonContent,
  IonFooter,
  IonHeader,
  IonInput,
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
import type { Channel, Message } from "../api/types";

// Placeholder for the signed-in user (driver1 from the seed). Replace with the
// authenticated identity once auth lands (see TODO.md).
const CURRENT_USER_ID = "22222222-2222-2222-2222-222222222222";

/**
 * Message board: lists channels, and drills into a channel's messages.
 * Documents attach via the CMS (message_document); this scaffold shows the
 * conversation timeline — wire attachment previews/upload next.
 */
export function MessagesPage() {
  const [channels, setChannels] = useState<Channel[]>([]);
  const [active, setActive] = useState<Channel | null>(null);
  const [messages, setMessages] = useState<Message[]>([]);
  const [draft, setDraft] = useState("");
  const [error, setError] = useState<string | null>(null);

  const byPostedAt = (a: Message, b: Message) =>
    a.posted_at.localeCompare(b.posted_at);

  const send = async () => {
    if (!active || !draft.trim()) return;
    try {
      const msg = await api.createMessage(active.id, CURRENT_USER_ID, draft.trim());
      setMessages((prev) => [...prev, msg].sort(byPostedAt));
      setDraft("");
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  useEffect(() => {
    api
      .listChannels()
      .then(setChannels)
      .catch((e: unknown) =>
        setError(e instanceof Error ? e.message : String(e)),
      );
  }, []);

  useEffect(() => {
    if (!active) return;
    api
      .messagesForChannel(active.id)
      .then((m) => setMessages([...m].sort(byPostedAt)))
      .catch((e: unknown) =>
        setError(e instanceof Error ? e.message : String(e)),
      );
  }, [active]);

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          {active && (
            <IonButtons slot="start">
              <IonButton onClick={() => setActive(null)}>Back</IonButton>
            </IonButtons>
          )}
          <IonTitle>{active ? active.name : "Message Board"}</IonTitle>
        </IonToolbar>
      </IonHeader>
      <IonContent className="ion-padding">
        {error && (
          <IonText color="danger">
            <p>Could not reach the middleware: {error}</p>
          </IonText>
        )}

        {!active && (
          <IonList>
            {channels.map((ch) => (
              <IonItem key={ch.id} button onClick={() => setActive(ch)}>
                <IonLabel>
                  <h2>{ch.name}</h2>
                  <IonNote>channel #{ch.channel_type_id}</IonNote>
                </IonLabel>
              </IonItem>
            ))}
          </IonList>
        )}

        {active && (
          <IonList>
            {messages.map((m) => (
              <IonItem key={m.id} lines="none">
                <IonLabel className="ion-text-wrap">
                  <p>{m.body}</p>
                  <IonNote>{new Date(m.posted_at).toLocaleString()}</IonNote>
                </IonLabel>
              </IonItem>
            ))}
          </IonList>
        )}
      </IonContent>

      {active && (
        <IonFooter>
          <IonToolbar>
            <IonInput
              value={draft}
              placeholder="Message…"
              onIonInput={(e) => setDraft(e.detail.value ?? "")}
              onKeyDown={(e) => {
                if (e.key === "Enter") void send();
              }}
            />
            <IonButtons slot="end">
              <IonButton strong disabled={!draft.trim()} onClick={() => void send()}>
                Send
              </IonButton>
            </IonButtons>
          </IonToolbar>
        </IonFooter>
      )}
    </IonPage>
  );
}
