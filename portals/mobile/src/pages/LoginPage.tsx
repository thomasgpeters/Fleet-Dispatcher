import { useState } from "react";
import {
  IonButton,
  IonContent,
  IonHeader,
  IonInput,
  IonItem,
  IonList,
  IonNote,
  IonPage,
  IonSpinner,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";

import { useAuth } from "../auth/AuthContext";

/**
 * Sign-in screen. Shown full-screen (no tabs) until a session exists; on success
 * the auth context flips and the app renders the tabbed UI. Authenticates via
 * ApiLogicServer's built-in JWT auth (POST /api/auth/login).
 */
export function LoginPage() {
  const { login } = useAuth();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const submit = async () => {
    if (!username.trim() || !password) return;
    setBusy(true);
    setError(null);
    try {
      await login(username.trim(), password);
      // No navigation needed — the auth state change re-renders into the app.
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>Fleet Dispatcher</IonTitle>
        </IonToolbar>
      </IonHeader>
      <IonContent className="ion-padding">
        <IonText color="medium">
          <p>Sign in to your dispatcher, driver, or updater account.</p>
        </IonText>
        <IonList>
          <IonItem>
            <IonInput
              label="Username"
              labelPlacement="stacked"
              autocapitalize="off"
              value={username}
              onIonInput={(e) => setUsername(e.detail.value ?? "")}
              placeholder="e.g. driver1"
            />
          </IonItem>
          <IonItem>
            <IonInput
              label="Password"
              labelPlacement="stacked"
              type="password"
              value={password}
              onIonInput={(e) => setPassword(e.detail.value ?? "")}
              onKeyDown={(e) => {
                if (e.key === "Enter") void submit();
              }}
            />
          </IonItem>
        </IonList>

        {error && (
          <IonText color="danger">
            <p>{error}</p>
          </IonText>
        )}

        <IonButton
          expand="block"
          className="ion-margin-top"
          disabled={busy || !username.trim() || !password}
          onClick={() => void submit()}
        >
          {busy ? <IonSpinner name="dots" /> : "Sign in"}
        </IonButton>

        <IonNote className="ion-margin-top" style={{ display: "block" }}>
          Demo: <code>driver1</code> / <code>dispatch1</code> / <code>updater1</code>,
          password <code>fleet123</code>.
        </IonNote>
      </IonContent>
    </IonPage>
  );
}
