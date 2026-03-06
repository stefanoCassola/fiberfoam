#!/bin/bash
# Entrypoint that runs the app as the host user's UID/GID so files
# written to bind-mounted volumes are owned by the host user.

# Auto-detect UID/GID from the bind-mounted output directory if not
# explicitly provided.  This way `docker compose up` works correctly
# even without launch.sh setting HOST_UID / HOST_GID.
if [ -z "$HOST_UID" ] || [ "$HOST_UID" = "0" ]; then
    if [ -d /data/cases ]; then
        HOST_UID="$(stat -c '%u' /data/cases)"
        HOST_GID="$(stat -c '%g' /data/cases)"
    fi
fi

HOST_UID="${HOST_UID:-0}"
HOST_GID="${HOST_GID:-0}"

# If UID/GID are non-root, create a matching user and switch to it
if [ "$HOST_UID" != "0" ]; then
    groupadd -o -g "$HOST_GID" appgroup 2>/dev/null || true
    useradd -o -u "$HOST_UID" -g "$HOST_GID" -m -d /home/appuser -s /bin/bash appuser 2>/dev/null || true

    # Ensure data directories are writable by the app user
    chown -R "$HOST_UID:$HOST_GID" /data /app/backend 2>/dev/null || true

    exec gosu "$HOST_UID:$HOST_GID" "$@"
else
    exec "$@"
fi
