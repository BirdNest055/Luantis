#!/bin/bash
# Start the Luanti client
# Usage: ./start_client.sh [server_address] [port]
# Example: ./start_client.sh localhost 30000
# Or just: ./start_client.sh  (for local/singleplayer)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADDRESS="${1:-}"
PORT="${2:-30000}"

if [ -n "$ADDRESS" ]; then
    echo "Starting Luanti client..."
    echo "  Connecting to: $ADDRESS:$PORT"
    echo ""
    exec "$SCRIPT_DIR/bin/luanti" \
        --address "$ADDRESS" \
        --port "$PORT" \
        --go
else
    echo "Starting Luanti client (main menu)..."
    echo ""
    exec "$SCRIPT_DIR/bin/luanti"
fi
