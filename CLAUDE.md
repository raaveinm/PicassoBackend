# CLAUDE.md
# PickUsAllBackend (picassobackend)

C++20 server for [PickUsAll (Picasso)](https://github.com/raaveinm/PickUsAll) — a KMP/Compose client for gaming-focused friend groups. This repo is the server side referenced as `picassobackend` throughout the client's docs; it is *not* built from the client repo and doesn't share a build system with it, but its wire protocol has to match what the client's `ChatRepository`/`ChatDao` expect (see "Wire protocol" below).

Product context, terminology (Artist/Palette/Color/...), the multi-server architecture, and the reasoning behind every decision below live in the client repo's `brainstorm/brainstorm.md`, `brainstorm/pipeline.md`, and especially `brainstorm/decisions.md` — this file summarizes what's relevant here, doesn't duplicate the full reasoning.

> **Read "What exists today" before trusting anything else here.** Parts of this file describe an *agreed plan*, not shipped behaviour. Sections are labelled accordingly.

Full reference documentation — module-by-module breakdown, every endpoint, the WS protocol, the error model — lives in [`docs/SERVER.md`](docs/SERVER.md). RTC requirements and the known gaps in the signaling protocol are in [`docs/WEBRTC.md`](docs/WEBRTC.md). This file is the short version plus the build gotchas.

## Status (2026-09-10)

Skeleton in place: every module from the layout below exists, has its own CMake target, and compiles. What actually *works* end to end is small; what is wired but stubbed is large. As of the `feat(rtc)` commit, the RTC signaling path (SDP/ICE/hangup relay) and WS auth both have real — but explicitly temporary and insecure — implementations, added specifically to unblock end-to-end testing ahead of real Steam auth and storage. **Do not deploy this build publicly**: see the callout in "Known WIP / gaps".

Verified running:

- Config from the environment (`PICASSO_PORT=8099` moves the port; defaults bind `0.0.0.0:8000`).
- `GET /ping` -> 200 `pong`, `X-Service-Status: Healthy`. `GET /` -> `static/index.html`.
- A failed bind logs `cannot bind <addr>:<port>` and exits 1 instead of aborting.
- Unimplemented endpoints answer a uniform 501 JSON naming the roadmap step, e.g. `GET /auth/steam/begin` -> `{"status":501,...,"message":"service: AuthService::beginLoginUrl is not implemented yet (roadmap step 3: auth)"}`.
- `GET /ws` -> 401 without a bearer token, and 401 if the token doesn't parse as a plain integer. With a numeric token (`Authorization: Bearer <anything-that-parses-as-uint64>`) it now succeeds — `AuthService::authenticate` treats the token as the literal steamId, no verification at all — and the upgrade completes with `101 Switching Protocols`.
- Once upgraded, `sdp_offer` / `sdp_answer` / `ice_candidate` / `call_hangup` frames are relayed end to end to the target steamId's connection(s), with `fromSteamId` stamped by the server. `chat_message` and `call_invite` frames are parsed and logged only — no service call yet.

Real logic, not stubs: `ConnectionHub` (presence + fan-out), `EnvelopeCodec`, `Config`, `ErrorHandler`, the WS session/socket lifecycle and identity plumbing, `CallSignalService::relayToPeer` (temporary open relay, see below), `AuthService::authenticate` (temporary, insecure).

Still stubbed, throwing `std::logic_error` that surfaces as 501: every repository method, `storage::migrate`, `ChatService::submit`, `CallSignalService::invite`, `AuthService::beginLoginUrl`/`completeLogin`, `OpenIdVerifier`. Nothing touches Postgres - `oatpp-postgresql` is chosen but deliberately not yet in `conanfile.txt`.

## What this server actually does (per the client's multi-server design)

Each Palette/DM conversation lives authoritatively on exactly one server instance (self-hosted or managed) — this server *is* SSOT for whatever conversations get created on it, and clients hold WS connections to every server they've added, not just one. Concretely, this server:

- Is a thin **signaling router for RTC calls** — it never touches call media (audio). Real audio is P2P mesh between call participants; the server only resolves "who's in this conversation and currently online" and forwards opaque SDP/ICE payloads by steamId. See "Wire protocol" below.
- Is the **SSOT for chat messages** — a client's local Room cache is a cache; this server's Postgres (not yet wired up) is the actual source of truth for a conversation's history.
- Does **not** run Steam API calls on the client's behalf — the client talks to Steam's Web API directly. This server's own Steam-facing responsibility (not yet built) is validating Steam OpenID assertions during login, since that's a server-side-only step (the client can't verify Steam's signature itself).

## What exists today

- `main.cpp` — one line: `return picasso::app::run()`. Everything else lives under `src/`.
- `static/index.html` — served by `GET /`. Path baked in at compile time via `PICASSO_STATIC_ROOT`, set on the `picasso_transport_http` target.
- `conanfile.txt` — `boost` plus the oat++ family (`oatpp`, `oatpp-websocket`, `oatpp-postgresql`), all pinned to plain `1.3.0`. All four resolve and link; the postgres one is declared but unused so far, so the linker drops it from the binary.
- `test/` no longer exists — it held `EndpointController`, which was never a test and is now `src/transport/http/HealthController.hpp`. The name is free for real tests.

## Layout

```
main.cpp                      — only calls app::run()
src/
  app/
    Config.hpp/.cpp           — env vars: PICASSO_BIND, PICASSO_PORT,
                                PICASSO_DB_DSN, PICASSO_PUBLIC_URL, PICASSO_LOG_LEVEL
    Components.hpp            — OATPP_CREATE_COMPONENT: ObjectMapper, DbClient, hub, services
    ServerRunner.hpp/.cpp     — bind with a real error instead of an abort, SIGTERM, graceful shutdown
  dto/
    Envelope.hpp              — EnvelopeDto + payload types
    Rest.hpp                  — REST-only DTOs: auth, history page, conversation list, errors
    MessageType.hpp/.cpp      — frame type + parse/toWireString
  domain/                     — plain C++: no oat++, no SQL, no sockets. This is the testable part.
    Ids.hpp                   — strong types SteamId, ConversationId, MessageId
    Message.hpp  Conversation.hpp  Session.hpp
    ports/
      ChatRepository.hpp  ConversationRepository.hpp  SessionRepository.hpp
      PresenceRegistry.hpp    — "who is online on this instance"
      SignalTransport.hpp     — "deliver this frame to that steamId"
  service/
    ChatService.hpp/.cpp        AuthService.hpp/.cpp
    CallSignalService.hpp/.cpp
  storage/                    — the only place SQL lives
    DbClient.hpp              — oatpp-postgresql, QUERY macros
    PgChatRepository.hpp/.cpp   PgConversationRepository.hpp/.cpp
    PgSessionRepository.hpp/.cpp
    migrations/0001_init.sql
  transport/
    http/  HealthController  AuthController  ConversationController  ErrorHandler
    ws/    WsController  ConnectionHub  WsSession  EnvelopeCodec
  steam/
    OpenIdVerifier.hpp/.cpp   — check_authentication call back to steamcommunity
```

Each module directory owns a `CMakeLists.txt` declaring one static library and its dependencies; `src/CMakeLists.txt` aggregates them into the `picasso_core` interface target that `PickUsAllBackend` links. Tests link `picasso_core` and get the whole graph without going through `main`.

**Dependency direction is the load-bearing rule:** `transport → service → domain/ports ← storage`. The per-module `target_link_libraries` calls are what enforce it — a layer reaching sideways fails to link rather than merely looking wrong in review. Concretely: `picasso_domain` links only `picasso_base` (no oat++, no driver, no sockets), and `picasso_service` links neither oat++ nor any `Pg*` class.

Two module entry points are worth knowing about:

- `service::makeServices` takes `PresenceRegistry` and `SignalTransport` as arguments rather than building them, because both are implemented by `ConnectionHub` — which lives a layer *above* in `transport/ws`.
- That forces a two-phase composition root, spelled out in `app::buildComponents`: hub → services → WS endpoint. The order is a consequence of the layering, not a preference.

## Structural invariants

The first is enforced today; the other two are design intent that the skeleton is shaped for but does not yet implement.

These are the reasons the layering exists. If a change makes one of them a matter of developer discipline rather than a matter of what compiles, the change is wrong.

- **`senderSteamId` comes from the authenticated connection, never from the payload.** Enforced by splitting inbound and outbound DTOs: `ChatMessageInDto` has no sender field at all, so there is nothing to trust. `ChatService::submit(SteamId sender, ...)` takes the sender as a parameter, and the only caller that can supply it is `WsSession`, which had the identity baked into its constructor after token validation on upgrade. Omitting the check doesn't compile.
- **SDP/ICE forwarding is membership-checked.** "Forward by `toSteamId` without parsing" taken literally makes the server an open relay: any authenticated user could push arbitrary payloads at any steamId, bypassing conversation membership entirely. `CallSignalService` must verify sender and target are both members of `conversationId`. ICE candidates arrive in bursts, so the user's conversation set is cached on the session at connect time rather than re-queried per candidate. **This is currently violated in code, not just unimplemented.** `CallSignalService::relayToPeer` forwards every `sdp_offer`/`sdp_answer`/`ice_candidate`/`call_hangup` by `toSteamId` with no membership check at all — a `TODO(roadmap step 4/5)` in `CallSignalService.cpp` says so explicitly. It's an open relay today, held together only by `AuthService::authenticate` also being a temporary stand-in (see "Status"). Both close together once storage (step 4) lands.
- **One writer per socket.** Fan-out happens on another connection's thread, and oat++'s blocking `sendOneFrameText` is not safe to call concurrently on one socket. Each session owns an outbound queue with a single writer. Bounding that queue also gives backpressure, but the drop policy has **three** tiers, not two: chat is never dropped (the server is SSOT), call control and SDP are never dropped (losing an `sdp_offer` kills session setup silently and nothing retries it), and only `ice_candidate` may be dropped — candidates are numerous, partly redundant, and more keep arriving. See [`docs/WEBRTC.md`](docs/WEBRTC.md) §3.5.

Presence needs no external store — the multi-server design makes presence inherently instance-local, so `PresenceRegistry` is in-memory. No Redis.

## Architecture decisions

Reasoning for the first and third lives in the client repo's `brainstorm/decisions.md` under "Server stack (picassobackend)" — summarized here:

- **oat++, not Drogon.** Switched for two reasons: near-zero required dependencies beyond OpenSSL (matters because Free Tier deploys via a one-line `install.sh` onto an arbitrary VPS — fewer things to go wrong on someone else's box), and more declarative `ENDPOINT`/DTO macros than Drogon offers for typical JSON REST/WS handling.
- **Blocking oat++ API, not the async one.** `oatpp::network::Server` + `HttpConnectionHandler` is thread-per-connection, which with long-lived WS connections means thread-per-client. Accepted deliberately: a Palette is a friend-group construct, not a public room, and Free Tier targets a small VPS — tens to low hundreds of concurrent connections is the real workload. The async API (`AsyncHttpConnectionHandler` + `AsyncWebSocket`) scales further but poisons the event loop on any blocking call, and `oatpp-postgresql`'s pool *is* blocking, which would force DB work onto a separate executor group. Containment requirement: keep the concurrency model inside `ws/ConnectionHub` and `ws/WsSession` so a future migration doesn't spread. Worth lowering thread stack size — the 8 MB default times a few hundred connections is wasted memory on a small box.
- **RTC is mesh + signaling-only**, not a server-side SFU/media mixer. A real SFU is a substantial media-engine undertaking that oat++ (a plain HTTP/WS/DTO framework, no media engine) doesn't give you for free — projects that need one embed mediasoup/Janus/LiveKit rather than hand-roll it. This server only resolves participants and forwards opaque signaling payloads; WebRTC media is a full P2P mesh between whoever's in the call. Accepted trade-off: mesh is O(n²) connections, fine for a DM or small Palette (a few friends), not for a large room — Palette is a friend-group construct, not a public room, so this fits the actual use case.
- **Postgres via `oatpp-postgresql`** (1.3.0, which is what forced the whole family off `.latest` — see gotchas), not libpqxx. Maps query results onto the same DTOs already used on the wire, ships a connection pool, and has built-in `migrateSchema` — no external migration tool, which matters for the one-line self-host story. Cost: another macro-heavy layer, and coupling to oat++ idioms. Wrap it: `storage/DbClient.hpp` holds the `QUERY` macros, the `Pg*Repository` classes expose a domain-shaped API, and nothing above `storage/` ever sees `DbClient`.
- **Plain headers, not C++20 modules** — see below.

### C++20 modules: tried, dropped

An earlier iteration used real modules (`.cppm`, `FILE_SET CXX_MODULES`). That is gone: `CMakeLists.txt` sets `CMAKE_CXX_SCAN_FOR_MODULES OFF`, `test.cppm` became `test.hpp`. Don't reintroduce them without a reason — the toolchain cost was real (module dependency scanning requires `clang-scan-deps`, which ruled out AppleClang entirely and forced a Homebrew LLVM toolchain on macOS).

One finding worth keeping if anyone reconsiders: **oat++'s `ENUM(...)` macro cannot be exported from a module.** It expands to a file-scope `static` variable as an initializer trick, and C++20 forbids exporting an internal-linkage entity — a hard compiler error, not a style problem. Any other oat++ macro using the same trick will fail identically. With plain headers this restriction does not apply, so `ENUM(...)` is usable at the DTO boundary today. `MessageType` should still be a plain `enum class` in `domain/`, for the unrelated reason that `domain/` must not depend on oat++; convert at the DTO boundary.

## Wire protocol (DTOs exist in `src/dto/`; only the RTC relay frames have a handler so far)

One WS connection per client per server (matching the client's multi-server design), carrying **both** chat and RTC signaling — every frame is an `EnvelopeDto`, `type` says which payload field is populated:

| `type` | payload field | direction | meaning | `WsSession::dispatch` today |
|---|---|---|---|---|
| `chat_message` | `chatMessage` (`ChatMessageInDto`) | client → server | new message; `localMessageId` is the client's outbox row id | logged only, no service call (`ChatService::submit` still throws) |
| `chat_ack` | `chatAck` (`ChatAckDto`) | server → sender | "your message landed" — echoes `localMessageId` so the client can resolve its `PENDING` row, and carries the server-assigned `messageId` | never produced yet |
| `call_invite` | `callInvite` (`CallInviteDto`) | client → server | "start/join a call in this conversation" | logged only (`CallSignalService::invite` still throws) |
| `incoming_call` / `call_accept` / `call_decline` / `peer_joined` / `call_leave` | `callSignal` (`CallSignalDto`) | both directions | just `{conversationId, steamId}` notifications, direction/meaning differs by `type` | falls to the `default:` case, logged as "unhandled frame type" |
| `sdp_offer` / `sdp_answer` | `sdp` (`SdpDto`) | both, routed by `toSteamId` | opaque SDP text — server forwards without parsing; membership is **supposed to be** checked but currently is not (open relay, see "Structural invariants") | relayed via `CallSignalService::relayToPeer`, `fromSteamId` server-stamped |
| `ice_candidate` | `iceCandidate` (`IceCandidateDto`) | both, routed by `toSteamId` | opaque ICE candidate — same: forwarded unparsed, membership check missing today | relayed, `fromSteamId` server-stamped |
| `call_hangup` | `callHangup` (`CallHangupDto`) | both, routed by `toSteamId` | no payload beyond addressing — the frame itself is the hang-up signal | relayed, `fromSteamId` server-stamped |

Inbound and outbound chat DTOs are deliberately different types — see "Structural invariants" above. `EnvelopeDto` also carries an unused `chatMessageOut` field (`ChatMessageOutDto`) with no corresponding wire `type` yet — reserved for when `ChatService` fans a message out to other members, not wired to anything today.

A missed call needs no durability treatment — if the target isn't online, it just doesn't connect. Chat messages do (see the client's outbox pattern in its own `CLAUDE.md`) — but durability there is entirely the client's job (durable local write before send); this server doesn't need special handling for a disconnected recipient beyond "they'll get it next time they sync."

**Open item, needs agreeing with the client:** the protocol above has no mechanism for that sync. The server is declared SSOT and the client's Room is a cache, but nothing tells the cache what it missed. Requires `chat_ack` to carry the server-assigned message id, plus a REST `GET /conversations/{id}/messages?after={id}`. Until that's settled, "they'll get it next time they sync" is unimplementable.

## Database schema

Lives in `src/storage/migrations/0001_init.sql`. Nothing applies it yet.

```
artists              (steam_id PK, display_name, avatar_url, last_seen_at)
conversations        (id identity PK, kind, title, created_at, created_by -> artists)
conversation_members (conversation_id -> conversations, steam_id -> artists, joined_at)
                     PK(conversation_id, steam_id)
messages             (id identity PK, conversation_id -> conversations,
                      sender_steam_id -> artists, text, timestamp)
session              (token_hash PK, steam_id -> artists, created_at, expires_at, revoked_at)
```

All ids and timestamps are `bigint`; timestamps are epoch milliseconds, matching the domain structs (`Message::createdAtEpochMs`) so nothing converts between a database time type and the wire value. `ConversationId` is therefore `int64`, not a string.

Three decisions worth not re-litigating:

- **No `role` on `conversation_members`.** Within a Palette every member is an equal owner — there is nothing to distinguish. Adding one later is a migration; enforcing a permission model that doesn't exist would be dead weight.
- **`messages.id` is the primary key on its own**, global and monotonic, plus an index on `(conversation_id, id)`. A composite PK over `(id, conversation_id)` would not make `id` unique by itself, and the `?after={id}` sync contract depends on exactly that. The separate index exists because the PK index alone can't answer "this conversation, after N".
- **`token_hash`, not `token`.** The plaintext is returned to the client once at login and never stored, so a database dump is not a set of live sessions. `expires_at` is separate from `revoked_at`: expiry is automatic, revocation is an explicit logout.

## Build

Verified working configuration: Linux, `g++-11`, Ninja Multi-Config, Conan from the repo-local `.venv`. `conan_conf.cmake` runs `conan install` during configure and derives the Conan profile from `CMAKE_CXX_COMPILER_VERSION`, so the compiler you pass at bootstrap determines the profile. It also has an `APPLE` branch that profiles Apple toolchains as plain `clang`; the modules-era hard error on AppleClang is gone from `CMakeLists.txt`, but the macOS path is not currently exercised.

**First-time setup cannot use `cmake --preset conan-release`.** It fails with `File not found: build/CMakePresets.json` on a clean checkout — that file is generated by Conan's `CMakeToolchain`, which only runs once CMake has already started configuring, which the preset needs to already exist to do. Bootstrap once with a direct invocation:

```
cmake -S . -B build -G "Ninja Multi-Config" \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-11 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-11 \
  -DCMAKE_BUILD_TYPE=Release
```

After that succeeds once, `build/CMakePresets.json` exists and the presets work normally.

### Gotchas that cost real time

- **`conan_conf.cmake` used to pin `[tool_requires] cmake/3.19.8`** — a workaround from the Drogon/libpqxx era (some vendored CMake buildsystems needed an old CMake). Removed: oat++ itself needs CMake ≥3.20 to build from source, so the old pin actively broke the build instead of fixing anything. If a future dependency needs an old CMake again, pin it then — don't reintroduce this preemptively.
- **Boost 1.91.0's Conan Center recipe fails to package `boost_cobalt_io_ssl`** regardless of settings (`ConanException: These libraries were expected to be built, but were not built`) — an upstream recipe bug, not something in this project. Fixed via `boost/*:without_cobalt=True` in `conanfile.txt` (Boost.Cobalt isn't used by this project anyway).
- **`find_package(Boost)` needs `CONFIG` explicit**, or modern CMake's policy `CMP0167` (FindBoost module removed) makes it fall back to CMake's own bundled legacy `FindBoost.cmake` instead of Conan's generated `BoostConfig.cmake`, which then fails to find anything Conan installed. Use `find_package(Boost REQUIRED CONFIG COMPONENTS ...)`.
- **`Boost::system` is not a linkable target in this Boost version** — Boost.System became header-only a while back. Don't link it; components you do need (e.g. `thread`) still work normally via `find_package(Boost REQUIRED CONFIG COMPONENTS thread)` + `Boost::thread`.
- **The build directory is multi-config.** Conan generates `build-Debug.ninja` / `build-Release.ninja` / `build-RelWithDebInfo.ninja` alongside `build.ninja`, and binaries land in `build/<Config>/`, not `build/`. A bare `ninja` in `build/` does not necessarily build the config you think it does. Same trap as the build preset above.
- **`cmake_policy(SET CMP0111 OLD)` in `CMakeLists.txt` is load-bearing.** Raising `cmake_minimum_required` from 3.6 to 3.20 flips CMP0111 (added in 3.19) to NEW, which turns a missing `IMPORTED_LOCATION` from a warning into a hard configure error. Conan's CMakeDeps files are not clean under it with a multi-config generator: they declare both `CONAN_LIB::*_DEBUG` and `CONAN_LIB::*_RELEASE` targets but only populate the one matching the `build_type` that `conan_conf.cmake` installed, so the other set has no location for any configuration. Nothing links those empty targets, so the OLD behaviour is correct here — don't "fix" this by pinning the project back to an ancient CMake minimum.
- **oat++ 1.3.0's actual header paths differ from what most examples/docs (and memory) suggest** — this version still nests things under `core/` and the old `parser/json/mapping/` path, it did not do the flattening some other version lines did:
  - `oatpp/core/macro/codegen.hpp` (codegen bracketing), not `oatpp/macro/codegen.hpp`
  - `oatpp/parser/json/mapping/ObjectMapper.hpp` + `oatpp::parser::json::mapping::ObjectMapper`, not `oatpp/json/ObjectMapper.hpp`
  - `oatpp/core/base/Environment.hpp` + `oatpp::base::Environment`, not `oatpp::Environment`
  - `oatpp/web/server/api/ApiController.hpp`, `oatpp/network/Server.hpp`, `oatpp/network/tcp/server/ConnectionProvider.hpp`, `oatpp/web/server/HttpRouter.hpp`, `oatpp/web/server/HttpConnectionHandler.hpp` — these matched expectations.
  - If a new header 404s, check the actual installed tree before guessing again: `find <conan package folder>/include -iname '<HeaderName>.hpp'` beats trusting any doc/memory for this specific version.
- **Pin the whole oat++ family to `1.3.0`, never `1.3.0.latest`.** ConanCenter ships both the tagged release and a package built from upstream's `latest` branch, and they are different versions to Conan. `oatpp-postgresql` only exists as `1.3.0` and depends on `oatpp/1.3.0`, so keeping `oatpp/1.3.0.latest` produces a hard `Version conflict: Conflict between oatpp/1.3.0 and oatpp/1.3.0.latest`, not a warning.
- **The two lines are not API-compatible.** `1.3.0.latest` has `ErrorHandler::handleError(const std::exception_ptr&)`; tagged `1.3.0` does not — it only has the pure virtual `handleError(status, message, headers)`, and `HttpProcessor` catches `std::exception` itself and hands the handler a ready-made 500. Anything overriding the exception_ptr form fails with "marked 'override', but does not override" after the switch. That is why `picasso::kNotImplementedPrefix` exists: with the exception type unavailable, a message prefix is the only way to tell "planned, not built" from "broke".
- **`oatpp::String` has no `std_str()` in 1.3.0.** It is an `ObjectWrapper` around `std::string`, so `operator->` already yields `std::string` — write `*value` to get the `std::string`, not `value->std_str()`. The latter is all over older oat++ examples and fails with `'std::basic_string<char>' has no member named 'std_str'`.
- **`oatpp::web::protocol::http::HttpError::getInfo()` and `getMessage()` are not const.** An error handler must catch it by non-const reference or the call fails with "discards qualifiers".
- **`Header` has no `LOCATION` constant** — only `CONTENT_TYPE`, `AUTHORIZATION` and a few others. Use the `"Location"` literal for redirects.

## Useful build/verify commands

```
cmake --build build --config Release      # after first-time bootstrap, see above
./build/Release/PickUsAllBackend          # binds 0.0.0.0:8000 unless PICASSO_* says otherwise
curl -i http://127.0.0.1:8000/ping        # -> 200, "pong", X-Service-Status: Healthy
curl -i http://127.0.0.1:8000/            # -> 200, static/index.html

PICASSO_PORT=8099 ./build/Release/PickUsAllBackend
```

**Use `--config`, not the build preset.** Conan's generated `conan-release` build preset sets no configuration, so under Ninja Multi-Config `cmake --build --preset conan-release` builds **Debug** and leaves `build/Release/PickUsAllBackend` stale. That combination silently tests an old binary — the preset name says release, the output does not.

## Roadmap

Order is constrained, not arbitrary: step 3 must precede step 4, or the first rows written to `messages` carry an unverified sender. Step 5 follows 4 because membership comes from the database.

1. ~~**Skeleton** — `app/`, config from env, a real error on a failed bind, one JSON error handler.~~ Done.
2. **WS transport** — hub, session and codec exist; `WsSession::dispatch` now calls into `CallSignalService` for the four RTC relay frame types, but `chat_message`/`call_invite` still just log, and the bounded outbound queue is still missing. Unblocks the client: `ChatRepository.sendToServer()` finally has something to connect to, once chat is wired the same way.
3. **Auth** — Steam OpenID, session tokens, validation on WS upgrade. A temporary stand-in (`AuthService::authenticate` trusting the bearer token as a literal steamId) exists only to unblock testing step 5 early; it must be replaced before `senderSteamId` is a real, verified identity. `beginLoginUrl`/`completeLogin` are still unimplemented.
4. **Storage** — schema, migrations, repositories, `ChatService` persists; REST history endpoint.
5. **RTC signaling** — membership check plus forwarding. Forwarding is now implemented (`CallSignalService::relayToPeer`); the membership check is not, so today's relay is open by steamId. `invite()` is still unimplemented. Closing the membership check depends on step 4 (repositories need to be real, not stubs).

## Known WIP / gaps

> **Security note:** the current build is not safe to expose on a public network. `AuthService::authenticate` accepts any bearer token that parses as an integer as proof of that steamId, and `CallSignalService::relayToPeer` forwards signaling frames to any steamId with no conversation-membership check. Both are explicit, commented temporary stand-ins added to unblock end-to-end RTC testing before steps 3 and 4 land — not partial implementations of the real thing.

- **Nothing is persisted.** `oatpp-postgresql` is in `conanfile.txt` and linked into `picasso_storage`, but no code includes a driver header yet — every repository method throws, and `storage::migrate` does not apply `0001_init.sql`. The dependency is wired, the SQL is written, nothing connects the two.
- **No real auth.** `OpenIdVerifier` and `AuthService::beginLoginUrl`/`completeLogin` are stubs. `AuthService::authenticate` is not a stub — it runs — but it verifies nothing (see security note above), so the `senderSteamId` invariant is structurally enforced but not yet backed by a real identity check.
- **Chat frames are decoded but not acted on.** `WsSession::dispatch` parses `chat_message`/`call_invite` envelopes, switches on the type, and logs — it does not call `ChatService`/`CallSignalService::invite` yet, because both still throw. RTC relay frames (`sdp_offer`, `sdp_answer`, `ice_candidate`, `call_hangup`) are the exception: those are actually relayed now.
- **RTC relay has no membership check.** See the security note above and "Structural invariants".
- **`WsSession` serialises sends with a mutex, not a bounded queue.** Concurrent sends are safe; there is no backpressure, so a slow consumer blocks whoever is fanning out to it. See the TODO in `WsSession.hpp`.
- No client (Kotlin) code talks to this server yet — `ChatRepository.sendToServer()` on the client side is still a `TODO()`.
- `static/` root is a compile-time constant (`PICASSO_STATIC_ROOT`) pointing at the source tree, which won't survive being containerized.
- No tests. `picasso_core` exists to link them against, and `domain/` + `ConnectionHub` are the parts written to be testable without a server, but nothing is written.
- No graceful shutdown — `server.run()` blocks until the process is killed; SIGTERM handling is named in `ServerRunner` but not implemented.

Closed since the previous revision: the hardcoded port/bind (now `PICASSO_*` env vars, defaulting to `0.0.0.0`) and the abort on a failed bind (now logs and exits 1). Also closed: RTC signaling frames now reach `CallSignalService` instead of only being logged, and `/ws` can now be fully entered (with the temporary auth stand-in above) instead of dead-ending at a 501.
