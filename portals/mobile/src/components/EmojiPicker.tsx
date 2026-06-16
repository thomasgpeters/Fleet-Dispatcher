import { IonContent } from "@ionic/react";

/**
 * A tiny, dependency-free emoji picker. A curated set (with trucking-relevant
 * symbols) keeps the bundle lean — no emoji library. Tapping an emoji calls
 * `onSelect`; the composer appends it to the draft. Rendering is native: emojis
 * are just UTF-8 text, so they display and store without any special handling.
 */
const EMOJI_GROUPS: { label: string; emojis: string[] }[] = [
  {
    label: "Smileys",
    emojis: [
      "😀", "😁", "😂", "🤣", "😊", "😉", "😍", "😎", "🙂", "😅",
      "🤔", "😴", "😬", "😅", "😢", "😮", "😡", "🥳", "🤝", "🙌",
    ],
  },
  {
    label: "Gestures",
    emojis: ["👍", "👎", "👌", "🙏", "👋", "✋", "💪", "🫡", "👏", "🤙"],
  },
  {
    label: "Trucking",
    emojis: [
      "🚚", "🚛", "🚐", "🛻", "📍", "🗺️", "⛽", "🛣️", "🚧", "📦",
      "🧊", "🌡️", "⏰", "🛞", "🔧",
    ],
  },
  {
    label: "Status",
    emojis: [
      "✅", "❌", "⚠️", "❗", "❓", "🆗", "🔴", "🟢", "🟡", "📞",
      "💬", "👀", "🔥", "💯",
    ],
  },
  {
    label: "Weather",
    emojis: ["☀️", "🌧️", "⛈️", "❄️", "🌫️", "💨", "🧊", "🌬️"],
  },
];

export function EmojiPicker({ onSelect }: { onSelect: (emoji: string) => void }) {
  return (
    <IonContent className="ion-padding">
      {EMOJI_GROUPS.map((group) => (
        <div key={group.label} style={{ marginBottom: 8 }}>
          <div style={{ fontSize: 12, color: "var(--ion-color-medium)", margin: "4px 0" }}>
            {group.label}
          </div>
          <div style={{ display: "flex", flexWrap: "wrap", gap: 4 }}>
            {group.emojis.map((e, i) => (
              <button
                key={`${e}-${i}`}
                type="button"
                aria-label={e}
                onClick={() => onSelect(e)}
                style={{
                  fontSize: 24,
                  lineHeight: "1.4",
                  width: 40,
                  height: 40,
                  background: "transparent",
                  border: "none",
                  cursor: "pointer",
                  padding: 0,
                }}
              >
                {e}
              </button>
            ))}
          </div>
        </div>
      ))}
    </IonContent>
  );
}
