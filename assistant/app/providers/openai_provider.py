"""OpenAI-compatible provider — serves both hosted OpenAI and local Ollama.

Ollama exposes an OpenAI-compatible Chat Completions API, so the only difference
is the base URL (and that Ollama ignores the API key). Tool schemas are
translated from the Anthropic-native shape into OpenAI function-calling shape.
"""

from __future__ import annotations

import json
from typing import Any

from ..config import Config
from .base import ConverseResult, LLMProvider, RunTool


def _to_openai_tools(tools: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Anthropic tool schema -> OpenAI function-calling schema."""
    return [
        {
            "type": "function",
            "function": {
                "name": t["name"],
                "description": t.get("description", ""),
                "parameters": t.get("input_schema", {"type": "object", "properties": {}}),
            },
        }
        for t in tools
    ]


class OpenAIProvider(LLMProvider):
    name = "openai"

    def __init__(self, cfg: Config) -> None:
        super().__init__(cfg)
        from openai import OpenAI  # lazy: only needed when selected

        # Ollama: base_url=http://localhost:11434/v1 and a placeholder key.
        kwargs: dict[str, Any] = {}
        if cfg.openai_base_url:
            kwargs["base_url"] = cfg.openai_base_url
        kwargs["api_key"] = cfg.openai_api_key or "ollama"
        self._client = OpenAI(**kwargs)

    def converse(
        self,
        *,
        system: str,
        tools: list[dict[str, Any]],
        user_text: str,
        run_tool: RunTool,
    ) -> ConverseResult:
        cfg = self.cfg
        oai_tools = _to_openai_tools(tools)
        messages: list[dict[str, Any]] = [
            {"role": "system", "content": system},
            {"role": "user", "content": user_text},
        ]

        reply_parts: list[str] = []
        stopped: str | None = None

        for _ in range(cfg.max_tool_turns):
            resp = self._client.chat.completions.create(
                model=cfg.model,
                messages=messages,
                tools=oai_tools,
                max_tokens=cfg.max_tokens,
            )
            choice = resp.choices[0]
            msg = choice.message

            if msg.content:
                reply_parts.append(msg.content)

            if not msg.tool_calls:
                stopped = choice.finish_reason
                break

            # Echo the assistant turn (with its tool calls) back into history.
            messages.append(
                {
                    "role": "assistant",
                    "content": msg.content or "",
                    "tool_calls": [
                        {
                            "id": tc.id,
                            "type": "function",
                            "function": {
                                "name": tc.function.name,
                                "arguments": tc.function.arguments,
                            },
                        }
                        for tc in msg.tool_calls
                    ],
                }
            )

            for tc in msg.tool_calls:
                try:
                    args = json.loads(tc.function.arguments or "{}")
                except json.JSONDecodeError:
                    args = {}
                content, _is_error = run_tool(tc.function.name, args)
                messages.append(
                    {"role": "tool", "tool_call_id": tc.id, "content": content}
                )
        else:
            stopped = "max_tool_turns"

        reply = " ".join(p.strip() for p in reply_parts if p.strip()).strip()
        return ConverseResult(reply=reply, stopped=stopped)
