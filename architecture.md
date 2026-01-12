# Architecture

This repo builds a networked 3D client/server game (a BZFlag-inspired prototype).

At runtime there are two programs:

- **Client**: creates the window (GLFW/OpenGL), renders via threepp, runs local input/audio/UI, and syncs gameplay state to/from a server.
- **Server**: authoritative simulation of players/shots/world + chat + plugin callbacks, with LAN discovery support.

The code is organized around a small **engine layer** (systems: render/physics/network/input/audio/gui) and a **gameplay layer** (World/Player/Shot/Game/Chat/etc.) that uses those systems.

## Important directories (highlighted tree)

```
src/
    client/
        main.cpp                       # Client startup + main loop
        game.{hpp,cpp}                 # Orchestrates gameplay objects on the client
        world.{hpp,cpp}                # Receives world from server, merges config/assets, builds render+physics
        player.{hpp,cpp}               # Local player input -> physics + sends network updates
        shot.{hpp,cpp}                 # Local + replicated shots (visual + raycast ricochet)
        console.{hpp,cpp}              # Chat/console glue to GUI
        server/
            server_browser_controller.*  # Orchestrates LAN scan + remote server lists + connect requests
            server_connector.*           # Connects, constructs Game on success
            server_discovery.*           # Client-side LAN discovery scanning (UDP broadcast)
            server_list_fetcher.*        # Remote server list fetch via HTTP (libcurl)

    server/
        main.cpp                       # Server startup + main loop + plugin boot
        game.{hpp,cpp}                 # Authoritative orchestration (clients, shots, chat, world)
        world.{hpp,cpp}                # Loads world config/assets + optionally zips world directory for clients
        client.{hpp,cpp}               # Per-client authoritative state + replication
        shot.{hpp,cpp}                 # Authoritative shot creation/expiry + hit checks
        chat.{hpp,cpp}                 # Chat routing + plugin hook
        plugin.{hpp,cpp}               # Embedded Python (pybind11) plugin API and callback registration
        server_discovery.*             # Server-side LAN discovery responder beacon
        terminal_commands.*            # Server stdin commands
        server_cli_options.*           # Server CLI parsing

    engine/
        client_engine.*                # Owns client systems and update ordering
        server_engine.*                # Owns server systems and update ordering
        types.hpp                      # Shared types + ids + thresholds
        user_pointer.hpp               # GLFW callback indirection

        components/
            client_network.*             # ENet client + protobuf decode + message queue (peek/flush)
            server_network.*             # ENet server + protobuf decode + send helpers
            render.*                     # threepp scene/camera/renderer; loads models via Assimp
            gui.*                        # Dear ImGui + HUD/console + server browser view integration
            gui/server_browser.*         # Server browser UI
            input.*                      # GLFW key handling + per-frame InputState
            audio.*                      # miniaudio engine + pooled clip instances

        physics/
            physics_world.*              # Bullet world + body creation + raycasts
            rigid_body.*                 # Rigid body wrapper
            compound_body.*              # Compound wrapper for multi-mesh static collisions

        mesh_loader.*                  # Loads GLB meshes for physics/render helpers

    common/
        data_path_resolver.*           # BZ3_DATA_DIR resolution + config layering + asset lookup

    protos/
        messages.proto                 # Protobuf wire schema (ClientMsg/ServerMsg)

    network/
        discovery_protocol.hpp         # UDP LAN discovery packet format

data/
    common/config.json               # Shared config layer (assets, network defaults, fonts)
    client/config.json               # Client config layer (graphics/audio/gui/server lists)
    server/config.json               # Server config layer (plugins, server defaults)
    common/{models,shaders,fonts}/   # Shared assets
    server/worlds/                   # Bundled worlds
    plugins/                         # Python plugins and bzapi helpers
```

## Core concepts

### Data root and configuration layering

The runtime **data root** is discovered via the `BZ3_DATA_DIR` environment variable (required). All config and assets are loaded relative to this directory.

