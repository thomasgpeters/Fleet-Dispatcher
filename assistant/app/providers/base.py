"""Provider interface + shared types for the assistant brain.

A provider owns the model's tool-use loop because the message/tool wire formats
differ per vendor. It shares two things with the rest of the app:

  * the tool *schemas* (`TOOLS`, Anthropic-native shape) — providers translate as
    needed (e.g. the OpenAI adapter maps them to function-calling shape);
  * a `run_tool(name, args) -> (content, is_error)` callback that actually
    executes a tool and records the action.

This keeps tool execution and action-recording in one place while letting each
provider speak its own protocol.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Any, Callable

from ..config import Config

# Executes one tool call, returns (content_for_model, is_error).
RunTool = Callable[[str, dict[str, Any]], "tuple[str, bool]"]


@dataclass
class ConverseResult:
    """Outcome of one assist turn (actions are recorded via the run_tool side)."""

    reply: str
    cache_read_tokens: int = 0  # prompt-cache hits (Anthropic); 0 elsewhere
    stopped: str | None = None


class LLMProvider(ABC):
    name: str = "base"

    def __init__(self, cfg: Config) -> None:
        self.cfg = cfg

    @abstractmethod
    def converse(
        self,
        *,
        system: str,
        tools: list[dict[str, Any]],
        user_text: str,
        run_tool: RunTool,
    ) -> ConverseResult:
        """Run the full tool-use loop for one driver utterance and return the
        spoken reply. `system` is the frozen prompt; `tools` is the Anthropic-
        native schema list; `user_text` is the per-call dynamic context + the
        transcript."""
        raise NotImplementedError
