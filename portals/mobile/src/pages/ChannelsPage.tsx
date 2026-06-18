import { useEffect, useState } from "react";
import {
  IonBadge,
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
import { bookmarkOutline } from "ionicons/icons";

import { api } from "../api/client";
import type { Channel } from "../api/types";
import { useAuth } from "../auth/AuthContext";
import { useRealtime } from "../realtime/RealtimeContext";

/**
 * Message board — channel list with unread badges. Tapping a channel pushes its
 * detail page (`/messages/:channelId`) with a native transition + back button.
 *
 * Unread is computed client-side from `channel_member.last_read_at` vs message
 * times. A server-side `unread_count` (LogicBank) is the production approach
 * (see docs/TODO.md → middleware rules).
 */
export function ChannelsPage() {
  const { user } = useAuth();
  const { subscribe, addListener } = useRealtime();
  const [channels, setChannels] = useState<Channel[]>([]);
  const [unread, setUnread] = useState<Record<string, number>>({});
  const [error, setError] = useState<string | null>(null);

  const load = async () => {
    if (!user) return;
    try {
      const chs = await api.listChannels();
      setChannels(chs);

      const counts: Record<string, number> = {};
      await Promise.all(
        chs.map(async (ch) => {
          const [membership, messages] = await Promise.all([
            api.myMembership(ch.id, user.id),
            api.messagesForChannel(ch.id),
          ]);
          const lastRead = membership?.last_read_at ?? null;
          counts[ch.id] = messages.filter(
            (m) =>
              m.author_id !== user.id &&
              (!lastRead || m.posted_at > lastRead),
          ).length;
        }),
      );
      setUnread(counts);
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  useEffect(() => {
    void load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [user]);

  // Realtime: a message in any channel bumps unread live (poll-free).
  useEffect(() => {
    subscribe(["messages"]);
    const off = addListener((evt) => {
      if (evt.type === "message") void load();
    });
    return off;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [subscribe, addListener]);

  const refresh = async (e: RefresherCustomEvent) => {
    await load();
    await e.detail.complete();
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>Message Board</IonTitle>
          <IonButtons slot="end">
            <IonButton routerLink="/saved" aria-label="Saved messages">
              <IonIcon slot="icon-only" icon={bookmarkOutline} />
            </IonButton>
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
          {channels.map((ch) => (
            <IonItem key={ch.id} routerLink={`/messages/${ch.id}`} detail>
              <IonLabel>
                <h2>{ch.name}</h2>
                <IonNote>channel #{ch.channel_type_id}</IonNote>
              </IonLabel>
              {unread[ch.id] > 0 && (
                <IonBadge slot="end" color="primary">
                  {unread[ch.id]}
                </IonBadge>
              )}
            </IonItem>
          ))}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
