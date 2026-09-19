-- Created by Kirill "Raaveinm" on 9/19/26.
--
-- Identifiers are snake_case

CREATE TABLE IF NOT EXISTS users (
    steam_id bigint PRIMARY KEY
);

-- A separate table, not a column on users, for two reasons. One row per user would
-- cap a user at one live session, and the connection hub deliberately maps one
-- steam_id to several sockets (phone and desktop at once). And a hash does not fit
-- in a bigint: SHA-256 is 256 bits, so storing it as an integer means truncating it
-- and giving up most of its collision resistance.
--
-- token_hash, not token: the plaintext is returned to the client once at login and
-- never stored, so a database dump is not a set of live sessions. expires_at is
-- separate from revoked_at - expiry is automatic, revocation is an explicit logout.
CREATE TABLE IF NOT EXISTS sessions (
    token_hash text   PRIMARY KEY,
    steam_id   bigint NOT NULL REFERENCES users (steam_id) ON DELETE CASCADE,
    created_at bigint NOT NULL,
    expires_at bigint NOT NULL,
    revoked_at bigint
);

CREATE INDEX IF NOT EXISTS sessions_by_steam_id ON sessions (steam_id);

-- Supertype. chat and palette below are its two disjoint specialisations.
--
-- UNIQUE (id, kind) looks redundant next to the primary key, but it is what lets
-- the specialisations key on (conversation_id, kind) and so makes "a conversation
-- is a dm or a palette, never both" a constraint instead of a convention.
CREATE TABLE IF NOT EXISTS conversations (
    id         bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    kind       text   NOT NULL CHECK (kind IN ('dm', 'palette')),
    created_at bigint NOT NULL,
    UNIQUE (id, kind)
);

-- Exactly two members, enforced by having exactly two columns.
--
-- CHECK (member_a < member_b) does double duty: it rules out a conversation with
-- yourself, and it forces one canonical ordering for a pair, so the UNIQUE below
-- actually prevents a second dm between the same two people rather than being
-- sidestepped by inserting them the other way round.
--
-- kind is a generated constant feeding the composite foreign key - a row here can
-- only ever point at a conversation whose kind is already 'dm'.
CREATE TABLE IF NOT EXISTS chat (
    conversation_id bigint PRIMARY KEY,
    kind            text   NOT NULL GENERATED ALWAYS AS ('dm') STORED,
    member_a        bigint NOT NULL REFERENCES users (steam_id),
    member_b        bigint NOT NULL REFERENCES users (steam_id),
    CHECK (member_a < member_b),
    UNIQUE (member_a, member_b),
    FOREIGN KEY (conversation_id, kind) REFERENCES conversations (id, kind) ON DELETE CASCADE
);

-- "Which conversations is this user in" runs on every WS connect and is cached on
-- the session. Without these two it is a sequential scan of every dm on the server.
CREATE INDEX IF NOT EXISTS chat_by_member_a ON chat (member_a);
CREATE INDEX IF NOT EXISTS chat_by_member_b ON chat (member_b);

CREATE TABLE IF NOT EXISTS palette (
    conversation_id bigint PRIMARY KEY,
    kind            text   NOT NULL GENERATED ALWAYS AS ('palette') STORED,
    name            text   NOT NULL,
    FOREIGN KEY (conversation_id, kind) REFERENCES conversations (id, kind) ON DELETE CASCADE
);

-- No role column: within a Palette every member is an equal owner, so there is
-- nothing to distinguish. Adding one later is a migration; enforcing a permission
-- model that does not exist would be dead weight now.
--
-- The primary key is the pair. Keying on palette_id alone would cap a Palette at
-- one member, which is the opposite of what a friend group is.
CREATE TABLE IF NOT EXISTS members (
    palette_id bigint NOT NULL REFERENCES palette (conversation_id) ON DELETE CASCADE,
    user_id    bigint NOT NULL REFERENCES users (steam_id),
    joined_at  bigint NOT NULL,
    PRIMARY KEY (palette_id, user_id)
);

CREATE INDEX IF NOT EXISTS members_by_user_id ON members (user_id);

-- id is the primary key on its own, and globally monotonic. That is what gives the
-- client's "?after={id}" cache sync a total order without a per-conversation
-- counter row to contend on. A composite key over (id, conversation_id) would not
-- guarantee id is unique by itself, which the sync contract depends on.
--
-- Messages hang off conversations, not off chat or palette, so history works the
-- same way for a dm and a Palette.
CREATE TABLE IF NOT EXISTS message_data (
    id              bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    conversation_id bigint NOT NULL REFERENCES conversations (id) ON DELETE CASCADE,
    sender_steam_id bigint NOT NULL REFERENCES users (steam_id),
    text_message    text   NOT NULL,
    sent_at         bigint NOT NULL
);

-- Serves the only hot read: "everything in this conversation after id N". The
-- primary key index is on id alone and cannot answer that without a scan.
CREATE INDEX IF NOT EXISTS message_data_by_conversation_id
    ON message_data (conversation_id, id);

-- game_id is a Steam appid - an opaque number here, with no games table to join.
--
-- enqueued_at exists so that entries of equal priority have a defined order;
-- without it "who has been waiting longest" is unanswerable. UNIQUE (user_id,
-- game_id) stops one user from queueing for the same game twice.
CREATE TABLE IF NOT EXISTS game_queue (
    id          bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id     bigint NOT NULL REFERENCES users (steam_id) ON DELETE CASCADE,
    game_id     int    NOT NULL,
    priority    int    NOT NULL DEFAULT 0,
    enqueued_at bigint NOT NULL,
    UNIQUE (user_id, game_id)
);

CREATE INDEX IF NOT EXISTS game_queue_by_game
    ON game_queue (game_id, priority DESC, enqueued_at);