Configuration is **layered JSON** merged into a single “config cache”:

- Client initializes layers: `data/common/config.json` → `data/client/config.json` → user config (created if missing).
- Server initializes layers: `data/common/config.json` → `data/server/config.json` → selected world `config.json`.

This is implemented in `src/common/data_path_resolver.*`. It also builds an **asset key lookup** by flattening `assets.*` and `fonts.*` entries across layers.

Practical implication:

- Prefer using asset keys (`ResolveConfiguredAsset(...)` or the World’s asset map) rather than hardcoding file paths.

#### User config + per-server downloads

The code keeps a per-user config directory (platform-specific) and uses it for:

- `config.json` (client-side user overrides)
- downloaded worlds received from servers

On Linux this typically ends up under `$XDG_CONFIG_HOME/bz3` or `~/.config/bz3`.

Relevant helpers live in `src/common/data_path_resolver.*`:

- `UserConfigDirectory()`
- `EnsureUserConfigFile("config.json")`
- `EnsureUserWorldsDirectory()`
- `EnsureUserWorldDirectoryForServer(host, port)`

### Engine vs gameplay

- **Engine** classes (`src/engine/...`) wrap subsystems: rendering, physics, networking, input, audio, GUI.
- **Gameplay** classes (`src/client/...`, `src/server/...`) define the game rules and state: players, shots, world loading, chat.

The engine exposes direct pointers (e.g. `engine.render`, `engine.physics`, `engine.network`) and the gameplay layer calls into these systems.

## Runtime flow

### Client main loop

The client loop lives in `src/client/main.cpp`.

High-level per-frame ordering:

1. `engine.earlyUpdate(dt)`
     - `Input::update()` reads keys and polls GLFW events.
     - `ClientNetwork::update()` pumps ENet and queues decoded protobuf messages.
2. Handle fullscreen toggle / disconnect events.
3. Either update the server browser (if not connected) or update gameplay.
     - `Game::earlyUpdate()`:
         - `World::update()` waits for `ServerMsg_Init` and initializes render + physics.
         - If world initialized, ensure local `Player` exists.
         - Consume a *subset* of network messages via `peekMessage<T>()` (join/leave/shot create/remove).
4. `engine.step(dt)`
     - `PhysicsWorld::update()` (Bullet stepping).
5. `Game::lateUpdate(dt)`
     - Local player sends `ClientMsg_PlayerLocation` when position/rotation change.
     - Update remote clients and shots.
     - Update scoreboard names.
6. `engine.lateUpdate(dt)`
     - `Render::update()` draws the scene.
     - `GUI::update()` draws ImGui (HUD or server browser).
     - `ClientNetwork::flushPeekedMessages()` frees/destroys any messages that were peeked.

The important pattern is **peek + flush**: gameplay “peeks” messages it wants to handle during the frame, and the network system destroys them during the engine’s late update.

### Server main loop

The server loop lives in `src/server/main.cpp`.

High-level per-frame ordering:

1. Poll stdin (non-blocking) for terminal commands.
2. `engine.earlyUpdate(dt)`
     - `ServerNetwork::update()` pumps ENet, queues decoded client messages.
3. `game.update(dt)`
     - Handle chat.
     - Update each connected client (replication, spawn, state).
     - Handle shot creation and update shots (expiry + hit checks).
     - World update sends init payload to newly-connected clients.
4. `engine.lateUpdate(dt)`
     - `PhysicsWorld::update()` (Bullet stepping).
     - `ServerNetwork::flushPeekedMessages()` frees/destroys peeked messages.

## Networking

### Transport + protocol

- Transport: **ENet** (reliable UDP).
- Schema: **protobuf** in `src/protos/messages.proto`.

The network components decode protobuf messages and translate them into simple C++ structs (`ClientMsg_*`, `ServerMsg_*`) stored in an internal queue.

Key files:

