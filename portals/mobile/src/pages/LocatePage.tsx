import { useState } from "react";
import {
  IonButton,
  IonContent,
  IonHeader,
  IonIcon,
  IonNote,
  IonPage,
  IonText,
  IonTitle,
  IonToolbar,
} from "@ionic/react";
import { locateOutline } from "ionicons/icons";

import { api } from "../api/client";
import { PHONE_PUSH_SOURCE_ID } from "../currentUser";
import { useAuth } from "../auth/AuthContext";

const MS_TO_MPH = 2.23694;

/**
 * Driver "share my location": reads the device GPS and pushes a position report
 * (phone_push) for the current rig. Feeds the dispatcher HUD's truck locations.
 */
export function LocatePage() {
  const { driverId, equipmentId } = useAuth();
  const [status, setStatus] = useState<string>("");
  const [last, setLast] = useState<{ lat: number; lng: number } | null>(null);
  const [busy, setBusy] = useState(false);

  const share = () => {
    if (!navigator.geolocation) {
      setStatus("Geolocation isn't available on this device/context.");
      return;
    }
    setBusy(true);
    setStatus("Getting your location…");
    navigator.geolocation.getCurrentPosition(
      (pos) => {
        const { latitude, longitude, accuracy, heading, speed } = pos.coords;
        api
          .postPosition({
            location_source_id: PHONE_PUSH_SOURCE_ID,
            driver_id: driverId ?? undefined,
            equipment_id: equipmentId ?? undefined,
            lat: latitude,
            lng: longitude,
            accuracy_m: accuracy ?? undefined,
            heading_deg: heading ?? undefined,
            speed_mph: speed != null ? speed * MS_TO_MPH : undefined,
          })
          .then(() => {
            setLast({ lat: latitude, lng: longitude });
            setStatus("Location shared.");
          })
          .catch((e: unknown) =>
            setStatus(e instanceof Error ? e.message : String(e)),
          )
          .finally(() => setBusy(false));
      },
      (err) => {
        setStatus(`Location error: ${err.message}`);
        setBusy(false);
      },
      { enableHighAccuracy: true, timeout: 15000 },
    );
  };

  return (
    <IonPage>
      <IonHeader>
        <IonToolbar>
          <IonTitle>Share Location</IonTitle>
        </IonToolbar>
      </IonHeader>
      <IonContent className="ion-padding">
        <IonButton expand="block" onClick={share} disabled={busy}>
          <IonIcon slot="start" icon={locateOutline} />
          Share my location
        </IonButton>

        {status && (
          <IonText>
            <p>{status}</p>
          </IonText>
        )}
        {last && (
          <IonNote>
            Last shared: {last.lat.toFixed(5)}, {last.lng.toFixed(5)}
          </IonNote>
        )}
      </IonContent>
    </IonPage>
  );
}
