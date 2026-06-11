# Hey Dispatch — driver voice assistant

A small FastAPI service that gives drivers a **hands-free, "Hey Dispatch"**
assistant. The driver presses to talk; the mobile app transcribes their speech
on-device (Web Speech API), POSTs the transcript here, and reads the reply back
aloud. This service does the **thinking and acting** — it calls the
[Claude API](https://docs.anthropic.com/en/api) with tool use and runs the tools
against the fleet's own services.

```
 Driver speaks ──▶ Mobile (push-to-talk, on-device STT)
                        │  POST /assist  { transcript, driver context }
                        ▼
                 assistant service ──▶ Claude (tool-use loop)
                        │                   │ picks tools
                        │◀──────────────────┘
                        ├─ send_message_to_dispatcher → ALS JSON:API (Message)
                        ├─ get_eta / validate_route / alternate_routes → HERE truck routing
                        └─ tech_help → built-in knowledge base
                        │  { reply, actions }
                        ▼
                 Mobile speaks the reply (on-device TTS)
```

STT and TTS stay **on the device** — this service never sees audio, only text.

## Intents (tools)

| Tool                          | What it does                                              |
| ----------------------------- | -------------------------------------------------------- |
| `send_message_to_dispatcher`  | Posts a message to the driver's dispatcher channel (ALS) |
| `get_eta`                     | ETA + distance to the destination (HERE, or estimate)    |
| `validate_route`              | Flags truck restrictions on the current route (HERE)     |
| `alternate_routes`            | Suggests alternate routes with drive times (HERE)        |
| `tech_help`                   | Answers equipment/app questions (DEF, regen, ELD, …)     |

The model only chooses **which** tool and the spoken arguments (e.g. message
text). All the trusted context — who the driver is, their position, the
destination, the truck profile — comes from the request body, never the model.

## Choosing the AI provider

The AI service is **pluggable** — a backend admin picks one and supplies its
credential via env, no code change:

| `ASSISTANT_PROVIDER` | Service        | Credential / endpoint                | Default model      |
| -------------------- | -------------- | ------------------------------------ | ------------------ |
| `anthropic` (default)| Hosted Claude  | `ANTHROPIC_API_KEY`                  | `claude-opus-4-8`  |
| `openai`             | Hosted OpenAI  | `OPENAI_API_KEY` (+ `OPENAI_BASE_URL`) | `gpt-4o`         |
| `ollama`             | Local models   | `OLLAMA_BASE_URL` (e.g. `…:11434/v1`)| `llama3.1`         |

Override the model with `ASSISTANT_MODEL`. Ollama speaks the OpenAI-compatible
API, so it reuses the OpenAI adapter pointed at a local base URL (no real key).
Provider code lives in [`app/providers/`](app/providers/); SDK imports are lazy,
so you only install the SDK for the provider you run. Note: prompt caching,
adaptive thinking, and `ASSISTANT_EFFORT` apply to the **Anthropic** provider;
the OpenAI/Ollama adapter uses standard chat-completions function calling.

## Run it

```bash
cd assistant
cp .env.example .env          # pick ASSISTANT_PROVIDER + its key (HERE_API_KEY optional)
pip install -r requirements.txt
set -a; . ./.env; set +a       # export the vars
uvicorn app.main:app --host 0.0.0.0 --port "${ASSISTANT_PORT:-5710}"
```

Health check:

```bash
curl -s localhost:5710/health
# {"status":"ok","provider":"anthropic","model":"claude-opus-4-8","ai_configured":true,"here_configured":false}
```

Try a turn:

```bash
curl -s localhost:5710/assist -H 'Content-Type: application/json' -d '{
  "transcript": "tell my dispatcher I am running about thirty minutes late",
  "driver_name": "Jesse",
  "author_user_id": "22222222-2222-2222-2222-222222222222",
  "dispatcher_channel_id": "..."
}' | jq
```

## Production (systemd)

A unit + env template live in [`deploy/`](deploy/). It runs uvicorn from a venv
under `/opt/fleet-dispatcher/assistant`, as the shared `fleet` system user, with
the provider key in a `chmod 600` env file:

```bash
# install code + venv
sudo mkdir -p /opt/fleet-dispatcher/assistant
sudo cp -r app requirements.txt /opt/fleet-dispatcher/assistant/
sudo python3 -m venv /opt/fleet-dispatcher/assistant/venv
sudo /opt/fleet-dispatcher/assistant/venv/bin/pip install \
     -r /opt/fleet-dispatcher/assistant/requirements.txt

# config (holds the AI provider key — keep it 0600)
sudo mkdir -p /etc/fleet-dispatcher
sudo cp deploy/assistant.env.example /etc/fleet-dispatcher/assistant.env
sudo chmod 600 /etc/fleet-dispatcher/assistant.env
sudo $EDITOR /etc/fleet-dispatcher/assistant.env      # ASSISTANT_PROVIDER + its key

# service
sudo cp deploy/fleet-dispatcher-assistant.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now fleet-dispatcher-assistant
journalctl -u fleet-dispatcher-assistant -f
```

Port comes from `ASSISTANT_PORT` in the env file (default `5710`); the unit ships
a `5710` default so it starts even without the env file.

## Design notes

- **Provider-agnostic loop:** the brain (`app/assistant.py`) builds the prompt,
  owns tool execution + action recording, and delegates the model call/tool loop
  to the selected provider (`app/providers/`). Swap provider via env — the rest
  of the app is unchanged.
- **Model:** Anthropic default `claude-opus-4-8` with adaptive thinking and
  `effort=low` — low effort keeps the voice round-trip snappy while still letting
  the model reason about which tool to call. Override via `ASSISTANT_*` env vars.
- **Prompt caching (Anthropic):** the system prompt + tool schemas form a frozen,
  cacheable prefix (`cache_control: ephemeral` on the system block). Per-call
  dynamic context (driver, position, destination) goes in the **user** turn,
  after the cache boundary, so the prefix keeps hitting the cache across drivers
  and turns. `/assist` returns `cache_read_tokens` so you can confirm it.
- **Tool loop:** a manual loop (up to `ASSISTANT_MAX_TOOL_TURNS`) so we keep full
  control — the Anthropic adapter appends Claude's turns verbatim (including
  thinking blocks) and handles `stop_reason: "refusal"` with a safe spoken
  fallback; the OpenAI/Ollama adapter uses chat-completions function calling.
- **HERE optional:** without `HERE_API_KEY`, `get_eta` falls back to a
  straight-line estimate and route tools say routing isn't configured yet.

## Ports

`5710` by default (`ASSISTANT_PORT`) — clear of the desktop (`8089`), ALS
(`5659`), and geospatial (`5701`) listeners. See [`../docs/DEPLOYMENT.md`](../docs/DEPLOYMENT.md).
