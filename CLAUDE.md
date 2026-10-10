# CLAUDE.md
# PickUsAllBackend (picassobackend)

C++20 server for [PickUsAll (Picasso)](https://github.com/raaveinm/PickUsAll) — a KMP/Compose client for gaming-focused friend groups. This repo is the server side referenced as `picassobackend` throughout the client's docs; it lives next to the client (`../picassofrontend`, whose own `CLAUDE.md` is the counterpart to this file), is *not* built from it and doesn't share a build system with it, but its wire protocol has to match what the client's `ChatRepository`/`ChatDao` and `features/impl-webrtc` expect (see "Wire protocol" below).

Product context, terminology (Artist/Palette/Color/...), the multi-server architecture, and the reasoning behind every decision below live in the sibling `../brainstorm/` directory (`brainstorm.md`, `pipeline.md`, and especially `decisions.md`) — this file summarizes what's relevant here, doesn't duplicate the full reasoning.

> **Read "Status" and "Known WIP / gaps" before trusting anything else here.** Parts of this file describe an *agreed plan*, not shipped behaviour. Sections are labelled accordingly.

There is no longer a `docs/` directory (it held `SERVER.md`, `WEBRTC.md` and a chartdb diagram; it was removed). The only user-facing doc is `static/docs.html`, served at `GET /docs` (env vars, build, network exposure, install flags) — **keep it in sync with `install.sh`'s header comment and with the "Configuration" section below**. Facts that used to live in `WEBRTC.md` and still matter are inlined in "Structural invariants".

## Status (2026-10-09)

Auth, storage, the contact graph, conversation creation and **chat** are real. What is left is call setup (`call_invite`), the relay membership check, and ops hardening. The build is **closer to, but still not, safe for a public network** — see the security note in "Known WIP / gaps".

The design behind chat and contacts is `../brainstorm/chat-sync-contract.md` (reasons in `../brainstorm/decisions.md`, "Chat sync & the contact graph"); the client side is in `../picassofrontend/`.

Real, running logic:

- **Config** from the environment (`PICASSO_*`, see "Configuration"). A missing `PICASSO_DB_DSN` fails startup with exit 1. The DSN is password-redacted before it is logged.
- **HTTP**: `GET /ping`, `GET /`, `GET /docs`, `GET /static/css|js/{name}`; unknown route -> `static/not_found.html` (404). API errors that carry a `json-error:` message prefix (`jsonError()` in `ErrorHandler.hpp`) get the JSON error body instead of that HTML page.
- **Steam OpenID login, three-legged** (the Steam redirect lands in a browser, a different process from the app): `GET /auth/steam/begin?state=` -> 302; `GET /auth/steam/return` verifies the assertion and *parks* a minted token under the nonce; `GET /auth/steam/poll?state=` -> 200 once, then 204; `POST /auth/logout`. Sessions are opaque 32-byte tokens, only the SHA-256 is stored, TTL 30 days.
- **`GET /ws`**: 401 without a valid session token; otherwise 101 and the verified steamId is handed to the session through the upgrade parameters.
- **Contacts** (Picasso's own graph; Steam friendship is irrelevant): `contacts` rows are directional, `canCommunicate(a, b)` is the single rule (both rows exist and are ally/friend). REST: `GET /contacts`, `POST /contacts/requests`, `POST /contacts/requests/{id}/accept`, `DELETE /contacts/requests/{id}` (decline or withdraw), `PUT /contacts/{id}` (ally/friend tier or block), `DELETE /contacts/{id}` (remove or unblock). A block deletes the other side's row so it looks like a removal; a request to a blocker or to an unknown steamId gets the same `202` as any other.
- **Conversations**: `POST /conversations` (a DM is get-or-create and needs mutual allies; a palette starts with only its creator and pending invites), `POST /conversations/{id}/invites`, `POST /palette-invites/{id}/accept`, `DELETE /palette-invites/{id}`. Pending invitees live in `palette_invites`, never in `members`.
- **Chat** (`ChatService`): `chat_message` frames are persisted idempotently on `(conversation, sender, client_message_id)`, acked to the sending socket (`chat_ack`), delivered to every other socket of every member (`chat_message_out`, the sender's other devices included) and refused with `chat_nack` (`invalid | too_long | not_member | not_allowed | rate_limited | internal`). A DM is checked against the contact graph on **every** send. Catch-up: `POST /sync` (per-conversation cursors; `delta` or `reset`; deletions since a cursor), scroll-up: `GET /conversations/{id}/messages?before=&limit=`, silent sender-only delete: `DELETE /conversations/{id}/messages/{messageId}` (soft: tombstone kept, `message_deleted` pushed to all members' sockets). Send rate: burst 10, 30/min per user.
- **WS pushes** through a domain `EventSink` -> `WsEventSink` (`transport/ws`): `contact_request`, `contact_updated`, `palette_invite`, `conversation_added`, `conversation_updated`, `chat_message_out`, `message_deleted`. Best effort by design — a device that was offline reads `GET /contacts` and `POST /sync`.
- **Outbound queue**: every `WsSession` has its own writer thread fed by a `BoundedFrameQueue` (512 frames / 4 MiB). Fan-out only enqueues (never waits on a socket); a client that falls behind is **disconnected** (close code 1013) and recovers through `POST /sync`.
- **WS relay frames**: `sdp_offer` / `sdp_answer` / `ice_candidate` / `call_hangup` are relayed to the target steamId's connection(s) with `fromSteamId` stamped by the server. **No membership check yet.**
- **Postgres storage** via `oatpp-postgresql` (pool of 10): `PgChatRepository`, `PgConversationRepository`, `PgContactRepository`, `PgSessionRepository`. **The schema migrates itself at startup** from `migrations/0001_init.sql`.
- **Presence/fan-out**: `ConnectionHub` (one steamId -> many sockets, `weak_ptr`, sends outside the lock, `sendToAllExcept` for "everyone but the connection that sent it"). Instance-local, no Redis.
- **Logging** to the console and `logs/log_file<MM-DD-YYYY>.csv`; **tests + CI** (see "Tests"); **deploy tooling** (see "Deployment").

Still stubbed (`notImplemented` -> uniform 501): `CallSignalService::invite` (`call_invite` is logged only).

Does not exist at all: handling of `incoming_call` / `call_accept` / `call_decline` / `peer_joined` / `call_leave` (the DTOs exist; `dispatch` logs "unhandled frame type"); a way to deliver ICE (STUN/TURN) config to clients and a coturn in compose; any code behind `game_queue`; leaving/kicking/renaming a palette and withdrawing an invite (not in the contract yet); the `friend` tier's only planned use (a Friend may add you to a palette without asking) is a **future release** — `friend` is stored and settable, nothing reads it.

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
    Rest.hpp                  — REST-only DTOs: errors, auth, contacts, conversations, sync, history
    Mappers.hpp               — domain -> wire DTOs, shared by REST and WS so a conversation looks the same either way
    MessageType.hpp/.cpp      — plain enum class + parse/toWireString
  domain/                     — plain C++: no oat++, no SQL, no sockets. The testable part.
    Ids.hpp                   — strong types SteamId, ConversationId, MessageId, ConnectionId
    Message.hpp  Conversation.hpp  Contact.hpp  Session.hpp  Clock.hpp  Errors.hpp (notImplemented)
    Events.hpp                — what the server tells connected clients, in domain terms
    ports/
      ChatRepository.hpp  ConversationRepository.hpp  ContactRepository.hpp  SessionRepository.hpp
      PresenceRegistry.hpp    — "who is online on this instance"
      SignalTransport.hpp     — "deliver this frame to that steamId" (+ "...except that connection")
      EventSink.hpp           — publish a domain event; transport/ws turns it into frames
  service/
    ChatService  ContactService  ConversationService  AuthService  CallSignalService
    Services.hpp/.cpp (makeServices)
  storage/                    — the only place SQL lives
    PicassoDatabaseClient.hpp — QUERY macros + startup SchemaMigration
    Rows.hpp  QueryCheck.hpp  — result-row DTOs; require()/returnedRow() helpers for query results
    PgChatRepository  PgConversationRepository  PgContactRepository  PgSessionRepository
    Repositories.hpp/.cpp     — makeRepositories(dsn): pool + executor + the repos
    migrations/0001_init.sql
  steam/
    OpenIdVerifier            — buildAuthUrl() + static verify() (check_authentication over TLS)
  logger/
    ActivityLogger  CsvLogger — console + daily CSV
  transport/
    http/  HealthController  AuthController  ContactController  ConversationController  HttpSupport  ErrorHandler  HttpModule
    ws/    WsController  WsModule  WsSession  BoundedFrameQueue  ConnectionHub  EnvelopeCodec  Outbound  WsEventSink
test/
  transport/http/   HealthControllerTest + an in-process HttpTestServer
  transport/ws/     BoundedFrameQueue and ConnectionHub tests (no socket needed)
  storage/          repository + service tests against real Postgres (chat, contacts, conversations, sessions)
static/             index.html docs.html not_found.html css/ js/
config/             Caddyfile  prometheus.yml  grafana/provisioning/...
deploy/firewall/    picasso-firewall.sh + systemd service/timer (installed by install.sh)
```

Each module directory owns a `CMakeLists.txt` declaring one static library and its dependencies; `src/CMakeLists.txt` aggregates them into the `picasso_core` interface target that `PickUsAllBackend` links. Tests link the module targets they need (`picasso_transport_http`, `picasso_storage`), not `main`.

**Dependency direction is the load-bearing rule:** `transport → service → domain/ports ← storage`. The per-module `target_link_libraries` calls are what enforce it — a layer reaching sideways fails to link rather than merely looking wrong in review. Concretely: `picasso_domain` links only `picasso_base` (no oat++, no driver, no sockets), `picasso_service` links neither oat++ nor any `Pg*` class, and `oatpp-postgresql` is `PRIVATE` to `picasso_storage` (the storage *test* links it directly on purpose, to seed rows no port can write).

Two module entry points are worth knowing about:

- `service::makeServices` takes `PresenceRegistry`, `SignalTransport` and `EventSink` as arguments rather than building them: the first two are implemented by `ConnectionHub` and the last by `WsEventSink`, both in `transport/ws` — a layer *above*. Services decide who is told what (domain events); only `transport/ws` knows what a frame looks like, which is what keeps JSON and sockets out of `service/`.
- That forces a multi-phase composition root, spelled out in `app::buildComponents`: repositories -> hub -> event sink -> services -> HTTP controllers -> WS endpoint. The order is a consequence of the layering, not a preference.

## Structural invariants

These are the reasons the layering exists. If a change makes one of them a matter of developer discipline rather than a matter of what compiles, the change is wrong.

- **`senderSteamId` comes from the authenticated connection, never from the payload.** Enforced by splitting inbound and outbound DTOs: `ChatMessageInDto` has no sender field at all, so there is nothing to trust. `ChatService::submit(SteamId sender, ...)` takes the sender as a parameter, and the only caller that can supply it is `WsSession`, whose identity is a `const` constructor argument set after the token check on upgrade. The identity is now backed by a real session lookup. For the relay frames the same rule is applied by overwriting `fromSteamId` server-side.
- **SDP/ICE forwarding must be membership-checked — currently violated.** "Forward by `toSteamId` without parsing" taken literally makes the server an open relay: any authenticated user could push arbitrary payloads at any steamId, bypassing conversation membership. `CallSignalService::relayToPeer` forwards every `sdp_offer`/`sdp_answer`/`ice_candidate`/`call_hangup` with no check; a `TODO(roadmap step 4/5)` in `CallSignalService.cpp` says so. It is an open relay for *any authenticated* user (no longer for any integer, since auth is real). Closing it needs (a) a conversation-creation path (nothing can create one today) and (b) the client sending the **server's** conversation id (it currently sends its local Room id — see "Open contract items"). ICE candidates arrive in bursts, so the user's conversation set should be cached on the session at connect time (`conversationsOf`) rather than re-queried per candidate.
- **One writer per socket — enforced by construction.** oat++'s blocking `sendOneFrameText` is not safe to call concurrently, and fan-out happens on another connection's thread. So *nothing but a session's own writer thread writes to its socket*: every other thread (and the ping reply) only pushes onto a `BoundedFrameQueue`. The queue never blocks a pusher and never drops a chat frame; when a client cannot keep up it **overflows**, and the writer closes the connection (1013). That is safe only because catch-up is complete (`POST /sync`). The three-tier drop idea from the old design (only `ice_candidate` may be dropped) is **not implemented** — relayed frames are opaque strings at this layer, so an overflow disconnects regardless of type. `WsSession::shutdown()` must run before the socket is destroyed (the writer holds a raw pointer); `SessionFactory::onBeforeDestroy` does that.
- **Per-conversation ordering.** A client's cursor is "the highest message id I hold", so ids must reach every recipient in commit order — if 11 arrived before 10, a disconnect in between would lose 10 for good. `ChatService` therefore holds a per-conversation lock (a striped mutex) from the insert until the delivery is *queued* on every recipient, and the delete path takes the same lock. This is valid only because queueing is non-blocking (above) and because the server is one process per deployment — if that ever changes (horizontal scaling), revisit it rather than patching with a database lock.
- **Persist, then ack, then fan out.** The ack to the sender is produced from the stored row; the others are told only for a *new* message (a retry — `duplicate` — is acked again and delivered to nobody).
- **Contacts gate DMs and palette invitations, never anything inside a palette.** `canCommunicate` is the one predicate; `ChatService` re-checks it on every DM send. An unknown steamId, a stranger and a blocker are deliberately answered identically (`not_allowed` / `403`) so the API is no probe for "who is on this server" or "who blocked me".

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

One WS connection per client per server, carrying **both** chat and RTC signaling — every frame is an `EnvelopeDto`; `type` says which payload field is populated. All ids are **strings** on the wire (a 64-bit steamId doesn't survive a JSON number in every client); all times are epoch **milliseconds**. The full contract with reasoning is `../brainstorm/chat-sync-contract.md`; the client's mirror of these shapes is `shared/.../data/server/Wire.kt`.

| `type` | payload field | direction | meaning |
|---|---|---|---|
| `chat_message` | `chatMessage` `{conversationId, clientMessageId, body}` | client → server | a new message. `clientMessageId` is a UUID the client mints; there is **no sender field** |
| `chat_ack` | `chatAck` `{conversationId, clientMessageId, message, duplicate}` | server → the sending socket only | stored; `message` carries the server id and clock; `duplicate` = a retry of one already stored |
| `chat_nack` | `chatNack` `{conversationId, clientMessageId, code}` | server → the sending socket | refused: `not_member`, `not_allowed`, `too_long`, `invalid` are permanent; `rate_limited`, `internal` are worth a retry |
| `chat_message_out` | `chatMessageOut` `{message}` | server → every other socket of every member | live delivery (the sender's other devices included) |
| `message_deleted` | `messageDeleted` `{conversationId, messageId}` | server → every socket of every member | silent removal |
| `conversation_added` / `conversation_updated` | `conversation` | server → clients | a DM/palette now includes this user / its membership changed |
| `contact_request` | `contactRequest` `{steamId, createdAt}` | server → the target | someone asked to be your ally (not sent when silently dropped) |
| `contact_updated` | `contactUpdated` `{steamId, level}` | server → both parties | their own row about `steamId` changed; `level` null = stranger (also how a block looks to the blocked) |
| `palette_invite` | `paletteInvite` | server → the invitee | `state` `pending`, or `resolved` once handled on another device |
| `call_invite` | `callInvite` | client → server | logged only (`invite` still throws) |
| `incoming_call` / `call_accept` / `call_decline` / `peer_joined` / `call_leave` | `callSignal` | both | DTOs only; `dispatch` logs "unhandled frame type" |
| `sdp_offer` / `sdp_answer` | `sdp` | both, routed by `toSteamId` | opaque SDP; relayed, `fromSteamId` server-stamped, **no membership check** |
| `ice_candidate` | `iceCandidate` | both, routed by `toSteamId` | opaque; the client packs `{sdpMid, sdpMLineIndex, candidate}` as nested JSON inside the one `candidate` string |
| `call_hangup` | `callHangup` | both, routed by `toSteamId` | the frame itself is the hang-up |

Malformed frames and unknown types are dropped and logged at debug; a throwing service call is caught in `dispatch` and logged so it can't take the connection down. An unparsable `chat_message` is answered with `chat_nack{invalid}`.

The shipped client only speaks the four relay frames for calls: the **offer itself is the ring**, one call at a time. `call_invite` and friends are the planned richer protocol, not something the client uses.

A missed call needs no durability treatment. Chat does: the client keeps an outbox (see `../picassofrontend/CLAUDE.md`) and the server dedups its retries.

### REST surface

Every endpoint except health and the login legs needs `Authorization: Bearer <session token>`; the caller's identity comes from it and nowhere else. A missing/invalid token is `401`.

| endpoint | notes |
|---|---|
| `GET /ping`, `/`, `/docs`, `/static/css/{n}`, `/static/js/{n}` | `/ping` is what the client's server-reachability check hits |
| `GET /auth/steam/begin?state=` · `GET /auth/steam/return` · `GET /auth/steam/poll?state=` · `POST /auth/logout` | login (see Status) |
| `GET /ws` | WebSocket upgrade, Bearer session token |
| `GET /contacts` | `{contacts, incoming, outgoing, paletteInvites}` — the caller's own view, blocklist included |
| `POST /contacts/requests` `{steamId}` | `202` for every case that must look alike (waiting, silently dropped, unknown steamId); `200` if it resolved into a contact; `409 unblock_first`; `422` self; `429` over the limits (20/day, 50 pending) |
| `POST /contacts/requests/{id}/accept` · `DELETE /contacts/requests/{id}` | accept; decline-or-withdraw (silent) |
| `PUT /contacts/{id}` `{level}` · `DELETE /contacts/{id}` | `ally`/`friend` tier or `imposter` (block); remove or unblock |
| `POST /conversations` | `{kind:"dm", peerSteamId}` (get-or-create, `200`/`201`) or `{kind:"palette", name, inviteSteamIds}` (all-or-nothing); `403 not_allowed` for anything not mutual allies |
| `POST /conversations/{id}/invites` `{steamId}` · `POST /palette-invites/{id}/accept` · `DELETE /palette-invites/{id}` | invite (any member, own allies only); accept; decline (silent, 7-day re-invite cooldown) |
| `POST /sync` | `{cursors:[{conversationId, after}], deletedSince?, limit?}` -> conversations (each `delta` or `reset`, with `hasMoreBefore`), `deleted`, `deletedCursor`. Walks **all** of the caller's conversations |
| `GET /conversations/{id}/messages?before=&limit=` | scroll-up page, ascending, `hasMoreBefore`; `limit` is clamped to 200 |
| `DELETE /conversations/{id}/messages/{messageId}` | silent delete, sender only, `204`, idempotent |

`404` is used for "no such thing" and "not yours" alike so existence isn't revealed; those carry the JSON error body (see `jsonError`).

### Open contract items

1. **ICE servers.** The client configures no STUN/TURN (works on LAN/NAT-friendly paths only). Needs a delivery mechanism (e.g. in the login response or a REST call) *and* a coturn in the compose stack.
2. **Relay membership check.** Now unblocked — both sides use server conversation ids — and still not done (`CallSignalService::relayToPeer`).
3. The numbers (rate limits, body size, request caps) are placeholders from the contract; palette leave/kick/rename and withdrawing an invite are not designed yet.

## Database schema

Lives in `src/storage/migrations/0001_init.sql` and **is applied at startup** by `SchemaMigration` (schema name `picasso_database`, version 1; statements are `CREATE ... IF NOT EXISTS`).

> **Until the first build is handed to anyone running `install.sh`, schema changes are made IN PLACE in `0001_init.sql`** (the contacts, palette-invite and message-sync columns all landed that way). `SchemaMigration` records version 1 as applied, so an existing dev database must be **dropped** (`docker compose down -v`), or the new DDL never runs. After that freeze point a change becomes `0002_*.sql` plus a matching `migration.addFile(2, ...)` in `PicassoDatabaseClient`'s constructor — don't edit 0001 on a deployed database.

```
users         (steam_id PK)                      — a row on first login, or as a stub when someone sends them a contact request
sessions      (token_hash PK, steam_id -> users ON DELETE CASCADE, created_at, expires_at, revoked_at)

conversations (id identity PK, kind CHECK ('dm'|'palette'), created_at, UNIQUE(id, kind))
  +-- chat    (conversation_id PK, kind generated 'dm', member_a -> users, member_b -> users)
  +-- palette (conversation_id PK, kind generated 'palette', name)
                +-- members        (palette_id -> palette, user_id -> users, joined_at)   PK(palette_id, user_id)
                +-- palette_invites(palette_id, invitee_id, inviter_id, created_at, declined_at) PK(palette_id, invitee_id)

message_data  (id identity PK, conversation_id -> conversations, sender_steam_id -> users,
               client_message_id, text_message, sent_at, deleted_at,
               UNIQUE(conversation_id, sender_steam_id, client_message_id))
               — index (conversation_id, id); partial index on deleted_at

contacts         (owner_id, other_id, level CHECK ('imposter'|'ally'|'friend'), since)   PK(owner_id, other_id)
contact_requests (from_id, to_id, created_at, declined_at)                               PK(from_id, to_id)

game_queue    (id identity PK, user_id -> users, game_id int, priority, enqueued_at,
               UNIQUE(user_id, game_id))
```

All ids and timestamps are `bigint`; timestamps are epoch **milliseconds**, matching the domain structs (`Message::created_at_epoch_ms`). `ConversationId` is therefore `int64`, not a string. Column names are unquoted snake_case on purpose — Postgres folds unquoted identifiers to lower case, so a column created as `"steamId"` stays quoted in every query forever. Because `chat.member_*`, `members.user_id` and `message_data.sender_steam_id` reference `users`, **a user must have logged in at least once before they can be added to a conversation or send a message.**

`conversations` is a supertype with two **disjoint** specialisations. `UNIQUE (id, kind)` looks redundant next to the PK, but it is what lets `chat` and `palette` key on `(conversation_id, kind)` via a generated constant `kind` column — that turns "a conversation is a dm or a palette, never both" into a constraint rather than a convention. Cost: `isMember`/`conversationsOf` are UNION queries over `chat` and `members`; the ports don't change, the repository absorbs it.

Decisions worth not re-litigating:

- **`contacts` is directional, "stranger" is the absence of a row.** A row is "owner regards other as level": a block is one-sided and the ally/friend tier is what the *owner* shares. Ally-ness stays mutual because accepting writes both rows; a block deletes the other side's row (it looks like a removal to them); a removal deletes both. `canCommunicate` = both rows exist and are ally/friend.
- **Pending palette invites are a table of their own, not a status on `members`**, so `isMember`/`members`/`conversationsOf` never need a filter — a forgotten filter would let a pending invitee read a palette.
- **`client_message_id` is a client-minted UUID**, the idempotency key; deliberately not the client's Room row id (an app-data wipe restarts that at 1 and the server would drop a brand-new message as a "retry").
- **Deletion is a tombstone.** `deleted_at` is set and the text emptied; the row stays, so the key above keeps working (a late retry cannot resurrect it) and no "client was offline too long" edge case exists. Every history read filters `deleted_at IS NULL`.
- **Declines are kept as `declined_at` markers** (contact requests and palette invites) so a re-request/re-invite within 7 days is silently dropped without the sender learning of the decline.

- **No `role` on `members`.** Within a Palette every member is an equal owner. Adding one later is a migration; enforcing a permission model that doesn't exist would be dead weight.
- **`members` PK is the pair `(palette_id, user_id)`.** Keying on `palette_id` alone caps a Palette at one member.
- **`message_data.id` is the primary key on its own**, global and monotonic, plus an index on `(conversation_id, id)`. A composite PK over `(id, conversation_id)` would not make `id` unique by itself, and the cursors of `POST /sync` depend on exactly that. Within one conversation ids are delivered in commit order (see the per-conversation lock under Structural invariants). Messages hang off `conversations`, not off `chat`/`palette`, so history works the same for a DM and a Palette.
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

- `picasso_transport_http_test` — an in-process server (`HttpTestServer`): `/ping`, unknown route -> 404, static-asset traversal rejected.
- `picasso_transport_ws_test` — no socket needed: `BoundedFrameQueue` (overflow disconnects instead of dropping or blocking; byte limit; close wakes a blocked writer) and `ConnectionHub` ("everyone except the connection that sent it").
- `picasso_storage_test` — integration tests against **real Postgres** named by `PICASSO_TEST_DB_DSN`; suites that need it report as skipped when it is unset, and CI makes an unreachable database fail rather than pass silently. **They `TRUNCATE` every table — never point this at a database you care about.** Covers the repositories (chat: idempotent append, newest/older paging, soft delete, `deletedSince`; contacts; conversations; sessions) **and the services on top of them** (`ChatServiceTest`: idempotency, delivery to everyone but the origin connection, stranger/frozen-DM refusals, validation and rate limiting with a controllable clock, **concurrent senders delivered in id order**, `delta`/`reset` sync, history paging, silent delete; `SocialServiceTest`: the contact state machine, indistinguishable answers for blocked/unknown, palette invites, pushed events).

The client has its own suite against a *running* server — `shared/src/jvmTest/.../ChatIntegrationTest.kt` and `ChatEndToEndTest.kt` (real Room, real WebSockets, real `SyncCoordinator`); see `../picassofrontend/CLAUDE.md`. They need the server seeded with users `1..200` having sessions `tok-<n>` (steamId `76561198000000000 + n`).

CI: `.github/workflows/main.yaml` (named `http-test`) installs gcc-11, Conan and a `postgres:17` service, then builds and runs the http, ws and storage suites. Not covered by any test: `AuthService`, the OpenID flow, `WsSession` itself (the pieces it is made of are), the controllers beyond health, `redactDsn`.

Local equivalent (macOS notes: no cmake/ninja on PATH — use CLion's; configure out of tree; `docker run postgres:17` for the DSN):

```
cmake --build build --config Release --target picasso_transport_http_test picasso_transport_ws_test picasso_storage_test
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
curl -i http://127.0.0.1:8000/contacts     # -> 401 (no token)

docker compose up -d --build               # full stack; needs POSTGRES_PASSWORD in .env
```

**Use `--config`, not the build preset.** Conan's generated `conan-release` build preset sets no configuration, so under Ninja Multi-Config `cmake --build --preset conan-release` builds **Debug** and leaves `build/Release/PickUsAllBackend` stale. That combination silently tests an old binary.

## Roadmap

Order was constrained, not arbitrary: auth before storage (the first rows of `message_data` must carry a verified sender), storage before the relay check (membership comes from the database).

1. ~~**Skeleton**~~ Done.
2. ~~**WS transport**~~ Done: hub, session, codec, relay frames, per-connection writer + bounded queue, chat dispatch, event sink.
3. ~~**Auth**~~ Done in functionality. **Hardening owed** — OpenID `return_to` check, expired-session purge.
4. ~~**Storage + conversation creation + contacts + chat**~~ Done (2026-10-09). Remaining there: palette leave/kick/rename and invite withdrawal (not in the contract yet).
5. **RTC signaling** — forwarding works; the **membership check is the next small step** (the data it needs now exists and the client sends server ids). `invite()` and the richer call frames are unimplemented (the client doesn't need them). Add ICE-server delivery + coturn.
6. **Ops** — graceful shutdown, published image instead of VPS compilation, `/metrics`, applying `PICASSO_LOG_LEVEL`, session purge.

## Known WIP / gaps

> **Security note:** safer than before, but still **not safe to expose on a public network**:
> - `CallSignalService::relayToPeer` forwards signaling frames from any authenticated user to any steamId with no conversation-membership check.
> - `OpenIdVerifier::verify` forwards the assertion to Steam but never checks that `openid.return_to` / `openid.realm` is *this* server's URL, nor that `claimed_id` is under `https://steamcommunity.com/openid/id/`. Steam's `check_authentication` only proves Steam signed the assertion for *some* relying party, so an assertion obtained by a different site could be accepted here and mint a session for that user. Compare `return_to` against `publicUrl_` (and bind it to the pending `state`) before this faces the internet.
> - `install.sh` cannot fix either; its own header says so.
> - An inbound WS message is buffered without a size cap (`WsSession::inbound_`); a hostile client can make the server buffer arbitrarily much. Cap it and close with 1009.

- **Uncommitted `Dockerfile` edit worth checking:** the builder stage installs `g++-20 gcc-20`, which Ubuntu 22.04 does not package; the verified toolchain is `g++-11` (CI and the build section above). Revert unless a newer base image is being adopted on purpose.
- **Chat is built but not load-tested**; the bounded queue's limits (512 frames / 4 MiB) and the rate limits are placeholders. An overflow closes the connection with 1013 and the client relies on its reconnect + `/sync` — verified in unit tests and by the client's end-to-end suite, but not under a real slow consumer.
- **A retry of a message that was deleted since returns the tombstone** (`duplicate: true`, empty body) rather than a rejection; the client then resolves its row with an empty text. Reachable only if another device deleted a message whose ack was lost — rare, and the next sync deletes it locally.
- **RTC relay has no membership check.** `ice_candidate` is never dropped under pressure (see the one-writer invariant).
- **`onPong` is a no-op** — no liveness tracking of idle connections.
- **No graceful shutdown** — `server.run()` blocks until the process is killed; no SIGTERM/SIGINT handling anywhere.
- **Expired/revoked sessions are never purged**; pending logins are in-memory (lost on restart, single instance only).
- **`PICASSO_LOG_LEVEL` is read but unused.** The CSV logger records everything the console does.
- **The Steam `check_authentication` call builds a fresh TLS client per login** — fine at friend-group scale.
- **Monitoring is host-level only** — no application metrics.
- **No coturn / STUN-TURN config delivery**, so calls only work on LAN / NAT-friendly paths.
- **`README.md` is a stub**; `static/docs.html` is the real user doc.
- `static/` root and migrations path are compile-time absolute constants — fine in the container (same `WORKDIR`), brittle anywhere else.

Closed since the previous revision: contacts (directional graph, requests, blocks, tiers), conversation and palette creation with confirmed invitations, idempotent chat with ack/nack/fan-out, `POST /sync`, history paging, silent delete, the bounded single-writer outbound queue, and the domain event sink.
