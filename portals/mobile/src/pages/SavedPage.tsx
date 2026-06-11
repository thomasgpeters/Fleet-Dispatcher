import { useEffect, useState } from "react";
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
  IonRefresher,
  IonRefresherContent,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import type { RefresherCustomEvent } from "@ionic/react";
import { bookmark, trashOutline } from "ionicons/icons";

import { api } from "../api/client";
import type { Message, SavedMessage } from "../api/types";
import { CURRENT_USER_ID } from "../currentUser";

type SavedRow = { saved: SavedMessage; message: Message | null };

/**
 * Personal archive — the user's saved messages across every channel, newest
 * first. Distinct from channels/groups (organization) and from pins (shared
 * importance): this collection is per-user. Reached from the Message Board
 * header; swipe/tap to remove an item.
 */
export function SavedPage() {
  const [rows, setRows] = useState<SavedRow[]>([]);
  const [error, setError] = useState<string | null>(null);

  const load = async () => {
    try {
      const saved = await api.savedForUser(CURRENT_USER_ID);
      // Resolve each saved entry's message body (best-effort; a deleted message
      // leaves the entry with a null body rather than failing the whole list).
      const resolved = await Promise.all(
        saved.map(async (s) => ({
          saved: s,
          message: await api.getMessage(s.message_id).catch(() => null),
        })),
      );
      setRows(resolved);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  useEffect(() => {
    void load();
  }, []);

  const refresh = async (e: RefresherCustomEvent) => {
    await load();
    await e.detail.complete();
  };

  const remove = async (s: SavedMessage) => {
    try {
      await api.unsaveMessage(s.id);
      setRows((prev) => prev.filter((r) => r.saved.id !== s.id));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton defaultHref="/messages" />
          </IonButtons>
          <IonTitle>
            <IonIcon icon={bookmark} /> Saved
          </IonTitle>
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
        {rows.length === 0 && !error && (
          <IonText color="medium">
            <p className="ion-padding">
              No saved messages yet. Swipe a message and tap the bookmark to keep
              it here.
            </p>
          </IonText>
        )}
        <IonList>
          {rows.map(({ saved, message }) => (
            <IonItem key={saved.id} lines="full">
              <IonLabel className="ion-text-wrap">
                <p>{message?.body ?? "(message unavailable)"}</p>
                {saved.note && <IonNote>note: {saved.note}</IonNote>}
                <IonNote> · saved {new Date(saved.saved_at).toLocaleString()}</IonNote>
              </IonLabel>
              <IonButton
                slot="end"
                fill="clear"
                color="medium"
                onClick={() => void remove(saved)}
              >
                <IonIcon slot="icon-only" icon={trashOutline} />
              </IonButton>
            </IonItem>
          ))}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
