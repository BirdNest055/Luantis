# OpenSecure CLI Guide for Luanti-Secure Project

## Overview

OpenSecure is installed globally and ready to use. This guide teaches you how to use it effectively in the Luanti-Secure project.

**Note:** OpenSecure is a separate tool from the Luanti-Secure game engine itself. It provides messaging and agent orchestration capabilities. The Luanti-Secure game engine is the core product — a fork of Luanti with real encrypted communications.

## Project Context

- **Project:** Luanti-Secure v9.24
- **Repository:** https://github.com/BirdNest055/Luantis
- **Current branch:** `luantis-v9.24-fix-settingtypes-context`
- **Key feature:** Real AES-256-GCM encryption for game traffic, ECDH X25519 forward secrecy, modular encryption toggle, bonus scoring, portable build system, log toggle with encryption_log_level control

## Quick Start

### 1. Check OpenSecure Status
```bash
opensecure status
```
Shows channel health and recent session recipients.

### 2. Start the Gateway (if not running)
```bash
opensecure gateway
```
Runs the WebSocket Gateway locally. Use `--force` to kill existing processes on the default port.

### 3. Initialize Project Workspace
```bash
opensecure setup --workspace /workspaces/Luanti-Secure
```
Sets up the agent workspace for this project.

## Essential Commands

### Agent Interaction
```bash
# Run a single agent turn
opensecure agent --message "Your message here"

# Send message and deliver via channel
opensecure agent --to +1234567890 --message "Hello" --deliver
```

### Model Management
```bash
# List available models
opensecure models list

# Scan for new models
opensecure models scan
```

### Channel Management
```bash
# List connected channels
opensecure channels status

# Login to a channel (e.g., WhatsApp, Telegram)
opensecure channels login
```

### Message Operations
```bash
# Send a message
opensecure message send --target +1234567890 --message "Hello"

# Send via Telegram
opensecure message send --channel telegram --target @username --message "Hello"
```

### Configuration
```bash
# Interactive configuration
opensecure configure

# Get config value
opensecure config get --path agents.defaults.workspace

# Set config value
opensecure config set --path agents.defaults.workspace --value /workspaces/Luanti-Secure
```

## Development Commands

### Run in Dev Mode
```bash
opensecure --dev gateway
```
Runs isolated gateway on port 19001 with separate state directory (~/.opensecure-dev).

### View Logs
```bash
opensecure logs
```
Tail gateway file logs via RPC.

### Health Check
```bash
opensecure health
```
Fetch health status from running gateway.

### Doctor (Diagnostics)
```bash
opensecure doctor
```
Health checks and quick fixes for gateway and channels.

## MCP Integration
```bash
# Manage MCP config
opensecure mcp status

# List MCP servers
opensecure mcp list
```

## Skills Management
```bash
# List available skills
opensecure skills list

# Inspect a skill
opensecure skills inspect <skill-name>
```

## Plugins
```bash
# List plugins
opensecure plugins list

# Manage plugins
opensecure plugins <subcommand>
```

## Project-Specific Setup

To configure OpenSecure for this project:

1. **Set workspace**: `opensecure config set --path agents.defaults.workspace --value /workspaces/Luanti-Secure`

2. **Start gateway**: `opensecure gateway`

3. **Verify setup**: `opensecure status`

4. **Test agent**: `opensecure agent --message "Hello from Luanti-Secure project"`

## Tips

- Use `--help` on any command for detailed usage: `opensecure <command> --help`
- Add `--json` flag for JSON output (useful for scripting)
- Use `--verbose` for detailed logging
- Check docs: `opensecure docs`
- Web UI: `opensecure dashboard`

## Documentation
Full documentation: https://docs.opensecure.ai/cli
