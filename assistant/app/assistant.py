"""The 'Hey Dispatch' brain: a provider-agnostic tool-use loop.

Voice-first, so latency matters. The cacheable prefix (system prompt + tool
schemas) stays frozen; the per-call dynamic context (driver name, position,
destination) goes in the user message. The actual model call + tool loop lives in
the selected provider (Anthropic, OpenAI, or Ollama — see ./providers). The
provider is created lazily so importing this module needs no API key.
"""

from __future__ import annotations

from typing import Any

from .config import config
from .providers import LLMProvider, get_provider
from .tools import TOOLS, AssistContext, dispatch


# Frozen so the system block stays byte-identical across calls (keeps the
# Anthropic prompt cache hitting). NO dynamic data here — that goes in the user
# turn.
SYSTEM_PROMPT = (
    "You are 'Dispatch', a hands-free voice assistant for a commercial truck "
    "driver who is usually moving. The driver speaks; their words were "
    "transcribed and may be imperfect. Your reply is read aloud by a "
    "text-to-speech engine.\n\n"
    "Style: speak in short, plain sentences. No lists, markdown, emoji, or "
    "spelled-out punctuation — it gets read literally. Lead with the answer. "
    "Confirm actions briefly (\"Told your dispatcher you're running late.\").\n\n"
    "Safety: never ask the driver to read or type while driving. If you are "
    "unsure what they meant, ask one short clarifying question instead of "
    "guessing.\n\n"
    "Use your tools to act for the driver: send messages to their dispatcher, "
    "estimate ETAs, validate the route for their truck, suggest alternates, and "
    "answer equipment or app questions. After a tool runs, relay the result "
    "conversationally — don't just repeat numbers, give the driver the bottom "
    "line."
)


_provider: LLMProvider | None = None


def _get_provider() -> LLMProvider:
    global _provider
    if _provider is None:
        _provider = get_provider(config)
    return _provider


def build_user_text(transcript: str, ctx: AssistContext) -> str:
    """The dynamic, per-call user turn. Kept out of the cached system prefix."""
    lines: list[str] = []
    if ctx.driver_name:
        lines.append(f"Driver: {ctx.driver_name}")
    if ctx.lat is not None and ctx.lng is not None:
        lines.append(f"Current position: {ctx.lat:.5f}, {ctx.lng:.5f}")
    if ctx.dest_label:
        lines.append(f"Destination: {ctx.dest_label}")
    elif ctx.dest_lat is not None and ctx.dest_lng is not None:
        lines.append(f"Destination: {ctx.dest_lat:.5f}, {ctx.dest_lng:.5f}")
    context_block = ("\n".join(lines) + "\n\n") if lines else ""
    return f"{context_block}Driver said: \"{transcript.strip()}\""


def run_assist(transcript: str, ctx: AssistContext) -> dict[str, Any]:
    """Run one assist turn: STT transcript in, spoken reply + actions out.

    Returns {"reply": str, "actions": list[dict], "cache_read_tokens": int,
    "stopped": str|None}.
    """
    provider = _get_provider()
    actions: list[dict[str, Any]] = []

    # Tool executor shared with the provider's loop: runs the tool, records the
    # action, and hands the model the spoken content + error flag.
    def run_tool(name: str, args: dict[str, Any]) -> tuple[str, bool]:
        content, action = dispatch(name, args, ctx)
        actions.append(action)
        return content, not action.get("ok", False)

    result = provider.converse(
        system=SYSTEM_PROMPT,
        tools=TOOLS,
        user_text=build_user_text(transcript, ctx),
        run_tool=run_tool,
    )

    reply = result.reply
    if not reply:
        reply = "Okay." if actions else "Sorry, I didn't catch that. Try again?"

    return {
        "reply": reply,
        "actions": actions,
        "cache_read_tokens": result.cache_read_tokens,
        "stopped": result.stopped,
    }
