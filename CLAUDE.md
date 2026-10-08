# CLAUDE.md
# PickUsAllBackend (picassobackend)

C++20 server for [PickUsAll (Picasso)](https://github.com/raaveinm/PickUsAll) — a KMP/Compose client for gaming-focused friend groups. This repo is the server side referenced as `picassobackend` throughout the client's docs; it lives next to the client (`../picassofrontend`, whose own `CLAUDE.md` is the counterpart to this file), is *not* built from it and doesn't share a build system with it, but its wire protocol has to match what the client's `ChatRepository`/`ChatDao` and `features/impl-webrtc` expect (see "Wire protocol" below).

Product context, terminology (Artist/Palette/Color/...), the multi-server architecture, and the reasoning behind every decision below live in the sibling `../brainstorm/` directory (`brainstorm.md`, `pipeline.md`, and especially `decisions.md`) — this file summarizes what's relevant here, doesn't duplicate the full reasoning.

> **Read "Status" and "Known WIP / gaps" before trusting anything else here.** Parts of this file describe an *agreed plan*, not shipped behaviour. Sections are labelled accordingly.

There is no longer a `docs/` directory (it held `SERVER.md`, `WEBRTC.md` and a chartdb diagram; it was removed). The only user-facing doc is `static/docs.html`, served at `GET /docs` (env vars, build, network exposure, install flags) — **keep it in sync with `install.sh`'s header comment and with the "Configuration" section below**. Facts that used to live in `WEBRTC.md` and still matter are inlined in "Structural invariants".

## Status (2026-10-08)

Auth and storage are real now; chat delivery and call setup are the remaining stubs. The build is **closer to, but still not, safe for a public network** — see the security note in "Known WIP / gaps".

Real, running logic:

- **Config** from the environment (`PICASSO_*`, see "Configuration"). A missing `PICASSO_DB_DSN` fails startup with exit 1 (`makeRepositories` throws on an empty DSN). The DSN is password-redacted (`redactDsn`) before it is logged.
- **HTTP**: `GET /ping` -> 200 `pong` + `X-Service-Status: Healthy`; `GET /` -> `static/index.html`; `GET /docs` -> `static/docs.html`; `GET /static/css/{name}`, `/static/js/{name}` (name allow-list + traversal rejection); unknown route -> `static/not_found.html` (404).
- **Steam OpenID login, three-legged** (the Steam redirect lands in a browser, a different process from the app, so the app can't read it): `GET /auth/steam/begin?state=<nonce>` -> 302 to Steam; `GET /auth/steam/return` verifies the assertion (`check_authentication` POST back to steamcommunity over oatpp-openssl), mints a session token, *parks* it under the `state` nonce and serves a "Signed in, close this tab" page; `GET /auth/steam/poll?state=<nonce>` -> 200 `{token, steamId (string), expiresAt}` once, then 204 forever (204 also means "not ready", "already claimed" and "expired" — indistinguishable on purpose); `POST /auth/logout` (Bearer) -> 204, idempotent. Without a `state` the return leg answers with the token DTO directly (bare browser login).
- **Sessions**: opaque 32-byte random token (hex) returned once; only its SHA-256 is stored (`sessions.token_hash`). TTL 30 days (`SESSION_TTL`). `AuthService::authenticate` hashes the presented token and requires `isUsableAt(now)` (not revoked, not expired). Parked logins live in an in-memory map, single-use, pruned after 5 min (`PENDING_LOGIN_TTL_MS`) — lost on restart, which is fine for a 5-minute handoff.
- **`GET /ws`**: 401 without `Authorization: Bearer <token>`, 401 for an unknown/expired/revoked token; otherwise 101 and the verified steamId is handed to the session through the upgrade parameters. The token must be the *session token from login*, not a steamId — the old "literal steamId" stand-in is gone.
- **WS frames**: `sdp_offer` / `sdp_answer` / `ice_candidate` / `call_hangup` are relayed to the target steamId's connection(s) with `fromSteamId` stamped by the server (`CallSignalService::relayToPeer`). **No membership check yet.**
- **Postgres storage** via `oatpp-postgresql` (pool of 10, 2 min TTL): `PgChatRepository` (`append`, `historyAfter`), `PgConversationRepository` (`isMember`, `members`, `conversationsOf`), `PgSessionRepository` (`store` — also upserts the `users` row in one transaction —, `findByTokenHash`, `revoke`). **The schema migrates itself at startup**: constructing `PicassoDatabaseClient` runs `oatpp::orm::SchemaMigration` over `migrations/0001_init.sql` (path baked in as `DATABASE_MIGRATIONS`).
- **Presence/fan-out**: `ConnectionHub` (one steamId -> many sockets, `weak_ptr`, sends outside the lock). Instance-local, no Redis.
- **Logging**: every `OATPP_LOG*` goes to the console *and* to `logs/log_file<MM-DD-YYYY>.csv` (`logger/ActivityLogger`: daily rotation, 7-day retention by the date in the file name; `CsvLogger` is installed before anything logs — `run()` builds the `ActivityLogger` first). `docker-compose.yaml` mounts `./logs`.
- **Failed bind** logs `cannot bind <addr>:<port>` and exits 1 instead of aborting.
- **Tests + CI** (see "Tests").
- **Deploy tooling**: `Dockerfile`, `docker-compose.yaml` (postgres, backend, caddy, node-exporter, prometheus, grafana), `install.sh`, Cloudflare-aware firewall (see "Deployment").

Still stubbed, throwing `notImplemented(...)` which `ErrorHandler` turns into a uniform 501 JSON naming the roadmap step:

- `ChatService::submit` (roadmap 4) — `WsSession::dispatch` logs `chat_message` and **does not call it**.
- `CallSignalService::invite` (roadmap 5) — `call_invite` is logged only.
- `GET /conversations/{conversationId}/messages?after=&limit=` -> 501 (`ConversationController`). The repository method behind it (`historyAfter`) works and is tested; the controller just isn't wired.

Does not exist at all (not a stub — there is no code or port for it):

- **Any way to create a conversation, palette or membership.** `ConversationRepository` is read-only (`isMember`/`members`/`conversationsOf`); there is no REST endpoint, no WS frame and no port method that inserts into `conversations`/`chat`/`palette`/`members`. The storage tests seed those rows with raw SQL. Until a creation path exists, `isMember` can never be true in a real deployment, which means the membership check (step 5) can't be switched on without also adding creation.
- `chat_ack` / `chat_message_out` production, `incoming_call` / `call_accept` / `call_decline` / `peer_joined` / `call_leave` handling (the enum and DTOs exist; `dispatch` falls to `default:` and logs "unhandled frame type").
- A way to deliver ICE (STUN/TURN) server config to clients; `compose` ships no coturn.
- `game_queue` has a table and nothing else — no port, service or endpoint.

## What this server actually does (per the client's multi-server design)

Each Palette/DM conversation lives authoritatively on exactly one server instance (self-hosted or managed) — this server *is* SSOT for whatever conversations get created on it. The client holds saved servers in its Room cache but is connected to the **one the user selected** — servers never talk to each other, so a conversation is only ever reachable through the server that owns it (selection UI is not built; the client currently takes the newest-added server, see `../picassofrontend/CLAUDE.md`). Concretely, this server:

- Is a thin **signaling router for RTC calls** — it never touches call media (audio). Real audio is P2P mesh between call participants; the server only forwards opaque SDP/ICE payloads by steamId. See "Wire protocol".
- Is the **SSOT for chat messages** — a client's local Room cache is a cache; this server's Postgres is the source of truth for a conversation's history (once `ChatService` writes to it).
- **Authenticates** users: it is the only place a Steam OpenID assertion can be verified (the client can't check Steam's signature itself), and it mints/validates the session tokens that every later request and the WS upgrade carry.
- Does **not** run Steam Web API calls on the client's behalf — the client talks to Steam directly for profiles, libraries, friends.

## Layout

```
main.cpp                      — only calls app::run()
src/
  app/
    App.hpp/.cpp              — run(): logger -> Environment::init -> config -> buildComponents -> serve
                                (the composition root; buildComponents is the two-phase wiring below)
    Config.hpp/.cpp           — env vars + redactDsn()
    Components.hpp            — struct of everything built at startup
    ServerRunner.hpp/.cpp     — bind with a real error instead of an abort; blocking server.run()
  dto/
    Envelope.hpp              — EnvelopeDto + payload DTOs
    Rest.hpp                  — ErrorDto, AuthTokenDto, ConversationDto, MessagePageDto
    MessageType.hpp/.cpp      — plain enum class + parse/toWireString
  domain/                     — plain C++: no oat++, no SQL, no sockets. The testable part.
    Ids.hpp                   — strong types SteamId, ConversationId, MessageId
    Message.hpp  Conversation.hpp  Session.hpp  Clock.hpp  Errors.hpp (notImplemented)
    ports/
      ChatRepository.hpp  ConversationRepository.hpp  SessionRepository.hpp
      PresenceRegistry.hpp    — "who is online on this instance"
      SignalTransport.hpp     — "deliver this frame to that steamId"
  service/
    ChatService  AuthService  CallSignalService  Services.hpp/.cpp (makeServices)
  storage/                    — the only place SQL lives
    PicassoDatabaseClient.hpp — QUERY macros + startup SchemaMigration
    Rows.hpp                  — result-row DTOs (ScalarInt64Row, MessageRow, SessionRow, ExistsRow)
    PgChatRepository  PgConversationRepository  PgSessionRepository
    Repositories.hpp/.cpp     — makeRepositories(dsn): pool + executor + the three repos
    migrations/0001_init.sql
  steam/
    OpenIdVerifier            — buildAuthUrl() + static verify() (check_authentication over TLS)
  logger/
    ActivityLogger  CsvLogger — console + daily CSV
  transport/
    http/  HealthController  AuthController  ConversationController  ErrorHandler  HttpModule
    ws/    WsController  WsModule  WsSession  ConnectionHub  EnvelopeCodec  Outbound
test/
  transport/http/   HealthControllerTest + an in-process HttpTestServer
  storage/          Chat/Conversation/Session repository tests against real Postgres
static/             index.html docs.html not_found.html css/ js/
config/             Caddyfile  prometheus.yml  grafana/provisioning/...
deploy/firewall/    picasso-firewall.sh + systemd service/timer (installed by install.sh)
```

Each module directory owns a `CMakeLists.txt` declaring one static library and its dependencies; `src/CMakeLists.txt` aggregates them into the `picasso_core` interface target that `PickUsAllBackend` links. Tests link the module targets they need (`picasso_transport_http`, `picasso_storage`), not `main`.

**Dependency direction is the load-bearing rule:** `transport → service → domain/ports ← storage`. The per-module `target_link_libraries` calls are what enforce it — a layer reaching sideways fails to link rather than merely looking wrong in review. Concretely: `picasso_domain` links only `picasso_base` (no oat++, no driver, no sockets), `picasso_service` links neither oat++ nor any `Pg*` class, and `oatpp-postgresql` is `PRIVATE` to `picasso_storage` (the storage *test* links it directly on purpose, to seed rows no port can write).

Two module entry points are worth knowing about:

- `service::makeServices` takes `PresenceRegistry` and `SignalTransport` as arguments rather than building them, because both are implemented by `ConnectionHub` — which lives a layer *above* in `transport/ws`.
- That forces a two-phase composition root, spelled out in `app::buildComponents`: repositories -> hub -> services -> HTTP controllers -> WS endpoint. The order is a consequence of the layering, not a preference.

## Structural invariants

These are the reasons the layering exists. If a change makes one of them a matter of developer discipline rather than a matter of what compiles, the change is wrong.

- **`senderSteamId` comes from the authenticated connection, never from the payload.** Enforced by splitting inbound and outbound DTOs: `ChatMessageInDto` has no sender field at all, so there is nothing to trust. `ChatService::submit(SteamId sender, ...)` takes the sender as a parameter, and the only caller that can supply it is `WsSession`, whose identity is a `const` constructor argument set after the token check on upgrade. The identity is now backed by a real session lookup. For the relay frames the same rule is applied by overwriting `fromSteamId` server-side.
- **SDP/ICE forwarding must be membership-checked — currently violated.** "Forward by `toSteamId` without parsing" taken literally makes the server an open relay: any authenticated user could push arbitrary payloads at any steamId, bypassing conversation membership. `CallSignalService::relayToPeer` forwards every `sdp_offer`/`sdp_answer`/`ice_candidate`/`call_hangup` with no check; a `TODO(roadmap step 4/5)` in `CallSignalService.cpp` says so. It is an open relay for *any authenticated* user (no longer for any integer, since auth is real). Closing it needs (a) a conversation-creation path (nothing can create one today) and (b) the client sending the **server's** conversation id (it currently sends its local Room id — see "Open contract items"). ICE candidates arrive in bursts, so the user's conversation set should be cached on the session at connect time (`conversationsOf`) rather than re-queried per candidate.
- **One writer per socket.** Fan-out happens on another connection's thread, and oat++'s blocking `sendOneFrameText` is not safe to call concurrently on one socket. Today `WsSession::send` serialises with a mutex (safe, but a slow consumer blocks whoever is fanning out to it). The intended shape is an outbound queue with a single writer; bounding it gives backpressure, and the drop policy has **three** tiers, not two: chat is never dropped (the server is SSOT), call control and SDP are never dropped (losing an `sdp_offer` kills session setup silently and nothing retries it), and only `ice_candidate` may be dropped — candidates are numerous, partly redundant, and more keep arriving. See the `TODO(roadmap step 2)` in `WsSession.hpp`.
- **Persist, then ack, then fan out.** `ChatService::submit` must not ack durability the database hasn't confirmed (shape is written out in the stub's comment).

Presence needs no external store — the multi-server design makes presence inherently instance-local, so `PresenceRegistry` is in-memory. No Redis, and no coturn in the compose stack (the earlier "oat++ + PostgreSQL + Redis + coturn" plan was revised in `../brainstorm/`; coturn is still wanted for ICE, see Open contract items).

## Architecture decisions

Reasoning for the first and third lives in `../brainstorm/decisions.md` under "Server stack (picassobackend)" — summarized here:

- **oat++, not Drogon.** Switched for two reasons: near-zero required dependencies beyond OpenSSL (matters because Free Tier deploys via a one-line `install.sh` onto an arbitrary VPS), and more declarative `ENDPOINT`/DTO macros than Drogon offers for typical JSON REST/WS handling. The migration is complete — no Drogon left.
- **Blocking oat++ API, not the async one.** `oatpp::network::Server` + `HttpConnectionHandler` is thread-per-connection, which with long-lived WS connections means thread-per-client. Accepted deliberately: a Palette is a friend-group construct, not a public room, and Free Tier targets a small VPS — tens to low hundreds of concurrent connections is the real workload. The async API scales further but poisons the event loop on any blocking call, and `oatpp-postgresql`'s pool *is* blocking, which would force DB work onto a separate executor group. Containment requirement: keep the concurrency model inside `ws/ConnectionHub` and `ws/WsSession` so a future migration doesn't spread. Worth lowering thread stack size — the 8 MB default times a few hundred connections is wasted memory on a small box.
- **RTC is mesh + signaling-only**, not a server-side SFU/media mixer. A real SFU is a substantial media-engine undertaking that oat++ doesn't give you for free; projects that need one embed mediasoup/Janus/LiveKit rather than hand-roll it. This server only forwards opaque signaling payloads; WebRTC media is a full P2P mesh between whoever's in the call. Accepted trade-off: mesh is O(n²) connections — fine for a DM or small Palette, not a large room. (The shipped client only does 1:1 DM calls.)
- **Postgres via `oatpp-postgresql`** (1.3.0, which is what forced the whole family off `.latest` — see gotchas), not libpqxx. **Decided and built.** Maps query results onto DTO-style row objects, ships a connection pool, and has built-in `SchemaMigration` — no external migration tool, which matters for the one-line self-host story. Cost: another macro-heavy layer, and coupling to oat++ idioms. Wrapped: `PicassoDatabaseClient.hpp` holds the `QUERY` macros, the `Pg*Repository` classes expose a domain-shaped API, and nothing above `storage/` ever sees `DbClient`.
- **Plain headers, not C++20 modules** — see below.

### C++20 modules: tried, dropped

An earlier iteration used real modules (`.cppm`, `FILE_SET CXX_MODULES`). That is gone: `CMakeLists.txt` sets `CMAKE_CXX_SCAN_FOR_MODULES OFF`. Don't reintroduce them without a reason — the toolchain cost was real (module dependency scanning requires `clang-scan-deps`, which ruled out AppleClang entirely and forced a Homebrew LLVM toolchain on macOS). `../brainstorm/decisions.md` records the reversal.

One finding worth keeping if anyone reconsiders: **oat++'s `ENUM(...)` macro cannot be exported from a module.** It expands to a file-scope `static` variable as an initializer trick, and C++20 forbids exporting an internal-linkage entity — a hard compiler error. With plain headers this restriction does not apply. `MessageType` is a plain `enum class` anyway (`domain/`-adjacent code must not depend on oat++); conversion to the wire string happens at the DTO boundary.

## Wire protocol

One WS connection per client per server, carrying **both** chat and RTC signaling — every frame is an `EnvelopeDto`; `type` says which payload field is populated. All ids are **strings** on the wire (a 64-bit steamId doesn't survive a JSON number in every client).

| `type` | payload field | direction | meaning | `WsSession::dispatch` today |
|---|---|---|---|---|
| `chat_message` | `chatMessage` (`ChatMessageInDto`) | client → server | new message; `localMessageId` is the client's outbox row id | logged only, no service call (`ChatService::submit` still throws) |
| `chat_ack` | `chatAck` (`ChatAckDto`) | server → sender | "your message landed" — echoes `localMessageId`, carries the server `messageId` | never produced |
| `call_invite` | `callInvite` (`CallInviteDto`) | client → server | "start/join a call in this conversation" | logged only (`invite` still throws) |
| `incoming_call` / `call_accept` / `call_decline` / `peer_joined` / `call_leave` | `callSignal` (`CallSignalDto`) | both | `{conversationId, steamId}` notifications | `default:` case, logged as "unhandled frame type" |
| `sdp_offer` / `sdp_answer` | `sdp` (`SdpDto`) | both, routed by `toSteamId` | opaque SDP text, never parsed | relayed via `relayToPeer`, `fromSteamId` server-stamped, **no membership check** |
| `ice_candidate` | `iceCandidate` (`IceCandidateDto`) | both, routed by `toSteamId` | opaque; the client packs `{sdpMid, sdpMLineIndex, candidate}` as nested JSON inside the one `candidate` string | relayed, same caveats |
| `call_hangup` | `callHangup` (`CallHangupDto`) | both, routed by `toSteamId` | the frame itself is the hang-up | relayed, same caveats |

`EnvelopeDto` also carries `chatMessageOut` (`ChatMessageOutDto`), reserved for fan-out to other members; no wire `type` maps to it yet. Malformed frames and unknown types are dropped and logged at debug; a throwing service call is caught in `dispatch` and logged so it can't take the connection down.

The shipped client (`features/impl-webrtc`) only speaks the four relay frames: it treats the **offer itself as the ring** (no `call_invite`/`incoming_call`/`call_accept`), and handles one call at a time. The `call_invite` flow in the DTOs is the planned richer protocol, not something the client uses.

A missed call needs no durability treatment — if the target isn't online, it just doesn't connect. Chat messages do, but durability there is entirely the client's job (durable local write before send, the outbox pattern in `../picassofrontend/CLAUDE.md`); the server needs no special handling for a disconnected recipient beyond "they'll get it next time they sync".

### REST surface

| endpoint | status | notes |
|---|---|---|
| `GET /ping`, `/`, `/docs`, `/static/css/{n}`, `/static/js/{n}` | real | `/ping` is what the client's server-reachability check hits |
| `GET /auth/steam/begin?state=` | real | 302 to Steam |
| `GET /auth/steam/return` | real | forwards every `openid.*` param to Steam; parks token under `state` |
| `GET /auth/steam/poll?state=` | real | 200 once, then 204 |
| `POST /auth/logout` | real | Bearer; 204, idempotent |
| `GET /ws` | real | Bearer session token |
| `GET /conversations/{id}/messages?after=&limit=` | **501** | `MessagePageDto{messages, nextAfter}`; `limit` defaults to 100 — **clamp it** when implementing |

### Open contract items (need agreeing with the client before step 4)

1. **Sync mechanism.** The server is SSOT and the client's Room is a cache, but nothing tells the cache what it missed. Needs `chat_ack` to carry the server `messageId` plus the (stubbed) `GET /conversations/{id}/messages?after={id}`. Until then "they'll get it next time they sync" is unimplementable.
2. **Timestamp units differ.** Server: `message_data.sent_at` and `ChatMessageOutDto.createdAt` are epoch **milliseconds**. Client: `MessageData.timestamp` is `Clock.System.now().epochSeconds`. One side must convert at the boundary; decide where.
3. **Conversation id spaces.** Wire/server ids are the Postgres `bigint` identity. The client has its own autoincrement `Conversations.id` and a separate `Conversations.remoteId` for the server's id. Today the client sends the *local* id as `conversationId` in call frames, and creates DMs locally with `remoteId = peer steamId` as a placeholder. Both must switch to a real server id once conversations can be created here.
4. **`kind` strings.** Server: `'dm'` / `'palette'`. Client Room: `"chat"` / `"palette"`. Map at the boundary; don't change either schema for it.
5. **ICE servers.** The client configures no STUN/TURN (works on LAN/NAT-friendly paths only). Needs a delivery mechanism (e.g. in the login response or a REST call) *and* a coturn in the compose stack.
6. **Conversation creation + membership** (see Status) — protocol undefined.

## Database schema

Lives in `src/storage/migrations/0001_init.sql` and **is applied at startup** by `SchemaMigration` (schema name `picasso_database`, version 1; statements are `CREATE ... IF NOT EXISTS`). A schema change means adding `0002_*.sql` and a matching `migration.addFile(2, ...)` in `PicassoDatabaseClient`'s constructor — don't edit 0001 in place on a deployed database.

```
users         (steam_id PK)                      — row created on a user's first login (PgSessionRepository::store)
sessions      (token_hash PK, steam_id -> users ON DELETE CASCADE, created_at, expires_at, revoked_at)

conversations (id identity PK, kind CHECK ('dm'|'palette'), created_at, UNIQUE(id, kind))
  +-- chat    (conversation_id PK, kind generated 'dm', member_a -> users, member_b -> users)
  +-- palette (conversation_id PK, kind generated 'palette', name)
                +-- members (palette_id -> palette, user_id -> users, joined_at)
                            PK(palette_id, user_id)

message_data  (id identity PK, conversation_id -> conversations, sender_steam_id -> users,
               text_message, sent_at)           — index (conversation_id, id)

game_queue    (id identity PK, user_id -> users, game_id int, priority, enqueued_at,
               UNIQUE(user_id, game_id))
```

All ids and timestamps are `bigint`; timestamps are epoch **milliseconds**, matching the domain structs (`Message::created_at_epoch_ms`). `ConversationId` is therefore `int64`, not a string. Column names are unquoted snake_case on purpose — Postgres folds unquoted identifiers to lower case, so a column created as `"steamId"` stays quoted in every query forever. Because `chat.member_*`, `members.user_id` and `message_data.sender_steam_id` reference `users`, **a user must have logged in at least once before they can be added to a conversation or send a message.**

`conversations` is a supertype with two **disjoint** specialisations. `UNIQUE (id, kind)` looks redundant next to the PK, but it is what lets `chat` and `palette` key on `(conversation_id, kind)` via a generated constant `kind` column — that turns "a conversation is a dm or a palette, never both" into a constraint rather than a convention. Cost: `isMember`/`conversationsOf` are UNION queries over `chat` and `members`; the ports don't change, the repository absorbs it.

Decisions worth not re-litigating:

- **No `role` on `members`.** Within a Palette every member is an equal owner. Adding one later is a migration; enforcing a permission model that doesn't exist would be dead weight.
- **`members` PK is the pair `(palette_id, user_id)`.** Keying on `palette_id` alone caps a Palette at one member.
- **`message_data.id` is the primary key on its own**, global and monotonic, plus an index on `(conversation_id, id)`. A composite PK over `(id, conversation_id)` would not make `id` unique by itself, and the `?after={id}` sync contract depends on exactly that. Messages hang off `conversations`, not off `chat`/`palette`, so history works the same for a DM and a Palette.
- **`sessions` is a table, not a column on `users`.** One row per user would cap a user at one live session, and `ConnectionHub` deliberately maps one steamId to several sockets. A hash also does not fit in a `bigint`.
- **`token_hash`, not `token`.** The plaintext is returned once at login and never stored, so a database dump is not a set of live sessions. `expires_at` is separate from `revoked_at`: expiry is automatic, revocation is an explicit logout. (Nothing purges expired rows yet.)
- **`CHECK (member_a < member_b)` on `chat`** rules out a conversation with yourself *and* forces one canonical ordering, so `UNIQUE (member_a, member_b)` can't be sidestepped by inserting the pair the other way round. Whoever writes the creation path must sort the pair.

`game_queue` has no server code behind it — no port, service or endpoint. `game_id` is a Steam appid, opaque here. The client's play-next queue is **per-user state that must survive switching servers**: it is merged across servers by timestamp, latest change wins, including removals (see `../brainstorm/decisions.md`). So this table will need an `updated_at` and a tombstone (`deleted_at`/flag) — it has only `enqueued_at` — plus a sync endpoint, in a `0002_*.sql` migration. Not built; client side needs Room v5 too.

## Configuration

| variable | default | notes |
|---|---|---|
| `PICASSO_BIND` | `0.0.0.0` | |
| `PICASSO_PORT` | `8000` | must be 1..65535 or startup fails |
| `PICASSO_DB_DSN` | *(none)* | libpq keyword/value or URI form. **Required.** compose builds it from `POSTGRES_*` |
| `PICASSO_PUBLIC_URL` | `http://127.0.0.1:8000` | becomes the OpenID `realm` and `return_to` — must be the URL Steam's browser redirect can actually reach, so behind Caddy it's the public `https://domain` |
| `PICASSO_LOG_LEVEL` | `info` | read and logged, **not applied yet** |

compose/install additionally use `POSTGRES_USER`, `POSTGRES_PASSWORD` (required by compose), `PICASSO_DOMAIN`, `PICASSO_BEHIND_CLOUDFLARE`, `PICASSO_SKIP_FIREWALL`, `PICASSO_SSH_PORT` — documented in `install.sh`'s header and `static/docs.html`.

## Deployment

`docker-compose.yaml` services on one bridge network `monitoring`: `postgres` (17, bound to `127.0.0.1:5432` only, healthcheck gates `backend`), `backend` (built from `Dockerfile`, `expose: 8000` — **not published**; only Caddy reaches it), `caddy` (80/443, `config/Caddyfile`: `/statics/*` -> Grafana, everything else -> `backend:8000`; automatic HTTPS when `PICASSO_DOMAIN` is a real domain, plain `:80` otherwise), `node-exporter`, `prometheus` (scrapes node-exporter only — the backend exposes no `/metrics`), `grafana` (anonymous disabled, served under `/statics`).

- **The VPS compiles the server.** `install.sh` clones the repo into `/opt/picassobackend` and runs `docker compose up -d --build`, which runs the multi-stage `Dockerfile` (Ubuntu 22.04, g++-11, Conan in a venv, tests disabled via `-DPICASSO_BUILD_TESTS=OFF`) — "a few minutes on first run". The older design note that the VPS "only ever runs a prebuilt image" is **not what ships today**; a published image would be the fix and is not done.
- The runtime image keeps `WORKDIR /app` and copies `static/` and `src/storage/migrations/` to the *same absolute paths the builder used*, because `PICASSO_STATIC_ROOT` and `DATABASE_MIGRATIONS` are compile-time absolute paths. **Don't change the WORKDIR or those copy destinations without also changing how the defines are set.**
- **Cloudflare mode** (`PICASSO_BEHIND_CLOUDFLARE=1`, requires a domain): `install.sh` downloads Cloudflare's ranges into `config/generated/trusted_proxies.caddy` (Caddy then trusts `CF-Connecting-IP` from those ranges only — never trust that header otherwise, it's forgeable) and the firewall allows 80/443 from Cloudflare only.
- **Firewall**: Docker-published ports bypass ufw, so `deploy/firewall/picasso-firewall.sh` manages a `PICASSO-INGRESS` chain hooked from `DOCKER-USER`; a systemd service + daily timer (installed from `deploy/firewall/*` with `@INSTALL_DIR@` substituted) re-applies it. `PICASSO_SKIP_FIREWALL=1` opts out.
- `.env` (git-ignored) holds the DB credentials; `install.sh` preserves existing ones and restricts values to `[A-Za-z0-9._~-]` because they land unquoted in the libpq DSN.
- `install.sh` ends by curling `http://127.0.0.1/ping` up to 30 s.

## Build

Verified working configuration: Linux, `g++-11`, Ninja Multi-Config, Conan from the repo-local `.venv`. `conan_conf.cmake` runs `conan install` during configure and derives the Conan profile from `CMAKE_CXX_COMPILER_VERSION`, so the compiler you pass at bootstrap determines the profile. It also has an `APPLE` branch that profiles Apple toolchains as plain `clang`; the macOS path is not currently exercised.

`conanfile.txt`: `boost/1.91.0`, `oatpp`, `oatpp-websocket`, `oatpp-postgresql`, `oatpp-openssl` (all `1.3.0`), `libpq/17.11`. All oat++ pieces are static.

**First-time setup cannot use `cmake --preset conan-release`.** It fails with `File not found: build/CMakePresets.json` on a clean checkout — that file is generated by Conan's `CMakeToolchain`, which only runs once CMake has already started configuring. Bootstrap once with a direct invocation:

```
cmake -S . -B build -G "Ninja Multi-Config" \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-11 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-11 \
  -DCMAKE_BUILD_TYPE=Release
```

After that succeeds once, `build/CMakePresets.json` exists and the presets work normally.

### Gotchas that cost real time

- **`conan_conf.cmake` used to pin `[tool_requires] cmake/3.19.8`** — a workaround from the Drogon/libpqxx era. Removed: oat++ itself needs CMake ≥3.20 to build from source, so the old pin actively broke the build. If a future dependency needs an old CMake again, pin it then — don't reintroduce this preemptively.
- **Boost 1.91.0's Conan Center recipe fails to package `boost_cobalt_io_ssl`** regardless of settings — an upstream recipe bug. Fixed via `boost/*:without_cobalt=True` in `conanfile.txt` (Boost.Cobalt isn't used).
- **`find_package(Boost)` needs `CONFIG` explicit**, or modern CMake's policy `CMP0167` (FindBoost module removed) makes it fall back to CMake's own legacy `FindBoost.cmake`, which then fails to find anything Conan installed. Use `find_package(Boost REQUIRED CONFIG COMPONENTS ...)`. Components in use: `thread` (app) and `unit_test_framework` (tests).
- **`Boost::system` is not a linkable target in this Boost version** — Boost.System became header-only. Don't link it.
- **The build directory is multi-config.** Conan generates `build-Debug.ninja` / `build-Release.ninja` / `build-RelWithDebInfo.ninja` alongside `build.ninja`, and binaries land in `build/<Config>/`, not `build/`. A bare `ninja` in `build/` does not necessarily build the config you think it does.
- **`cmake_policy(SET CMP0111 OLD)` in `CMakeLists.txt` is load-bearing.** Raising `cmake_minimum_required` to 3.20 flips CMP0111 to NEW, which turns a missing `IMPORTED_LOCATION` from a warning into a hard configure error. Conan's CMakeDeps files declare both `CONAN_LIB::*_DEBUG` and `*_RELEASE` targets but only populate the one matching the installed `build_type`. Nothing links the empty targets, so OLD is correct — don't "fix" this by pinning the project back to an ancient CMake minimum.
- **oat++ 1.3.0's actual header paths differ from what most examples suggest** — this version still nests things under `core/` and the old `parser/json/mapping/` path:
  - `oatpp/core/macro/codegen.hpp`, not `oatpp/macro/codegen.hpp`
  - `oatpp/parser/json/mapping/ObjectMapper.hpp` + `oatpp::parser::json::mapping::ObjectMapper`, not `oatpp/json/ObjectMapper.hpp`
  - `oatpp/core/base/Environment.hpp` + `oatpp::base::Environment`, not `oatpp::Environment`
  - `oatpp/web/server/api/ApiController.hpp`, `oatpp/network/Server.hpp`, `oatpp/network/tcp/server/ConnectionProvider.hpp`, `oatpp/web/server/HttpRouter.hpp`, `oatpp/web/server/HttpConnectionHandler.hpp` matched expectations.
  - If a new header 404s, check the actual installed tree before guessing again: `find <conan package folder>/include -iname '<HeaderName>.hpp'`.
