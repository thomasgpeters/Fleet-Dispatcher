import { useCallback, useEffect, useRef, useState } from "react";
import {
  IonBadge,
  IonButton,
  IonContent,
  IonHeader,
  IonIcon,
  IonItem,
  IonLabel,
  IonList,
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import { micOutline, stopCircleOutline } from "ionicons/icons";

import { api } from "../api/client";
import { assist, type AssistAction } from "../api/assistant";
import {
  CURRENT_DRIVER_ID,
  CURRENT_USER_ID,
} from "../currentUser";

type Phase = "idle" | "listening" | "thinking" | "speaking";

/** Best-effort current GPS fix; resolves null if unavailable/denied. */
function getFix(): Promise<{ lat: number; lng: number } | null> {
  return new Promise((resolve) => {
    if (!navigator.geolocation) return resolve(null);
    navigator.geolocation.getCurrentPosition(
      (p) => resolve({ lat: p.coords.latitude, lng: p.coords.longitude }),
      () => resolve(null),
      { enableHighAccuracy: true, timeout: 8000 },
    );
  });
}

/**
 * "Hey Dispatch" — hands-free voice assistant. Push-to-talk: hold the button to
 * talk, release to send. STT and TTS run on-device (Web Speech API); the
 * transcript + the driver's context go to the assistant service, which thinks
 * and acts (message dispatch, ETA, route checks) and returns a spoken reply.
 */
export function AssistantPage() {
  const [phase, setPhase] = useState<Phase>("idle");
  const [transcript, setTranscript] = useState("");
  const [reply, setReply] = useState("");
  const [actions, setActions] = useState<AssistAction[]>([]);
  const [error, setError] = useState("");

  // Resolved destination from the driver's active trip (for ETA / route tools).
  const destRef = useRef<{ lat: number; lng: number; label?: string } | null>(
    null,
  );
  // The driver↔dispatcher channel (placeholder: first channel until auth lands).
  const channelRef = useRef<string | null>(null);
  const recRef = useRef<SpeechRecognition | null>(null);

  const supported =
    typeof window !== "undefined" &&
    (window.SpeechRecognition || window.webkitSpeechRecognition);

  // Prime context once: dispatcher channel + active-trip destination.
  useEffect(() => {
    api
      .listChannels()
      .then((cs) => {
        channelRef.current = cs[0]?.id ?? null;
      })
      .catch(() => undefined);

    api
      .tripsForDriver(CURRENT_DRIVER_ID)
      .then(async (trips) => {
        const trip = trips.find((t) => t.trip_status_id === 2) ?? trips[0];
        if (!trip) return;
        const wps = await api.waypointsForTrip(trip.id);
        const dest =
          wps.find((w) => w.stop_type_id === 2) ?? wps[wps.length - 1];
        if (dest) {
          destRef.current = { lat: dest.lat, lng: dest.lng, label: dest.label };
        }
      })
      .catch(() => undefined);
  }, []);

  const speak = useCallback((text: string) => {
    if (!("speechSynthesis" in window)) return;
    window.speechSynthesis.cancel();
    const u = new SpeechSynthesisUtterance(text);
    u.rate = 1.0;
    u.onend = () => setPhase("idle");
    setPhase("speaking");
    window.speechSynthesis.speak(u);
  }, []);

  const send = useCallback(
    async (text: string) => {
      const said = text.trim();
      if (!said) {
        setPhase("idle");
        return;
      }
      setPhase("thinking");
      setError("");
      setReply("");
      setActions([]);
      try {
        const fix = await getFix();
        const res = await assist({
          transcript: said,
          driver_id: CURRENT_DRIVER_ID,
          author_user_id: CURRENT_USER_ID,
          dispatcher_channel_id: channelRef.current ?? undefined,
          lat: fix?.lat,
          lng: fix?.lng,
          dest_lat: destRef.current?.lat,
          dest_lng: destRef.current?.lng,
          dest_label: destRef.current?.label,
        });
        setReply(res.reply);
        setActions(res.actions);
        speak(res.reply);
      } catch (e) {
        const msg = e instanceof Error ? e.message : String(e);
        setError(msg);
        setPhase("idle");
      }
    },
    [speak],
  );

  const startListening = useCallback(() => {
    if (!supported || phase === "listening" || phase === "thinking") return;
    window.speechSynthesis?.cancel();
    const Ctor = window.SpeechRecognition ?? window.webkitSpeechRecognition!;
    const rec = new Ctor();
    rec.lang = "en-US";
    rec.interimResults = true;
    rec.continuous = false;
    rec.maxAlternatives = 1;

    let finalText = "";
    rec.onresult = (ev) => {
      let interim = "";
      for (let i = ev.resultIndex; i < ev.results.length; i++) {
        const r = ev.results[i];
        if (r.isFinal) finalText += r[0].transcript;
        else interim += r[0].transcript;
      }
      setTranscript((finalText + interim).trim());
    };
    rec.onerror = (ev) => {
      setError(`Mic: ${ev.error}`);
      setPhase("idle");
    };
    rec.onend = () => {
      recRef.current = null;
      // Released → send whatever we captured.
      void send(finalText || transcript);
    };

    recRef.current = rec;
    setTranscript("");
    setError("");
    setPhase("listening");
    rec.start();
  }, [supported, phase, send, transcript]);

  const stopListening = useCallback(() => {
    recRef.current?.stop(); // fires onend → send
  }, []);

  const hint =
    phase === "listening"
      ? "Listening… release to send"
      : phase === "thinking"
        ? "Thinking…"
        : phase === "speaking"
          ? "Speaking…"
          : "Hold to talk";

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>Hey Dispatch</IonTitle>
        </IonToolbar>
      </IonHeader>
      <IonContent className="ion-padding">
        {!supported && (
          <IonText color="danger">
            <p>
              Voice input isn't supported in this browser. Try Chrome, or the
              installed app.
            </p>
          </IonText>
        )}

        <IonButton
          expand="block"
          size="large"
          color={phase === "listening" ? "danger" : "primary"}
          disabled={!supported || phase === "thinking" || phase === "speaking"}
          onMouseDown={startListening}
          onMouseUp={stopListening}
          onMouseLeave={stopListening}
          onTouchStart={(e) => {
            e.preventDefault();
            startListening();
          }}
          onTouchEnd={(e) => {
            e.preventDefault();
            stopListening();
          }}
        >
          <IonIcon
            slot="start"
            icon={phase === "listening" ? stopCircleOutline : micOutline}
          />
          {hint}
        </IonButton>

        {transcript && (
          <IonItem lines="none">
            <IonLabel className="ion-text-wrap">
              <IonNote>You said</IonNote>
              <p>{transcript}</p>
            </IonLabel>
          </IonItem>
        )}

        {reply && (
          <IonItem lines="none" color="light">
            <IonLabel className="ion-text-wrap">
              <IonNote>Dispatch</IonNote>
              <p>{reply}</p>
            </IonLabel>
          </IonItem>
        )}

        {actions.length > 0 && (
          <IonList>
            {actions.map((a, i) => (
              <IonItem key={i} lines="full">
                <IonBadge slot="start" color={a.ok ? "success" : "danger"}>
                  {a.ok ? "✓" : "!"}
                </IonBadge>
                <IonLabel className="ion-text-wrap">
                  <h3>{a.tool}</h3>
                  <p>{a.detail}</p>
                </IonLabel>
              </IonItem>
            ))}
          </IonList>
        )}

        {error && (
          <IonText color="danger">
            <p>{error}</p>
          </IonText>
        )}
      </IonContent>
    </IonPage>
  );
}
