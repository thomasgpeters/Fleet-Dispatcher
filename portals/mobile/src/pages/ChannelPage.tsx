import { useEffect, useRef, useState } from "react";
import { useParams } from "react-router-dom";
import {
  IonActionSheet,
  IonAlert,
  IonBackButton,
  IonButton,
  IonButtons,
  IonChip,
  IonContent,
  IonFooter,
  IonHeader,
  IonIcon,
  IonInput,
  IonItem,
  IonItemOption,
  IonItemOptions,
  IonItemSliding,
  IonLabel,
  IonList,
  IonListHeader,
  IonNote,
  IonPage,
  IonPopover,
  IonRefresher,
  IonRefresherContent,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import type { RefresherCustomEvent } from "@ionic/react";
import {
  arrowUndoOutline,
  attachOutline,
  addOutline,
  bookmark,
  bookmarkOutline,
  chatbubblesOutline,
  close,
  documentOutline,
  happyOutline,
  lockClosedOutline,
  pin,
  pinOutline,
  returnDownForwardOutline,
  send,
} from "ionicons/icons";

import { EmojiPicker } from "../components/EmojiPicker";
import { api, canManageTopics, PIN_SCOPE, postBlockReason } from "../api/client";
import { useAuth } from "../auth/AuthContext";
import { useRealtime } from "../realtime/RealtimeContext";
import type {
  Channel,
  ChannelMember,
  ChannelTopic,
  Document,
  Message,
  MessagePin,
  SavedMessage,
} from "../api/types";

/** Human label for a pin scope (pin_scope.code). */
const SCOPE_LABEL: Record<number, string> = {
  [PIN_SCOPE.self]: "only me",
  [PIN_SCOPE.channel]: "channel",
  [PIN_SCOPE.everyone]: "everyone",
};

/** A short single-line preview of a message body for reply quotes. */
function snippet(body: string, max = 80): string {
  const oneLine = body.replace(/\s+/g, " ").trim();
  return oneLine.length > max ? `${oneLine.slice(0, max - 1)}…` : oneLine;
}

/** Strip the "data:<mime>;base64," prefix, returning just the base64 payload. */
function fileToBase64(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result).split(",")[1] ?? "");
    reader.onerror = () => reject(reader.error);
    reader.readAsDataURL(file);
  });
}

/**
 * Channel detail — the conversation timeline with attachments and a composer.
 * Reached via a native push from the channel list, so it carries a back button.
 * Each message can be pinned (with a chosen visibility scope) or saved to the
 * user's personal archive; visible pins surface in a strip at the top.
 */
