import { useEffect, useRef, useState } from "react";
import { useParams } from "react-router-dom";
import {
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
  IonLabel,
  IonList,
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import { attachOutline, documentOutline, send } from "ionicons/icons";

import { api } from "../api/client";
import type { Channel, Document, Message } from "../api/types";

// Placeholder for the signed-in user (driver1 from the seed). Replace with the
// authenticated identity once auth lands (see TODO.md).
const CURRENT_USER_ID = "22222222-2222-2222-2222-222222222222";

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
 */
export function ChannelPage() {
  const { channelId } = useParams<{ channelId: string }>();
  const [channel, setChannel] = useState<Channel | null>(null);
  const [messages, setMessages] = useState<Message[]>([]);
  const [docsByMessage, setDocsByMessage] = useState<Record<string, Document[]>>({});
  const [draft, setDraft] = useState("");
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

      // Resolve attachments per message (metadata + base64 for small docs).
      const map: Record<string, Document[]> = {};
      for (const m of sorted) {
        const links = await api.attachmentsForMessage(m.id);
        if (links.length) {
          map[m.id] = await Promise.all(
            links.map((l) => api.getDocument(l.document_id)),
          );
        }
      }
      setDocsByMessage(map);
    } catch (e) {
      fail(e);
    }
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

  const openDocument = (doc: Document) => {
    if (!doc.data) return;
    window.open(`data:${doc.content_type};base64,${doc.data}`, "_blank");
  };

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
        {error && (
          <IonText color="danger">
            <p>Could not reach the middleware: {error}</p>
          </IonText>
        )}
        <IonList>
          {messages.map((m) => (
            <IonItem key={m.id} lines="none">
              <IonLabel className="ion-text-wrap">
                <p>{m.body}</p>
                {(docsByMessage[m.id] ?? []).map((doc) => (
                  <IonChip key={doc.id} onClick={() => openDocument(doc)}>
                    <IonIcon icon={documentOutline} />
                    <IonLabel>{doc.filename}</IonLabel>
                  </IonChip>
                ))}
                <IonNote>{new Date(m.posted_at).toLocaleString()}</IonNote>
              </IonLabel>
            </IonItem>
          ))}
        </IonList>
      </IonContent>

      <IonFooter>
        <IonToolbar>
          <IonButtons slot="start">
            <IonButton onClick={() => fileInput.current?.click()}>
              <IonIcon slot="icon-only" icon={attachOutline} />
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
