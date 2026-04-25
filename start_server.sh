#!/bin/bash
# Start the Luanti server
# Usage: ./start_server.sh [world_name] [port]
# Example: ./start_server.sh myworld 30000

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORLD="${1:-world}"
PORT="${2:-30000}"

echo "Starting Luanti server..."
echo "  World: $WORLD"
echo "  Port:  $PORT"
echo ""

exec "$SCRIPT_DIR/bin/luantiserver" \
    --world "$SCRIPT_DIR/worlds/$WORLD" \
    --port "$PORT" \
    --gameid devtest \
    "$@"