- Client network: `src/engine/components/client_network.*`
- Server network: `src/engine/components/server_network.*`

### Message lifecycle (peek/flush)

Both client and server store received messages as heap-allocated structs and/or ENet packets. The pattern is:

- `peekMessage<T>(optionalPredicate)` returns a pointer to a queued message and marks it “peeked”.
- `flushPeekedMessages()` deletes the heap message or destroys the ENet packet backing it.

If you add a new message type, you must:

1. Update `src/protos/messages.proto`.
2. Update the encode/decode glue in the network components.
3. Add handling in the appropriate gameplay layer.

## World distribution and loading

### Server side

`src/server/world.*` loads config and assets for the selected world.

If the server is launched with a “custom world directory” (CLI), it can zip that directory on startup and later send it to connecting clients via `ServerMsg_Init.worldData`.

On a new connection, the server sends:

- Client id
- Server name
- Default player parameters
- Optional zipped world bytes

### Client side

`src/client/world.*` waits for `ServerMsg_Init`.

If `worldData` is present:

1. Create a per-server world directory under the user config directory (see `EnsureUserWorldDirectoryForServer(...)`).
2. Write `world.zip` and unzip all files.
3. If a `config.json` exists in the extracted world, merge it into the config cache as a new layer labelled “world config”.
4. Optionally read `manifest.json` for additional defaults and assets.

Then world initialization builds:

- Render model: `Render::create(asset("world"))`
- Static collision: `PhysicsWorld::createStaticMesh(asset("world"), 0.0f)`

### Key end-to-end flows

#### Connect → init → world

1. Server browser selects a server and calls `ServerConnector::connect(...)`.
2. `ClientNetwork::connect(...)` establishes ENet connection.
3. Client constructs `Game`, which constructs `World`.
4. Server sends `ServerMsg_Init` (client id + defaults + optional world zip).
5. Client `World::update()` consumes `ServerMsg_Init`, optionally unpacks world zip, merges world config/manifest, then creates render + physics world.
6. Client constructs the local `Player` and sends `ClientMsg_Init` with chosen player name.

If you see “connected but nothing happens”, follow this exact chain and verify where it breaks.

#### Shots

- Client firing creates a local shot immediately and also sends `ClientMsg_CreateShot` with a *local shot id*.
- Server receives it, allocates a *global shot id*, and broadcasts `ServerMsg_CreateShot` to everyone except the owner.
- When a server-side shot expires (or hits a player), the shot destructor broadcasts `ServerMsg_RemoveShot`:
    - to the owner: remove-by-local-id
    - to others: remove-by-global-id

#### Chat

- Client submits chat via the GUI console panel.
- Client sends `ClientMsg_Chat`.
- Server `Chat::update()` receives it, offers plugins a chance to handle it, then forwards `ServerMsg_Chat` to the target (or broadcasts).

## Engine systems

### Render (client)

`src/engine/components/render.*` owns:

- a threepp `Scene`
- a `PerspectiveCamera`
- a renderer bound to the GLFW/OpenGL context

Models are loaded via `threepp::AssimpLoader` (GLB/Assimp formats). Gameplay stores a `render_id` per object and updates transforms each frame.

### GUI (client)

`src/engine/components/gui.*` owns Dear ImGui setup and two major views:

- **Server browser** view (when not connected)
- **HUD/console** view (when in game)

Fonts are loaded via `ResolveConfiguredAsset(...)` using configured font keys.

### Input (client)

`src/engine/components/input.*` translates GLFW key state into an `InputState` struct. Gameplay reads this from `engine.input->getInputState()`.

### Audio (client)

`src/engine/components/audio.*` wraps miniaudio:

- `Audio::loadClip(path, maxInstances)` pools multiple instances to avoid “dropouts” when many events occur (e.g. rapid firing).
- Listener position and direction are updated from the local player each frame.

### Physics (client + server)

`src/engine/physics/physics_world.*` wraps Bullet.

