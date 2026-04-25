# OpenClaw CLI Guide for Clawtest Project

## Overview

OpenClaw is installed globally and ready to use. This guide teaches you how to use it effectively in the Clawtest project.

**Note:** OpenClaw is a separate tool from the Clawtest game engine itself. It provides messaging and agent orchestration capabilities. The Clawtest game engine is the core product — a fork of Luanti with real encrypted communications.

## Project Context

- **Project:** Clawtest v9.3
- **Repository:** https://github.com/BirdNest055/Clawtest
- **Current branch:** `clawtest-v9.3`
- **Key feature:** Real AES-256-GCM encryption for game traffic, modular encryption toggle

## Quick Start

### 1. Check OpenClaw Status
```bash
openclaw status
```
Shows channel health and recent session recipients.

### 2. Start the Gateway (if not running)
```bash
openclaw gateway
```
Runs the WebSocket Gateway locally. Use `--force` to kill existing processes on the default port.

### 3. Initialize Project Workspace
```bash
openclaw setup --workspace /workspaces/Clawtest
```
Sets up the agent workspace for this project.

## Essential Commands

### Agent Interaction
```bash
# Run a single agent turn
openclaw agent --message "Your message here"

# Send message and deliver via channel
openclaw agent --to +1234567890 --message "Hello" --deliver
```

### Model Management
```bash
# List available models
openclaw models list

# Scan for new models
openclaw models scan
```

### Channel Management
```bash
# List connected channels
openclaw channels status

# Login to a channel (e.g., WhatsApp, Telegram)
openclaw channels login
```

### Message Operations
```bash
# Send a message
openclaw message send --target +1234567890 --message "Hello"

# Send via Telegram
openclaw message send --channel telegram --target @username --message "Hello"
```

### Configuration
```bash
# Interactive configuration
openclaw configure

# Get config value
openclaw config get --path agents.defaults.workspace

# Set config value
openclaw config set --path agents.defaults.workspace --value /workspaces/Clawtest
```

## Development Commands

### Run in Dev Mode
```bash
openclaw --dev gateway
```
Runs isolated gateway on port 19001 with separate state directory (~/.openclaw-dev).

### View Logs
```bash
openclaw logs
```
Tail gateway file logs via RPC.

### Health Check
```bash
openclaw health
```
Fetch health status from running gateway.

### Doctor (Diagnostics)
```bash
openclaw doctor
```
Health checks and quick fixes for gateway and channels.

## MCP Integration
```bash
# Manage MCP config
openclaw mcp status

# List MCP servers
openclaw mcp list
```

## Skills Management
```bash
# List available skills
openclaw skills list

# Inspect a skill
openclaw skills inspect <skill-name>
```

## Plugins
```bash
# List plugins
openclaw plugins list

# Manage plugins
openclaw plugins <subcommand>
```

## Project-Specific Setup

To configure OpenClaw for this project:

1. **Set workspace**: `openclaw config set --path agents.defaults.workspace --value /workspaces/Clawtest`

2. **Start gateway**: `openclaw gateway`

3. **Verify setup**: `openclaw status`

4. **Test agent**: `openclaw agent --message "Hello from Clawtest project"`

## Tips

- Use `--help` on any command for detailed usage: `openclaw <command> --help`
- Add `--json` flag for JSON output (useful for scripting)
- Use `--verbose` for detailed logging
- Check docs: `openclaw docs`
- Web UI: `openclaw dashboard`

## Documentation
Full documentation: https://docs.openclaw.ai/cli
