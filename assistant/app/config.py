"""Configuration for the Hey Dispatch assistant service (env-driven).

The AI provider is pluggable so a backend admin can pick the service and supply
its key without code changes:

    ASSISTANT_PROVIDER=anthropic   ANTHROPIC_API_KEY=sk-ant-...   (default)
    ASSISTANT_PROVIDER=openai      OPENAI_API_KEY=sk-...
    ASSISTANT_PROVIDER=ollama      OLLAMA_BASE_URL=http://localhost:11434/v1

`anthropic` and `openai` are hosted; `ollama` runs models locally and speaks the
OpenAI-compatible API, so it reuses the OpenAI adapter with a local base URL and
no real key. Each provider has a sensible default model; override with
`ASSISTANT_MODEL`.
"""

from __future__ import annotations

import os


# Per-provider default model when ASSISTANT_MODEL isn't set.
_DEFAULT_MODELS = {
    "anthropic": "claude-opus-4-8",  # project standard (see docs/AI_ASSISTANT.md)
    "openai": "gpt-4o",
    "ollama": "llama3.1",            # any tool-capable local model
}


class Config:
    # Which AI service runs the assistant brain.
    provider: str = os.environ.get("ASSISTANT_PROVIDER", "anthropic").lower()

    # Model + reasoning effort. Model defaults per-provider; effort applies to
    # providers that support it (Anthropic). Override via env as needed.
    model: str = os.environ.get("ASSISTANT_MODEL") or _DEFAULT_MODELS.get(
        provider, "claude-opus-4-8"
    )
    effort: str = os.environ.get("ASSISTANT_EFFORT", "low")  # low|medium|high|max
    max_tokens: int = int(os.environ.get("ASSISTANT_MAX_TOKENS", "1024"))
    max_tool_turns: int = int(os.environ.get("ASSISTANT_MAX_TOOL_TURNS", "5"))

    # Provider credentials / endpoints (admin supplies the one they use).
    anthropic_api_key: str | None = os.environ.get("ANTHROPIC_API_KEY")
    openai_api_key: str | None = os.environ.get("OPENAI_API_KEY")
    # OpenAI-compatible base URL: set for Ollama (or an OpenAI-compatible gateway).
    openai_base_url: str | None = os.environ.get("OPENAI_BASE_URL") or os.environ.get(
        "OLLAMA_BASE_URL"
    )

    # Downstream services.
    fleet_api_base_url: str = os.environ.get(
        "FLEET_API_BASE_URL", "http://localhost:5659/api"
    )  # ApiLogicServer JSON:API
    gis_base_url: str = os.environ.get("GIS_BASE_URL", "http://localhost:5701")  # geospatial endpoint
    here_api_key: str = os.environ.get("HERE_API_KEY", "")  # truck routing (optional)

    port: int = int(os.environ.get("ASSISTANT_PORT", "5710"))

    @property
    def ai_configured(self) -> bool:
        """Is the selected provider usable (key/endpoint present)?"""
        if self.provider == "anthropic":
            return bool(self.anthropic_api_key)
        if self.provider == "openai":
            return bool(self.openai_api_key)
        if self.provider == "ollama":
            return bool(self.openai_base_url)
        return False


config = Config()
