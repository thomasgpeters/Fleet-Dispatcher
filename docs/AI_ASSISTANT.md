# Feature 3 — "Hey Dispatch" driver voice assistant

A hands-free assistant for drivers. The driver says **"Hey Dispatch"**, holds the
talk button, and speaks a request — *"send a message to my dispatcher", "what's
my ETA", "validate my route", "I need tech help"*. The app transcribes on-device,
the assistant service decides what to do (calling tools), and the reply is read
back aloud.

Implementation lives in [`../assistant/`](../assistant/) (service) and
`portals/mobile/src/pages/AssistantPage.tsx` (the **Dispatch** tab).

## Decisions (2026-06-11)

| Question            | Decision                                                          |
| ------------------- | ---------------------------------------------------------------- |
| Where does AI run?  | A dedicated **FastAPI service** (`assistant/`), not in ALS or Wt  |
| Activation          | **Push-to-talk** first (wake-word "Hey Dispatch" is a follow-up)  |
| Speech (STT/TTS)    | **On-device** via the browser **Web Speech API** (no audio leaves device) |
| Intents (tools)     | send message · ETA · validate route · alternate routes · tech help |
| AI provider         | **Admin-selectable**: Anthropic (default) / OpenAI / Ollama       |

## Architecture

```
 Driver speaks ─▶ Mobile "Dispatch" tab (push-to-talk)
                    │  on-device STT (Web Speech API) → transcript
                    │  POST /assist { transcript, driver context }
                    ▼
              assistant service (FastAPI, :5710)
                    │  build prompt (frozen system+tools  +  dynamic user turn)
                    ▼
              AI provider (Anthropic | OpenAI | Ollama) ── tool-use loop ──┐
                    │                                                       │
                    │   chooses + calls tools:                             │
                    ├─ send_message_to_dispatcher → ALS JSON:API (Message) │
                    ├─ get_eta / validate_route / alternate_routes → HERE   │
                    └─ tech_help → built-in knowledge base                  │
                    │◀──────────── tool results fed back ───────────────────┘
                    │  { reply, actions }
                    ▼
              Mobile reads the reply aloud (on-device TTS)
```

**Trust boundary:** the model only chooses *which* tool and the spoken arguments
(e.g. the message text). Everything identifying or sensitive — who the driver is,
their GPS position, the destination, the truck profile — is supplied by the
**request** (the mobile app already holds it), never by the model. Handlers
re-read context from the server-trusted `AssistContext`, not from tool input.

## Tools (intents)

| Tool                          | Backed by                         | Fallback when unconfigured            |
| ----------------------------- | --------------------------------- | ------------------------------------- |
| `send_message_to_dispatcher`  | ALS JSON:API `POST /Message`      | error surfaced to the model verbally  |
| `get_eta`                     | HERE truck routing v8             | straight-line haversine estimate      |
| `validate_route`              | HERE notices (height/weight/etc.) | "routing isn't set up yet"            |
| `alternate_routes`            | HERE alternatives                 | "routing isn't set up yet"            |
| `tech_help`                   | built-in keyword KB               | route the question to dispatcher/shop |

HERE is optional (`HERE_API_KEY`); without it the route tools degrade gracefully
so the assistant still works for messaging and tech help on day one.

## Pluggable AI provider

The backend admin chooses the AI service and supplies its credential — no code
change. Selected by `ASSISTANT_PROVIDER`; provider code is in
`assistant/app/providers/` behind a small `LLMProvider.converse(...)` interface.

| `ASSISTANT_PROVIDER` | Service / SDK         | Credential / endpoint                  | Default model     |
| -------------------- | --------------------- | -------------------------------------- | ----------------- |
| `anthropic` (default)| Claude (`anthropic`)  | `ANTHROPIC_API_KEY`                    | `claude-opus-4-8` |
| `openai`             | OpenAI (`openai`)     | `OPENAI_API_KEY` (+ `OPENAI_BASE_URL`) | `gpt-4o`          |
| `ollama`             | Local (OpenAI-compat) | `OLLAMA_BASE_URL` (`…:11434/v1`)       | `llama3.1`        |

- **Lazy SDKs:** the provider's SDK imports only when selected, so a deployment
  installs just the package it uses.
- **Anthropic specifics:** adaptive thinking, `ASSISTANT_EFFORT` (low for voice
  latency), and **prompt caching** — the system prompt + tool schemas are a
  frozen, cacheable prefix; the per-call dynamic context (driver, position,
  destination) goes in the *user* turn after the cache boundary, so the prefix
  keeps hitting the cache across drivers and turns. `/assist` returns
  `cache_read_tokens` to confirm.
- **OpenAI / Ollama:** standard chat-completions function calling. Ollama reuses
  the OpenAI adapter with a local base URL and a placeholder key.

The brain (`app/assistant.py`) is provider-agnostic: it builds the prompt, owns
tool execution + action recording (via a `run_tool` callback), and delegates the
model call/tool loop to the provider.

## Mobile (Dispatch tab)

`AssistantPage.tsx`: hold-to-talk button → `SpeechRecognition` (STT, interim
results shown) → on release, `POST /assist` with the driver's context (dispatcher
channel + active-trip destination, primed on mount; live GPS fix per call) →
`speechSynthesis` reads the reply. Falls back to a clear message where the Web
Speech API isn't available (use Chrome or the installed app). Base URL:
`VITE_ASSISTANT_BASE_URL` (default `http://localhost:5710`).

## Ports & deployment

Runs on `5710` (`ASSISTANT_PORT`) — clear of the desktop (`8089`), ALS (`5659`),
and geospatial (`5701`). Standup is step 8 in [`DEPLOYMENT.md`](DEPLOYMENT.md);
service usage + env reference in [`../assistant/README.md`](../assistant/README.md).

## Open items

- Wake-word ("Hey Dispatch") hands-free activation (push-to-talk shipped first).
- Real tech-help knowledge base / retrieval (current KB is a placeholder).
- Per-driver truck profile (height/weight) feeding the route tools — needs auth.
- Tighten CORS for production (dev allows all origins for the mobile/Capacitor
  schemes).
- Live end-to-end test against a configured provider + HERE key.
