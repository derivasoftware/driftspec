#!/usr/bin/env bash
# Auto-launch the argos daemon for the main worktree (REQ-00751-1).
#
# REQ-00408-1 motivates why the daemon must be warm on session open
# (precognition fires remote-load in cold start), but does not specify
# the hook itself — that is REQ-00751-1.
#
# Fired by Claude Code's SessionStart hook (project-level .claude/settings.json)
# so precognition fires on every Claude Code session opened in any argos
# worktree. Idempotent: if the daemon is already running for the main
# workspace, this exits silently.
#
# Exits 0 quickly under all paths — never blocks session startup.

set -u

# Resolve the main worktree from $CLAUDE_PROJECT_DIR. When the hook fires
# inside a session/perspective worktree, ``git rev-parse --git-common-dir``
# still points at the main clone's ``.git`` so the daemon is always
# attached to the long-lived workspace, not transient worktrees.
PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$PWD}"
COMMON_GIT_DIR="$(git -C "$PROJECT_DIR" rev-parse --git-common-dir 2>/dev/null)"
if [[ -z "$COMMON_GIT_DIR" ]]; then
  exit 0
fi
case "$COMMON_GIT_DIR" in
  /*) ;;
  *) COMMON_GIT_DIR="$PROJECT_DIR/$COMMON_GIT_DIR" ;;
esac
MAIN_WORKSPACE="$(cd "$COMMON_GIT_DIR/.." && pwd)"

LOG_DIR="$MAIN_WORKSPACE/.argos"
LOG_FILE="$LOG_DIR/daemon.log"
if ! mkdir -p "$LOG_DIR" 2>/dev/null || ! : >>"$LOG_FILE" 2>/dev/null; then
  LOG_FILE=/dev/null
fi

note() { printf '%s start-argos-daemon: %s\n' "$(date -Is)" "$1" >>"$LOG_FILE" 2>/dev/null || :; }

# Where to find argos. PATH first, for an operator who exposed it
# system-wide; the main worktree's own venv second, which is where every
# argos checkout keeps it. Requiring PATH was the whole reason this hook
# did nothing: a session opened without the venv activated exited here,
# silently, and every command in it paid the cold load (DDEV-413).
ARGOS=""
if command -v argos >/dev/null 2>&1; then
  ARGOS="$(command -v argos)"
elif [[ -x "$MAIN_WORKSPACE/.venv/bin/argos" ]]; then
  ARGOS="$MAIN_WORKSPACE/.venv/bin/argos"
fi
if [[ -z "$ARGOS" ]]; then
  note "no argos on PATH and none at $MAIN_WORKSPACE/.venv/bin/argos; daemon not started"
  exit 0
fi

# Idempotency check. ``argos daemon status`` exits 0 even when the
# daemon is dead (it only checks the PID file's existence), so the
# exit code alone is not a reliable liveness signal. Grep the
# rendered output for ``alive:  True`` — that line is only printed
# when the recorded PID is currently running.
if "$ARGOS" --workspace "$MAIN_WORKSPACE" daemon status 2>/dev/null \
   | grep -qE '^alive:[[:space:]]+True'; then
  exit 0
fi

# A stale PID file from a previous crash would otherwise make
# ``daemon start`` refuse to bind. Remove the daemon paths before
# the launch so this restart-on-stale path actually relaunches.
if "$ARGOS" --workspace "$MAIN_WORKSPACE" daemon stop >/dev/null 2>&1; then
  : # daemon was actually alive after all — pkill+restart anyway is fine
fi

# The log lives beside the daemon's own state, under the workspace's
# ``.argos/`` (already gitignored), and not at a fixed path in /tmp. A
# shared /tmp file belongs to whoever created it first, so on a machine
# with more than one user the redirection fails with EACCES for everyone
# else — and because the shell opens the target before running the
# command, the daemon never starts at all. That failure is silent: the
# hook still exits 0, as it must. Per-workspace also keeps the trails of
# separate checkouts from interleaving.

# Detached launch. nohup + & + setsid ensures the daemon outlives the
# hook process and the Claude session itself. Output goes to a log so
# we have a trail if discovery fails.
note "starting the daemon for $MAIN_WORKSPACE with $ARGOS"
setsid nohup "$ARGOS" --workspace "$MAIN_WORKSPACE" daemon start \
  --rest --rest-host 127.0.0.1 --auto-pull \
  >>"$LOG_FILE" 2>&1 </dev/null &

exit 0
