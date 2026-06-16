import { useEffect, useRef, useState } from "react";
import {
  IonAvatar,
  IonBackButton,
  IonButton,
  IonButtons,
  IonContent,
  IonHeader,
  IonIcon,
  IonInput,
  IonItem,
  IonList,
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import { cameraOutline, logOutOutline, personCircleOutline } from "ionicons/icons";

import { api } from "../api/client";
import type { Document } from "../api/types";
import { useAuth } from "../auth/AuthContext";

const ROLE_LABEL: Record<number, string> = {
  1: "Dispatcher",
  2: "Driver",
  3: "Updater",
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

/** Account & profile: view/edit fields, set a photo (via the CMS), sign out. */
export function ProfilePage() {
  const { user, logout, refreshProfile } = useAuth();
  const [fullName, setFullName] = useState("");
  const [email, setEmail] = useState("");
  const [phone, setPhone] = useState("");
  const [title, setTitle] = useState("");
  const [timezone, setTimezone] = useState("");
  const [avatar, setAvatar] = useState<string | null>(null); // data URL
  const [status, setStatus] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const fileInput = useRef<HTMLInputElement>(null);

  // Seed the form from the signed-in user.
  useEffect(() => {
    if (!user) return;
    setFullName(user.full_name ?? "");
    setEmail(user.email ?? "");
    setPhone(user.phone ?? "");
    setTitle(user.title ?? "");
    setTimezone(user.timezone ?? "");
    if (user.avatar_document_id) {
      api
        .getDocument(user.avatar_document_id)
        .then((d: Document) => {
          if (d.data) setAvatar(`data:${d.content_type};base64,${d.data}`);
        })
        .catch(() => undefined);
    } else {
      setAvatar(null);
    }
  }, [user]);

  if (!user) return null;

  const save = async () => {
    setBusy(true);
    setStatus(null);
    try {
      await api.updateUser(user.id, {
        full_name: fullName.trim(),
        email: email.trim() || undefined,
        phone: phone.trim() || undefined,
        title: title.trim() || undefined,
        timezone: timezone.trim() || undefined,
      });
      await refreshProfile();
      setStatus("Profile saved.");
    } catch (e) {
      setStatus(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  const onPickPhoto = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    e.target.value = "";
    if (!file) return;
    setBusy(true);
    setStatus(null);
    try {
      const base64 = await fileToBase64(file);
      const doc = await api.createDocument({
        title: `Avatar — ${user.username}`,
        document_type_id: 2, // image
        filename: file.name,
        content_type: file.type || "image/jpeg",
        byte_size: file.size,
        data: base64,
        uploaded_by: user.id,
      });
      await api.updateUser(user.id, { avatar_document_id: doc.id });
      setAvatar(`data:${doc.content_type};base64,${base64}`);
      await refreshProfile();
      setStatus("Photo updated.");
    } catch (err) {
      setStatus(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(false);
    }
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonButtons slot="start">
            <IonBackButton defaultHref="/loads" />
          </IonButtons>
          <IonTitle>Profile</IonTitle>
          <IonButtons slot="end">
            <IonButton color="medium" onClick={logout}>
              <IonIcon slot="start" icon={logOutOutline} />
              Sign out
            </IonButton>
          </IonButtons>
        </IonToolbar>
      </IonHeader>
      <IonContent className="ion-padding">
        <div style={{ display: "flex", alignItems: "center", gap: 16, marginBottom: 12 }}>
          <IonAvatar style={{ width: 72, height: 72 }}>
            {avatar ? (
              <img src={avatar} alt="Profile" />
            ) : (
              <IonIcon icon={personCircleOutline} style={{ fontSize: 72 }} />
            )}
          </IonAvatar>
          <div>
            <IonText>
              <h2 style={{ margin: 0 }}>{user.full_name}</h2>
            </IonText>
            <IonNote>
              @{user.username} · {ROLE_LABEL[user.app_role_id] ?? "User"}
            </IonNote>
            <div>
              <IonButton
                size="small"
                fill="clear"
                onClick={() => fileInput.current?.click()}
              >
                <IonIcon slot="start" icon={cameraOutline} />
                Change photo
              </IonButton>
            </div>
          </div>
        </div>

        <IonList>
          <IonItem>
            <IonInput
              label="Full name"
              labelPlacement="stacked"
              value={fullName}
              onIonInput={(e) => setFullName(e.detail.value ?? "")}
            />
          </IonItem>
          <IonItem>
            <IonInput
              label="Email"
              labelPlacement="stacked"
              type="email"
              value={email}
              onIonInput={(e) => setEmail(e.detail.value ?? "")}
            />
          </IonItem>
          <IonItem>
            <IonInput
              label="Phone"
              labelPlacement="stacked"
              value={phone}
              onIonInput={(e) => setPhone(e.detail.value ?? "")}
            />
          </IonItem>
          <IonItem>
            <IonInput
              label="Title"
              labelPlacement="stacked"
              value={title}
              onIonInput={(e) => setTitle(e.detail.value ?? "")}
            />
          </IonItem>
          <IonItem>
            <IonInput
              label="Time zone"
              labelPlacement="stacked"
              placeholder="America/Chicago"
              value={timezone}
              onIonInput={(e) => setTimezone(e.detail.value ?? "")}
            />
          </IonItem>
        </IonList>

        {status && (
          <IonText color="medium">
            <p>{status}</p>
          </IonText>
        )}

        <IonButton
          expand="block"
          className="ion-margin-top"
          disabled={busy}
          onClick={() => void save()}
        >
          Save profile
        </IonButton>

        <input
          ref={fileInput}
          type="file"
          accept="image/*"
          hidden
          onChange={(e) => void onPickPhoto(e)}
        />
      </IonContent>
    </IonPage>
  );
}