- The world steps at fixed timestep substeps.
- `createStaticMesh()` loads a GLB and builds convex hull shapes per mesh.
- `raycast()` is used for shot ricochet.

## Gameplay modules

### Client gameplay

- `Game` (`src/client/game.*`): owns `World`, `Player`, remote `Client` objects, and `Shot` list; handles focus switching between HUD and chat.
- `World` (`src/client/world.*`): merges assets + defaults, receives server init, unzips/merges world layer, creates render+physics world.
- `Player` (`src/client/player.*`):
    - Sends `ClientMsg_Init` once on construction.
    - Handles spawn request (`ClientMsg_RequestPlayerSpawn`).
    - Applies movement/jump/fire based on `InputState`.
    - Sends location updates when position/rotation exceed thresholds.
- `Shot` (`src/client/shot.*`):
    - Local shots send `ClientMsg_CreateShot` with a local shot id.
    - Replicated shots use a global id from the server.
    - Client simulates shot motion and does a physics raycast each tick for ricochet.

### Server gameplay

- `Game` (`src/server/game.*`): authoritative hub; creates `Client` objects on join and updates shots/clients/chat/world.
- `Client` (`src/server/client.*`): authoritative per-player state; handles initialization, spawn, location forwarding, parameter updates, and death.
- `Shot` (`src/server/shot.*`): authoritative creation/expiry + hit checks; sends create/remove messages.
- `Chat` (`src/server/chat.*`): routes chat; offers plugin interception.
- `World` (`src/server/world.*`): world config/assets + optional world zip distribution.

## Server browser and discovery

There are two server discovery sources:

1. **LAN discovery**
     - Protocol: `src/network/discovery_protocol.hpp`
     - Server responder: `src/server/server_discovery.*` (UDP port 47800)
     - Client scanner: `src/client/server/server_discovery.*`

2. **Remote server lists (HTTP JSON)**
     - Fetch: `src/client/server/server_list_fetcher.*` (libcurl)
     - Orchestration: `src/client/server/server_browser_controller.*`
     - UI: `src/engine/components/gui/server_browser.*`

The server browser controller merges results from LAN + remote lists into a single list of UI entries and delegates connection to `ServerConnector`.

## Plugins (server)

The server embeds Python using pybind11 (`src/server/plugin.*`).

At startup the server:

1. Creates a Python interpreter (`py::scoped_interpreter`).
2. Loads configured plugins from `data/plugins/<pluginName>/plugin.py`.
3. Exposes an embedded module named `bzapi` containing:
     - callback registration by event type
     - chat send
     - set player parameter
     - kill/kick/disconnect
     - basic player lookup helpers

Plugin callbacks are keyed by `ClientMsg_Type` and currently invoked from gameplay (notably chat).

## “Where do I implement X?” cheat sheet

- **Main loop timing / ordering**: `src/client/main.cpp`, `src/server/main.cpp`, `src/engine/*_engine.*`
- **New networked feature**: `src/protos/messages.proto` + `src/engine/components/*_network.*` + gameplay handler in `src/client/*` and/or `src/server/*`
- **World loading / packaging**: `src/server/world.*` and `src/client/world.*`
- **Physics issues**: `src/engine/physics/physics_world.*` (and GLB meshes used by world assets)
- **UI/HUD**: `src/engine/components/gui.*`
- **Server browser**: `src/client/server/*` (control) + `src/engine/components/gui/server_browser.*` (view)
- **Plugins / moderation / commands**: `src/server/plugin.*` and `data/plugins/*`

## Common gotchas

- `BZ3_DATA_DIR` must point at the repo’s `data/` directory (or an installed equivalent).
- Config and assets are *layered*; if something “mysteriously” changes between worlds, check the world’s `config.json` and `manifest.json` merging.
- Network messages are freed on `flushPeekedMessages()`; don’t hold pointers returned by `peekMessage<T>()` beyond the current frame.