export function ChannelPage() {
  // topicId is present when viewing a forum topic (drill-in); absent = the
  // channel's General stream.
  const { channelId, topicId } = useParams<{ channelId: string; topicId?: string }>();
  const { user } = useAuth();
  const { subscribe, addListener } = useRealtime();
  const userId = user?.id ?? "";
  const [channel, setChannel] = useState<Channel | null>(null);
  const [membership, setMembership] = useState<ChannelMember | null>(null);
  const [topics, setTopics] = useState<ChannelTopic[]>([]);   // channel mode
  const [topic, setTopic] = useState<ChannelTopic | null>(null);  // topic mode
  const [messages, setMessages] = useState<Message[]>([]);
  const [docsByMessage, setDocsByMessage] = useState<Record<string, Document[]>>({});
  const [pins, setPins] = useState<MessagePin[]>([]);
  const [myPins, setMyPins] = useState<Record<string, MessagePin>>({});
  const [saved, setSaved] = useState<Record<string, SavedMessage>>({});
  const [scopeFor, setScopeFor] = useState<string | null>(null); // message awaiting a scope pick
  const [replyTo, setReplyTo] = useState<Message | null>(null);
  const [draft, setDraft] = useState("");
  const [emojiOpen, setEmojiOpen] = useState(false);
  const [newTopicOpen, setNewTopicOpen] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const fileInput = useRef<HTMLInputElement>(null);

  const fail = (e: unknown) =>
    setError(e instanceof Error ? e.message : String(e));

  const load = async () => {
    try {
      const [ch, msgs, myMember, tps] = await Promise.all([
        api.getChannel(channelId),
        api.messagesForChannel(channelId),
        api.myMembership(channelId, userId),
        topicId ? api.getTopic(topicId) : api.topicsForChannel(channelId),
      ]);
      setChannel(ch);
      setMembership(myMember);  // drives composer gating (P1/P2)
      if (topicId) {
        setTopic(tps as ChannelTopic);
      } else {
        setTopics(tps as ChannelTopic[]);
      }
      const sorted = [...msgs].sort((a, b) => a.posted_at.localeCompare(b.posted_at));
      setMessages(sorted);

      // Resolve attachment metadata per message (no base64 `data` — fetched
      // on tap to keep the timeline light).
      const map: Record<string, Document[]> = {};
      for (const m of sorted) {
        const links = await api.attachmentsForMessage(m.id);
        if (links.length) {
          map[m.id] = await Promise.all(
            links.map((l) => api.getDocumentMeta(l.document_id)),
          );
        }
      }
      setDocsByMessage(map);

      // Pins visible to me + my own pins (for toggle state); my saved messages.
      const visible = await api.visiblePinsForChannel(channelId, userId);
      setPins(visible);
      const mine: Record<string, MessagePin> = {};
      for (const p of visible) {
        if (p.pinned_by === userId) mine[p.message_id] = p;
      }
      setMyPins(mine);

      const savedRows = await api.savedForUser(userId);
      const sMap: Record<string, SavedMessage> = {};
      for (const s of savedRows) sMap[s.message_id] = s;
      setSaved(sMap);

      // Entering a channel marks it read for the current user.
      void api.markChannelRead(channelId, userId).catch(() => {});
    } catch (e) {
      fail(e);
    }
  };

  const refresh = async (e: RefresherCustomEvent) => {
    await load();
    await e.detail.complete();
  };

  useEffect(() => {
    void load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [channelId]);

  // Realtime: the live conversation comes from the Kafka stream (via the bridge),
  // not by re-reading the DB through ALS. The initial `load()` is the snapshot;
  // every new message is applied directly from the event payload (de-duped by id,
  // which also absorbs our own optimistic append on send).
  useEffect(() => {
    subscribe([`channel:${channelId}`]);
    const off = addListener((evt) => {
      if (evt.type !== "message" || evt.channel_id !== channelId) return;
      const incoming: Message = {
        id: String(evt.id),
        channel_id: String(evt.channel_id),
        topic_id: evt.topic_id ? String(evt.topic_id) : null,
        author_id: String(evt.author_id),
        body: String(evt.body ?? ""),
        posted_at: String(evt.posted_at ?? new Date().toISOString()),
        reply_to_id: evt.reply_to_id ? String(evt.reply_to_id) : undefined,
      };
      setMessages((prev) =>
        prev.some((m) => m.id === incoming.id)
          ? prev
          : [...prev, incoming].sort((a, b) =>
              a.posted_at.localeCompare(b.posted_at),
            ),
      );
    });
    return off;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [channelId, subscribe, addListener]);

  const send_ = async () => {
    if (!draft.trim()) return;
    try {
      const msg = await api.createMessage(
        channelId,
        userId,
        draft.trim(),
        replyTo?.id,
        topicId,  // undefined = General stream
      );
      setMessages((prev) => [...prev, msg]);
      setDraft("");
      setReplyTo(null);
    } catch (e) {
      fail(e);
    }
  };

  // --- pin / save actions ---

  const pinWithScope = async (messageId: string, scopeId: number) => {
    try {
      const existing = myPins[messageId];
      const saved_ = existing
        ? await api.repinMessage(existing.id, scopeId)
        : await api.pinMessage(messageId, channelId, userId, scopeId);
      setMyPins((prev) => ({ ...prev, [messageId]: saved_ }));
      setPins((prev) => [
        ...prev.filter((p) => p.id !== saved_.id),
        saved_,
      ]);
    } catch (e) {
      fail(e);
    }
  };

  const unpin = async (p: MessagePin) => {
    try {
      await api.unpinMessage(p.id);
      setMyPins((prev) => {
        const next = { ...prev };
        delete next[p.message_id];
        return next;
      });
      setPins((prev) => prev.filter((x) => x.id !== p.id));
    } catch (e) {
      fail(e);
    }
  };

  const toggleSave = async (m: Message) => {
    try {
      const existing = saved[m.id];
      if (existing) {
        await api.unsaveMessage(existing.id);
        setSaved((prev) => {
          const next = { ...prev };
          delete next[m.id];
          return next;
        });
      } else {
        const s = await api.saveMessage(userId, m.id);
        setSaved((prev) => ({ ...prev, [m.id]: s }));
      }
    } catch (e) {
      fail(e);
    }
  };

  const onPickFile = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    e.target.value = ""; // allow re-picking the same file
    if (!file) return;
    try {
      const base64 = await fileToBase64(file);
      const msg = await api.createMessage(
        channelId,
        userId,
        `📎 ${file.name}`,
        undefined,
        topicId,  // attach within the current topic (or General)
      );
      const doc = await api.createDocument({
        title: file.name,
        document_type_id: file.type.startsWith("image/") ? 2 : 1, // image | document
        filename: file.name,
        content_type: file.type || "application/octet-stream",
        byte_size: file.size,
        data: base64,
        uploaded_by: userId,
      });
      await api.linkMessageDocument(msg.id, doc.id);
      setMessages((prev) => [...prev, msg]);
      setDocsByMessage((prev) => ({ ...prev, [msg.id]: [doc] }));
    } catch (err) {
      fail(err);
    }
  };

  const openDocument = async (doc: Document) => {
    try {
      const full = doc.data ? doc : await api.getDocument(doc.id);
      if (full.data) {
        window.open(`data:${full.content_type};base64,${full.data}`, "_blank");
      }
    } catch (e) {
      fail(e);
    }
  };

  const pinnedMessages = pins
    .map((p) => ({ pin: p, msg: messages.find((m) => m.id === p.message_id) }))
    .filter((x): x is { pin: MessagePin; msg: Message } => Boolean(x.msg));

  // For rendering reply quotes: resolve a reply_to_id to its message.
  const messagesById: Record<string, Message> = {};
  for (const m of messages) messagesById[m.id] = m;

  // Topic mode shows just that topic's messages; channel mode shows the General
  // stream (messages with no topic). Topics are reached via the list below.
  const timeline = messages.filter((m) =>
    topicId ? m.topic_id === topicId : !m.topic_id,
  );

  // P1/P2 governance: hide the composer with a reason when the user can't post.
  const blockReason = postBlockReason(channel, membership);
  // Only admins/dispatchers add topics (mirrors the server rule).
  const canAddTopics = canManageTopics(membership, user?.app_role_id);

  const createTopic_ = async (name: string) => {
    const trimmed = name.trim();
    if (!trimmed) return;
    try {
      const t = await api.createTopic(channelId, trimmed, userId);
      setTopics((prev) => [...prev, t]);
    } catch (e) {
      fail(e);
    }
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton
              defaultHref={topicId ? `/messages/${channelId}` : "/messages"}
            />
          </IonButtons>
          <IonTitle>
            {topicId
              ? (topic?.name ?? "Topic")
              : (channel?.name ?? "Channel")}
          </IonTitle>
        </IonToolbar>
      </IonHeader>

      <IonContent className="ion-padding">
        <IonRefresher slot="fixed" onIonRefresh={(e) => void refresh(e)}>
          <IonRefresherContent />
        </IonRefresher>
        {error && (
          <IonText color="danger">
            <p>Could not reach the middleware: {error}</p>
          </IonText>
        )}

        {pinnedMessages.length > 0 && (
          <IonList>
            <IonListHeader>
              <IonLabel>
                <IonIcon icon={pin} /> Pinned
              </IonLabel>
            </IonListHeader>
            {pinnedMessages.map(({ pin: p, msg }) => (
              <IonItem key={p.id} lines="none">
                <IonLabel className="ion-text-wrap">
                  <p>{msg.body}</p>
                  <IonNote>pinned · {SCOPE_LABEL[p.pin_scope_id]}</IonNote>
                </IonLabel>
                {myPins[msg.id] && (
                  <IonButton
                    slot="end"
                    fill="clear"
                    onClick={() => void unpin(myPins[msg.id])}
                  >
                    <IonIcon slot="icon-only" icon={pin} />
                  </IonButton>
                )}
              </IonItem>
            ))}
          </IonList>
        )}

        {/* Topics (channel mode): tap to drill into a focused thread. Admins/
            dispatchers can add topics; regular members only browse them. */}
        {!topicId && (topics.length > 0 || canAddTopics) && (
          <IonList>
            <IonListHeader>
              <IonLabel>
                <IonIcon icon={chatbubblesOutline} /> Topics
              </IonLabel>
              {canAddTopics && (
                <IonButton onClick={() => setNewTopicOpen(true)}>
                  <IonIcon slot="start" icon={addOutline} />
                  New
                </IonButton>
              )}
            </IonListHeader>
            {topics.map((t) => (
              <IonItem
                key={t.id}
                button
                detail
                routerLink={`/messages/${channelId}/topics/${t.id}`}
              >
                <IonLabel>
                  {t.name}
                  {t.is_closed && <IonNote> · closed</IonNote>}
                </IonLabel>
              </IonItem>
            ))}
          </IonList>
        )}

        <IonList>
          {timeline.map((m) => (
            <IonItemSliding key={m.id}>
              <IonItem lines="none">
                <IonLabel className="ion-text-wrap">
                  {m.reply_to_id && (
                    <IonNote
                      className="ion-text-wrap"
                      style={{
                        display: "block",
                        borderLeft: "3px solid var(--ion-color-medium)",
                        paddingLeft: 8,
                        marginBottom: 4,
                        fontStyle: "italic",
                      }}
                    >
                      <IonIcon icon={returnDownForwardOutline} />{" "}
                      {messagesById[m.reply_to_id]
                        ? snippet(messagesById[m.reply_to_id].body)
                        : "quoted message"}
                    </IonNote>
                  )}
                  <p>{m.body}</p>
                  {(docsByMessage[m.id] ?? []).map((doc) => (
                    <IonChip key={doc.id} onClick={() => void openDocument(doc)}>
                      <IonIcon icon={documentOutline} />
                      <IonLabel>{doc.filename}</IonLabel>
                    </IonChip>
                  ))}
                  <IonNote>{new Date(m.posted_at).toLocaleString()}</IonNote>
                </IonLabel>
                {/* Inline state markers (also adjustable via swipe). */}
                {myPins[m.id] && (
                  <IonIcon slot="end" icon={pin} color="medium" aria-label="pinned" />
                )}
                {saved[m.id] && (
                  <IonIcon
                    slot="end"
                    icon={bookmark}
                    color="medium"
                    aria-label="saved"
                  />
                )}
              </IonItem>
              <IonItemOptions slot="start">
                <IonItemOption color="success" onClick={() => setReplyTo(m)}>
                  <IonIcon slot="icon-only" icon={arrowUndoOutline} />
                </IonItemOption>
              </IonItemOptions>
              <IonItemOptions slot="end">
                <IonItemOption
                  color="primary"
                  onClick={() =>
                    myPins[m.id] ? void unpin(myPins[m.id]) : setScopeFor(m.id)
                  }
                >
                  <IonIcon slot="icon-only" icon={myPins[m.id] ? pin : pinOutline} />
                </IonItemOption>
                <IonItemOption color="tertiary" onClick={() => void toggleSave(m)}>
                  <IonIcon
                    slot="icon-only"
                    icon={saved[m.id] ? bookmark : bookmarkOutline}
                  />
                </IonItemOption>
              </IonItemOptions>
            </IonItemSliding>
          ))}
        </IonList>
      </IonContent>

      {/* Scope picker shown when pinning a message. */}
      <IonActionSheet
        isOpen={scopeFor !== null}
        header="Pin for…"
        onDidDismiss={() => setScopeFor(null)}
        buttons={[
          {
            text: "Only me",
            handler: () => {
              if (scopeFor) void pinWithScope(scopeFor, PIN_SCOPE.self);
            },
          },
          {
            text: "Everyone in this channel",
            handler: () => {
              if (scopeFor) void pinWithScope(scopeFor, PIN_SCOPE.channel);
            },
          },
          {
            text: "Everyone",
            handler: () => {
              if (scopeFor) void pinWithScope(scopeFor, PIN_SCOPE.everyone);
            },
          },
          { text: "Cancel", role: "cancel" },
        ]}
      />

      {/* New-topic prompt (admins/dispatchers only). */}
      <IonAlert
        isOpen={newTopicOpen}
        header="New topic"
        onDidDismiss={() => setNewTopicOpen(false)}
        inputs={[{ name: "name", type: "text", placeholder: "Topic name" }]}
        buttons={[
          { text: "Cancel", role: "cancel" },
          {
            text: "Create",
            handler: (data) => {
              void createTopic_(String(data?.name ?? ""));
            },
          },
        ]}
      />

      <IonFooter>
        {blockReason ? (
          // Read-only: a single clean notice in place of the composer.
          <IonToolbar>
            <IonItem lines="none">
              <IonIcon slot="start" icon={lockClosedOutline} color="medium" />
              <IonLabel className="ion-text-wrap">
                <IonNote>{blockReason}</IonNote>
              </IonLabel>
            </IonItem>
          </IonToolbar>
        ) : (
        <>
        {replyTo && (
          <IonToolbar>
            <IonItem lines="none">
              <IonIcon slot="start" icon={returnDownForwardOutline} color="medium" />
              <IonLabel className="ion-text-wrap">
                <IonNote>Replying to</IonNote>
                <p>{snippet(replyTo.body)}</p>
              </IonLabel>
              <IonButton
                slot="end"
                fill="clear"
                color="medium"
                onClick={() => setReplyTo(null)}
                aria-label="Cancel reply"
              >
                <IonIcon slot="icon-only" icon={close} />
              </IonButton>
            </IonItem>
          </IonToolbar>
        )}
        <IonToolbar>
          <IonButtons slot="start">
            <IonButton onClick={() => fileInput.current?.click()}>
              <IonIcon slot="icon-only" icon={attachOutline} />
            </IonButton>
            <IonButton id="emoji-trigger" onClick={() => setEmojiOpen(true)}>
              <IonIcon slot="icon-only" icon={happyOutline} />
            </IonButton>
          </IonButtons>
          <IonInput
            value={draft}
            placeholder="Message…"
            onIonInput={(e) => setDraft(e.detail.value ?? "")}
            onKeyDown={(e) => {
              if (e.key === "Enter") void send_();
            }}
          />
          <IonButtons slot="end">
            <IonButton strong disabled={!draft.trim()} onClick={() => void send_()}>
              <IonIcon slot="icon-only" icon={send} />
            </IonButton>
          </IonButtons>
        </IonToolbar>
        </>
        )}
      </IonFooter>

      {/* Emoji picker for the composer (appends to the draft). */}
      <IonPopover
        trigger="emoji-trigger"
        isOpen={emojiOpen}
        onDidDismiss={() => setEmojiOpen(false)}
        side="top"
        alignment="start"
      >
        <EmojiPicker onSelect={(e) => setDraft((prev) => prev + e)} />
      </IonPopover>

      {/* Hidden native file picker driven by the attach button. */}
      <input
        ref={fileInput}
        type="file"
        hidden
        onChange={(e) => void onPickFile(e)}
      />
    </IonPage>
  );
}