- **Pin the whole oat++ family to `1.3.0`, never `1.3.0.latest`.** ConanCenter ships both the tagged release and a package built from upstream's `latest` branch, and they are different versions to Conan. `oatpp-postgresql` only exists as `1.3.0` and depends on `oatpp/1.3.0`, so mixing produces a hard `Version conflict`.
- **The two lines are not API-compatible.** `1.3.0.latest` has `ErrorHandler::handleError(const std::exception_ptr&)`; tagged `1.3.0` does not — `HttpProcessor` catches `std::exception` itself and hands the handler a ready-made 500. That is why `picasso::kNotImplementedPrefix` exists: with the exception type unavailable, a message prefix is the only way to tell "planned, not built" from "broke". (`ErrorHandler::handleError` is marked `[[deprecated]]` in the source for the same reason — it's the only override available.)
- **`oatpp::String` has no `std_str()` in 1.3.0.** It is an `ObjectWrapper` around `std::string` — write `*value`, not `value->std_str()`.
- **`HttpError::getInfo()` and `getMessage()` are not const.** Catch by non-const reference.
- **`Header` has no `LOCATION` constant** — only `CONTENT_TYPE`, `AUTHORIZATION` and a few others. Use the `"Location"` literal for redirects (`AuthController` does).
- **Query parameters arrive still percent-encoded**, which is why `AuthController` has its own `decodeParam`: Steam's `openid.*` values (and the `state` nonce) must be decoded before being re-encoded into the `check_authentication` form body, or the signature check fails.

## Tests

Boost.Test, built by default (`-DPICASSO_BUILD_TESTS=OFF` to skip; the Dockerfile does):

- `picasso_transport_http_test` — spins up an in-process server (`HttpTestServer`): `/ping`, unknown route -> 404, static-asset traversal rejected.
- `picasso_storage_test` — integration tests against **real Postgres** named by `PICASSO_TEST_DB_DSN`; suites that need it report as skipped when it is unset, and CI makes an unreachable database fail rather than pass silently. **They `TRUNCATE` every table — never point this at a database you care about.** Covers chat append/history (ordering, `after`, limit, per-conversation scoping, verbatim text), membership (DM symmetric, palette, non-member, `members`, `conversationsOf`) and sessions (round-trip, revoke, duplicate hash, `store` upserting a missing user), plus `makeRepositories("")` throwing.

CI: `.github/workflows/main.yaml` (named `http-test`; PRs and pushes to `main`) installs gcc-11, Conan and a `postgres:17` service, then builds and runs both suites. Not covered by any test: `AuthService`, the OpenID flow, `WsSession`/`ConnectionHub`, the controllers beyond health, `redactDsn`. `domain/` + `ConnectionHub` are written to be testable without a server.

Local equivalent of what CI does (after the bootstrap above; derived from the CI workflow and `test/CMakeLists.txt`, not run while writing this):

```
cmake --build build --config Release --target picasso_transport_http_test picasso_storage_test
PICASSO_TEST_DB_DSN="host=127.0.0.1 port=5432 dbname=picasso_test user=picasso password=picasso" \
  ctest --test-dir build -C Release --output-on-failure
```

## Useful build/verify commands

```
cmake --build build --config Release      # after first-time bootstrap, see above
PICASSO_DB_DSN="host=127.0.0.1 dbname=picasso_database user=picasso password=..." \
  ./build/Release/PickUsAllBackend         # binds 0.0.0.0:8000 unless PICASSO_* says otherwise
curl -i http://127.0.0.1:8000/ping         # -> 200, "pong", X-Service-Status: Healthy
curl -i http://127.0.0.1:8000/             # -> 200, static/index.html
curl -i http://127.0.0.1:8000/ws           # -> 401 (no token)
curl -i http://127.0.0.1:8000/conversations/1/messages   # -> 501 JSON naming roadmap step 4

docker compose up -d --build               # full stack; needs POSTGRES_PASSWORD in .env
```

**Use `--config`, not the build preset.** Conan's generated `conan-release` build preset sets no configuration, so under Ninja Multi-Config `cmake --build --preset conan-release` builds **Debug** and leaves `build/Release/PickUsAllBackend` stale. That combination silently tests an old binary.

## Roadmap

Order is constrained, not arbitrary: step 3 must precede step 4, or the first rows written to `message_data` carry an unverified sender. Step 5 follows 4 because membership comes from the database.

1. ~~**Skeleton** — `app/`, config from env, a real error on a failed bind, one JSON error handler.~~ Done.
2. **WS transport** — hub, session, codec done; relay frames wired. Missing: dispatch for `chat_message`/`call_invite`, and the bounded single-writer outbound queue. Unblocks the client: `ChatRepository.sendToServer()` needs this plus step 4.
3. ~~**Auth** — Steam OpenID, session tokens, validation on WS upgrade.~~ Done in functionality (login/poll/logout, hashed sessions, real WS auth). **Hardening still owed** — see Known gaps (OpenID `return_to` check, expired-session purge).
4. **Storage** — repositories and migrations are done. Remaining: **conversation/palette/member creation path**, `ChatService::submit` (persist -> ack -> fan out), `GET /conversations/{id}/messages`, and settling the contract items above.
5. **RTC signaling** — forwarding is implemented; the membership check is not and depends on step 4's creation path. `invite()` and the richer call frames are unimplemented (the client doesn't need them yet). Add ICE-server delivery + coturn.
6. **Ops** (new) — graceful shutdown, published image instead of VPS compilation, `/metrics`, applying `PICASSO_LOG_LEVEL`, session purge.

