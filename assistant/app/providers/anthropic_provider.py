"""Anthropic provider — the project default. Adaptive thinking, effort control,
and prompt caching on the frozen system+tools prefix."""

from __future__ import annotations

from typing import Any

from ..config import Config
from .base import ConverseResult, LLMProvider, RunTool


class AnthropicProvider(LLMProvider):
    name = "anthropic"

    def __init__(self, cfg: Config) -> None:
        super().__init__(cfg)
        import anthropic  # lazy: only needed when this provider is selected

        self._client = anthropic.Anthropic()  # reads ANTHROPIC_API_KEY from env

    def converse(
        self,
        *,
        system: str,
        tools: list[dict[str, Any]],
        user_text: str,
        run_tool: RunTool,
    ) -> ConverseResult:
        cfg = self.cfg

        # Cacheable prefix: cache_control on the (single) system block marks the
        # boundary; the tool schemas sit in the prefix too, so they cache. The
        # dynamic context is in the user turn, after the boundary.
        system_blocks = [
            {"type": "text", "text": system, "cache_control": {"type": "ephemeral"}}
        ]
        messages: list[dict[str, Any]] = [{"role": "user", "content": user_text}]

        cache_read = 0
        reply_parts: list[str] = []
        stopped: str | None = None

        for _ in range(cfg.max_tool_turns):
            resp = self._client.messages.create(
                model=cfg.model,
                max_tokens=cfg.max_tokens,
                system=system_blocks,
                tools=tools,
                thinking={"type": "adaptive"},
                output_config={"effort": cfg.effort},
                messages=messages,
            )

            usage = getattr(resp, "usage", None)
            if usage is not None:
                cache_read += getattr(usage, "cache_read_input_tokens", 0) or 0

            # Safety classifiers can decline (HTTP 200, stop_reason "refusal").
            if resp.stop_reason == "refusal":
                stopped = "refusal"
                reply_parts.append(
                    "Sorry, I can't help with that one. Want me to get your "
                    "dispatcher on the line?"
                )
                break

            for block in resp.content:
                if getattr(block, "type", None) == "text":
                    reply_parts.append(block.text)

            if resp.stop_reason != "tool_use":
                stopped = resp.stop_reason
                break

            # Append the assistant turn verbatim (thinking + tool_use blocks) so
            # the model sees its own reasoning next pass.
            messages.append({"role": "assistant", "content": resp.content})

            tool_results: list[dict[str, Any]] = []
            for block in resp.content:
                if getattr(block, "type", None) != "tool_use":
                    continue
                content, is_error = run_tool(block.name, block.input or {})
                tool_results.append(
                    {
                        "type": "tool_result",
                        "tool_use_id": block.id,
                        "content": content,
                        "is_error": is_error,
                    }
                )
            messages.append({"role": "user", "content": tool_results})
        else:
            stopped = "max_tool_turns"

        reply = " ".join(p.strip() for p in reply_parts if p.strip()).strip()
        return ConverseResult(reply=reply, cache_read_tokens=cache_read, stopped=stopped)
