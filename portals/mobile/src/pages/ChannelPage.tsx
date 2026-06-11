import { useEffect, useRef, useState } from "react";
import { useParams } from "react-router-dom";
import {
  IonActionSheet,
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
  attachOutline,
  bookmark,
  bookmarkOutline,
  documentOutline,
  happyOutline,
  pin,
  pinOutline,
  send,
} from "ionicons/icons";

import { EmojiPicker } from "../components/EmojiPicker";
import { api, PIN_SCOPE } from "../api/client";
import type {
  Channel,
  Document,
  Message,
  MessagePin,
  SavedMessage,
} from "../api/types";
import { CURRENT_USER_ID } from "../currentUser";

/** Human label for a pin scope (pin_scope.code). */
const SCOPE_LABEL: Record<number, string> = {
  [PIN_SCOPE.self]: "only me",
  [PIN_SCOPE.channel]: "channel",
  [PIN_SCOPE.everyone]: "everyone",
};

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
  const { channelId } = useParams<{ channelId: string }>();
  const [channel, setChannel] = useState<Channel | null>(null);
  const [messages, setMessages] = useState<Message[]>([]);
  const [docsByMessage, setDocsByMessage] = useState<Record<string, Document[]>>({});
  const [pins, setPins] = useState<MessagePin[]>([]);
  const [myPins, setMyPins] = useState<Record<string, MessagePin>>({});
  const [saved, setSaved] = useState<Record<string, SavedMessage>>({});
  const [scopeFor, setScopeFor] = useState<string | null>(null); // message awaiting a scope pick
  const [draft, setDraft] = useState("");
  const [emojiOpen, setEmojiOpen] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const fileInput = useRef<HTMLInputElement>(null);

  const fail = (e: unknown) =>
    setError(e instanceof Error ? e.message : String(e));

  const load = async () => {
    try {
      const [ch, msgs] = await Promise.all([
        api.getChannel(channelId),
        api.messagesForChannel(channelId),
      ]);
      setChannel(ch);
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
      const visible = await api.visiblePinsForChannel(channelId, CURRENT_USER_ID);
      setPins(visible);
      const mine: Record<string, MessagePin> = {};
      for (const p of visible) {
        if (p.pinned_by === CURRENT_USER_ID) mine[p.message_id] = p;
      }
      setMyPins(mine);

      const savedRows = await api.savedForUser(CURRENT_USER_ID);
      const sMap: Record<string, SavedMessage> = {};
      for (const s of savedRows) sMap[s.message_id] = s;
      setSaved(sMap);

      // Entering a channel marks it read for the current user.
      void api.markChannelRead(channelId, CURRENT_USER_ID).catch(() => {});
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

  const send_ = async () => {
    if (!draft.trim()) return;
    try {
      const msg = await api.createMessage(channelId, CURRENT_USER_ID, draft.trim());
      setMessages((prev) => [...prev, msg]);
      setDraft("");
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
        : await api.pinMessage(messageId, channelId, CURRENT_USER_ID, scopeId);
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
        const s = await api.saveMessage(CURRENT_USER_ID, m.id);
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
        CURRENT_USER_ID,
        `📎 ${file.name}`,
      );
      const doc = await api.createDocument({
        title: file.name,
        document_type_id: file.type.startsWith("image/") ? 2 : 1, // image | document
        filename: file.name,
        content_type: file.type || "application/octet-stream",
        byte_size: file.size,
        data: base64,
        uploaded_by: CURRENT_USER_ID,
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

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton defaultHref="/messages" />
          </IonButtons>
          <IonTitle>{channel?.name ?? "Channel"}</IonTitle>
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

        <IonList>
          {messages.map((m) => (
            <IonItemSliding key={m.id}>
              <IonItem lines="none">
                <IonLabel className="ion-text-wrap">
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

      <IonFooter>
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
