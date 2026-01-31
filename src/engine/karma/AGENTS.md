# src/engine/karma/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory is a **forwarder header tree** that mirrors Karma’s intended
public include layout. It exists only while Karma is in-tree.

## Purpose
- Allows game code to include `#include "karma/..."` today.
- When Karma becomes its own repo, these forwarders disappear and the real
  engine headers will be installed under `karma/`.

## What’s inside
Each subdirectory mirrors the engine subsystem and typically contains only
headers that include the real implementation from `src/engine/...`.

## Rules
- Do not place implementation here.
- Keep it strictly as forwarding headers and API surface definitions.