## Known WIP / gaps

> **Security note:** safer than before (a bearer token must now be a real, unexpired, unrevoked session), but still **not safe to expose on a public network**:
> - `CallSignalService::relayToPeer` forwards signaling frames from any authenticated user to any steamId with no conversation-membership check.
> - `OpenIdVerifier::verify` forwards the assertion to Steam but never checks that `openid.return_to` / `openid.realm` is *this* server's URL, nor that `claimed_id` is under `https://steamcommunity.com/openid/id/`. Steam's `check_authentication` only proves Steam signed the assertion for *some* relying party, so an assertion obtained by a different site could be accepted here and mint a session for that user. Compare `return_to` against `publicUrl_` (and bind it to the pending `state`) before this faces the internet.
> - `install.sh` cannot fix either; its own header says so.

- **Chat frames are decoded but not acted on** — `ChatService::submit` is a stub; nothing is ever written to `message_data` by the server.
- **No conversation creation path** (see Status) — the largest structural hole; it blocks chat, membership and the client's DM/Palette creation at once.
- **`GET /conversations/{id}/messages` answers 501.**
- **RTC relay has no membership check.**
- **`WsSession` serialises sends with a mutex, not a bounded queue**, and has no liveness tracking (`onPong` is a no-op).
- **No graceful shutdown** — `server.run()` blocks until the process is killed; there is no SIGTERM/SIGINT handling anywhere (the old comment claiming `ServerRunner` names it is gone). A `docker stop` therefore waits out the grace period and kills the process.
- **Expired/revoked sessions are never purged**; pending logins are in-memory (lost on restart, single instance only).
- **`PICASSO_LOG_LEVEL` is read but unused.** The CSV logger records everything the console does.
- **The Steam `check_authentication` call builds a fresh TLS client per login** (`OpenIdVerifier::verify` is `static`) — fine at friend-group scale.
- **Monitoring is host-level only** — no application metrics.
- **No coturn / STUN-TURN config delivery**, so calls only work on LAN / NAT-friendly paths.
- **`README.md` is a stub** (install one-liner + `PICASSO_DOMAIN`); `static/docs.html` is the real user doc.
- `static/` root and migrations path are compile-time absolute constants — fine in the container (same `WORKDIR`), brittle anywhere else.

Closed since the previous revision: real Steam OpenID + hashed sessions + real WS auth; Postgres repositories and self-applying migrations; structured CSV logging; compose/Caddy/Grafana/firewall/Cloudflare deployment; Boost.Test suites and CI; `PICASSO_*` config with DSN redaction; failed-bind handling; RTC relay frames reaching `CallSignalService`.
