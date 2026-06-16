"""Pluggable AI providers for the assistant brain.

The provider runs the model's tool-use loop; the rest of the app (tools, FastAPI
surface) is provider-agnostic. Pick one via `ASSISTANT_PROVIDER` (see config).
"""

from __future__ import annotations

from ..config import Config
from .base import ConverseResult, LLMProvider, RunTool


def get_provider(cfg: Config) -> LLMProvider:
    """Construct the configured provider. Imports the SDK lazily so a deployment
    only needs the package for the provider it actually uses."""
    p = cfg.provider
    if p == "anthropic":
        from .anthropic_provider import AnthropicProvider

        return AnthropicProvider(cfg)
    if p in ("openai", "ollama"):
        # Ollama speaks the OpenAI-compatible API — same adapter, local base URL.
        from .openai_provider import OpenAIProvider

        return OpenAIProvider(cfg)
    raise ValueError(
        f"Unknown ASSISTANT_PROVIDER={p!r}; expected anthropic|openai|ollama."
    )


__all__ = ["get_provider", "LLMProvider", "ConverseResult", "RunTool"]
