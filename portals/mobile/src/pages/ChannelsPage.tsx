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

import {
  api,
  CHANNEL_TYPE,
  MEMBER_ROLE,
  MEMBER_STATUS,
} from "../api/client";
import type { Channel, ChannelMember } from "../api/types";

/** Human labels for the badges shown on each channel row. */
const CHANNEL_TYPE_LABEL: Record<number, string> = {
  [CHANNEL_TYPE.direct]: "Direct message",
  [CHANNEL_TYPE.group]: "Group",
  [CHANNEL_TYPE.broadcast]: "Broadcast",
};
const ROLE_LABEL: Record<number, string> = {
  [MEMBER_ROLE.owner]: "Owner",
  [MEMBER_ROLE.admin]: "Admin",
};
const STATUS_LABEL: Record<number, string> = {
  [MEMBER_STATUS.muted]: "Muted",
  [MEMBER_STATUS.banned]: "Banned",
};
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
  const [memberships, setMemberships] = useState<Record<string, ChannelMember>>({});
  const [error, setError] = useState<string | null>(null);

  const load = async () => {
    if (!user) return;
    try {
      const chs = await api.listChannels();
      setChannels(chs);

      const counts: Record<string, number> = {};
      const mine: Record<string, ChannelMember> = {};
      await Promise.all(
        chs.map(async (ch) => {
          const [membership, messages] = await Promise.all([
            api.myMembership(ch.id, user.id),
            api.messagesForChannel(ch.id),
          ]);
          if (membership) mine[ch.id] = membership;
          const lastRead = membership?.last_read_at ?? null;
          counts[ch.id] = messages.filter(
            (m) =>
              m.author_id !== user.id &&
              (!lastRead || m.posted_at > lastRead),
          ).length;
        }),
      );
      setUnread(counts);
      setMemberships(mine);
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  useEffect(() => {
    void load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [user]);

  // Realtime: bump unread straight from the Kafka stream (via the bridge) — no
  // DB re-read through ALS. The initial counts come from the snapshot `load()`.
  useEffect(() => {
    subscribe(["messages"]);
    const off = addListener((evt) => {
      if (evt.type !== "message") return;
      if (user && evt.author_id === user.id) return; // our own message
      const ch = String(evt.channel_id);
      setUnread((prev) => ({ ...prev, [ch]: (prev[ch] ?? 0) + 1 }));
    });
    return off;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [subscribe, addListener, user]);

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
          {channels.map((ch) => {
            const m = memberships[ch.id];
            const standing = m && STATUS_LABEL[m.member_status_id];
            const role = m && ROLE_LABEL[m.member_role_id];
            return (
              <IonItem key={ch.id} routerLink={`/messages/${ch.id}`} detail>
                <IonLabel>
                  <h2>{ch.name}</h2>
                  <IonNote>
                    {CHANNEL_TYPE_LABEL[ch.channel_type_id] ?? "Channel"}
                  </IonNote>
                </IonLabel>
                {/* Role / standing / unread badges at the row edge. */}
                {standing && (
                  <IonBadge slot="end" color="danger">
                    {standing}
                  </IonBadge>
                )}
                {role && (
                  <IonBadge slot="end" color="medium">
                    {role}
                  </IonBadge>
                )}
                {unread[ch.id] > 0 && (
                  <IonBadge slot="end" color="primary">
                    {unread[ch.id]}
                  </IonBadge>
                )}
              </IonItem>
            );
          })}
        </IonList>
      </IonContent>
    </IonPage>
  );
}
