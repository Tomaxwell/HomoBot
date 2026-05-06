#!/usr/bin/env python3
"""Project guardrails for Codex hooks."""

import argparse
import json
import re
import sys


DENY_PATTERNS = [
    (re.compile(r"\b(systemctl|reboot|shutdown|poweroff|halt)\b"), "Process and power control must go through EM/operator procedures."),
    (re.compile(r"\b(git\s+reset\s+--hard|git\s+checkout\s+--|git\s+clean\s+-[fdx])\b"), "Destructive git cleanup can overwrite user or Claude Code work."),
    (re.compile(r"\bros2\s+topic\s+pub\s+/sm/"), "Do not publish directly into SM state-control topics from Codex."),
    (re.compile(r"\brm\s+.*\bclaude_[^/\s]*\.md\b"), "Do not delete Claude Code design documents from Codex."),
    (re.compile(r"\brm\s+.*\bcodex_[^/\s]*\.md\b"), "Do not delete Codex design documents without an explicit user request."),
]


def read_payload() -> dict:
    try:
        raw = sys.stdin.read()
        return json.loads(raw) if raw.strip() else {}
    except json.JSONDecodeError:
        return {}


def deny(reason: str) -> None:
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason": reason,
        }
    }, ensure_ascii=False))


def session_start() -> None:
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "SessionStart",
            "additionalContext": (
                "ROS2 project guardrails loaded: Codex owns codex_* artifacts; "
                "Claude Code owns claude_* artifacts; EM is the only process-control "
                "authority; Gateway is the only cloud/app exit; SM is the state authority."
            ),
        }
    }, ensure_ascii=False))


def pre_tool_use(payload: dict) -> None:
    command = (
        payload.get("tool_input", {}).get("command")
        or payload.get("tool_input", {}).get("cmd")
        or ""
    )
    for pattern, reason in DENY_PATTERNS:
        if pattern.search(command):
            deny(reason)
            return


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hook", required=True)
    args = parser.parse_args()
    payload = read_payload()

    if args.hook == "SessionStart":
        session_start()
    elif args.hook == "PreToolUse":
        pre_tool_use(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
