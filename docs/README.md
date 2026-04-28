# Luantis Documentation

This directory contains all documentation for the Luantis project, including
reference documentation for the Luanti engine and Luantis-specific guides.

For general Luanti engine documentation, also see: https://docs.luanti.org

Note that the inner workings of the engine are not well documented. It's most
often better to read the code.

Markdown files are written in a way that they can also be read in plain text.
When modifying, please keep it that way!

Here is a list with descriptions of relevant files:

## Luantis-Specific Documentation

- [V9_PLAN.md](V9_PLAN.md): Luantis v9 development roadmap and feature plan.
- [ENCRYPTION_DATA_FLOW.md](ENCRYPTION_DATA_FLOW.md): E2EE encryption architecture
    and data flow for voice chat and keypair authentication.
- [OPENSECURE_GUIDE.md](OPENSECURE_GUIDE.md): Guide for the OpenSecure encrypted
    connection system.
- [GUI_EDITING_GUIDE.md](GUI_EDITING_GUIDE.md): Guide for editing Luantis GUI
    elements and formspecs.
- [ai-agent-instructions.md](ai-agent-instructions.md): Instructions for AI coding
    agents working on this codebase.
- [ai-codebase-reference.md](ai-codebase-reference.md): Reference map of the
    Luantis codebase for automated tooling.
- [luanti-project-map.md](luanti-project-map.md): Comprehensive project structure
    and file purpose map.
- [TODO_FIXME_LIST.md](TODO_FIXME_LIST.md): Auto-generated list of TODO and FIXME
    items across the codebase.

## Server Modding

- [lua_api.md](lua_api.md): Server Modding API reference. (Not only the Lua part,
    but also file structure and everything else.)
    If you want to make a mod or game, look here!
    A rendered version is also available at <https://api.luanti.org/>.
    [mkdocs/README.md](mkdocs/README.md) for documentation building instructions.
- [builtin_entities.md](builtin_entities.md): Doc for entities predefined by the
    engine (in builtin), i.e. dropped items and falling nodes.

## Client-Side Content

- [texture_packs.md](texture_packs.md): Layout and description of Luanti's
    texture packs structure and configuration.
- [client_lua_api.md](client_lua_api.md): Client-Provided Client-Side Modding
    (CPCSM) API reference.

## Mainmenu Scripting

- [menu_lua_api.md](menu_lua_api.md): API reference for the mainmenu scripting
    environment.
- [fst_api.txt](fst_api.txt): Formspec Toolkit API, included in builtin for the
    main menu.

## Security

- [sscsm_api.md](sscsm_api.md): Server-Sent Client-Side Modding API reference.
- [sscsm_security.md](sscsm_security.md): Security model for SSCSM.

## Formats and Protocols

- [world_format.md](world_format.md): Structure of Luanti world directories and
    format of the files therein.
    Note: If you want to write your own deserializer, it will be easier to read
    the `serialize()` and `deSerialize()` functions of the various structures in
    C++, e.g. `MapBlock::deSerialize()`.
- [protocol.txt](protocol.txt): *Rough* outline of Luanti's network protocol.

## Building and Development

- [compiling/](compiling/): Compilation instructions, and options.
- [ides/](ides/): Instructions for configuring certain IDEs for engine development.
- [developing/](developing/): Information about Luanti development.
    Note: [developing/profiling.md](developing/profiling.md) can be useful for
    modders and server operators!
- [android.md](android.md): Android quirks.
- [direction.md](direction.md): Information related to the future direction of
    Luanti. Commonly referred to as the roadmap document.
- [breakages.md](breakages.md): List of planned breakages for the next major
    release.
- [docker_server.md](docker_server.md): Information about our Docker server
    images in the ghcr.

## Licensing

- [lgpl-2.1.txt](lgpl-2.1.txt): GNU Lesser General Public License v2.1 text.
