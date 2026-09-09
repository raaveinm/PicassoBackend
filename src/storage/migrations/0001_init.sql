-- Created by Kirill "Raaveinm" on 9/9/26.
-- Initial schema. Not applied by anything yet - see storage::migrate (roadmap step 4).
--
-- Timestamps are bigint epoch milliseconds rather than TIMESTAMPTZ, matching the
-- domain structs (Message::createdAtEpochMs and friends) so nothing has to convert
-- between a database time type and the value that goes on the wire.

CREATE TABLE IF NOT EXISTS artists (
    steam_id     bigint PRIMARY KEY,
    display_name text   NOT NULL,
    avatar_url   text,
    last_seen_at bigint NOT NULL
);

CREATE TABLE IF NOT EXISTS conversations (
    id         bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    kind       varchar NOT NULL CHECK (kind IN ('dm', 'palette')),
    title      text    NOT NULL,
    created_at bigint  NOT NULL,
    created_by bigint  NOT NULL REFERENCES artists (steam_id)
);

-- No role column: within a Palette every member is an equal owner, so there is
-- nothing to distinguish. Adding one later is a migration; enforcing a permission
-- model that does not exist would be dead weight now.
CREATE TABLE IF NOT EXISTS conversation_members (
    conversation_id bigint NOT NULL REFERENCES conversations (id) ON DELETE CASCADE,
    steam_id        bigint NOT NULL REFERENCES artists (steam_id),
    joined_at       bigint,
    PRIMARY KEY (conversation_id, steam_id)
);

-- Membership is read per connection and per signaling frame, always by steam_id.
CREATE INDEX IF NOT EXISTS conversation_members_by_steam_id
    ON conversation_members (steam_id);

-- id is the primary key on its own, and globally monotonic. That is what gives the
-- client's "?after={id}" cache sync a total order without a per-conversation counter
-- row to contend on. A composite PK over (id, conversation_id) would not guarantee
-- id is unique by itself, which the sync contract depends on.
CREATE TABLE IF NOT EXISTS messages (
    id              bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    conversation_id bigint NOT NULL REFERENCES conversations (id) ON DELETE CASCADE,
    sender_steam_id bigint NOT NULL REFERENCES artists (steam_id),
    text            text   NOT NULL,
    timestamp       bigint NOT NULL
);

-- Serves the only hot read: "everything in this conversation after id N". The PK
-- index is on (id) alone and cannot answer that without a scan, hence this one.
CREATE INDEX IF NOT EXISTS messages_by_conversation_id
    ON messages (conversation_id, id);

-- token_hash, not token: the plaintext is returned to the client once at login and
-- never stored, so a database dump does not hand over live sessions. The column
-- holds a SHA-256 of what the client presents, and lookup hashes before comparing.
--
-- expires_at is separate from revoked_at: revocation is an explicit logout, expiry
-- is automatic. Without it a token stays valid until someone logs out by hand.
CREATE TABLE IF NOT EXISTS session (
    token_hash text   PRIMARY KEY,
    steam_id   bigint NOT NULL REFERENCES artists (steam_id),
    created_at bigint NOT NULL,
    expires_at bigint NOT NULL,
    revoked_at bigint
);

CREATE INDEX IF NOT EXISTS session_by_steam_id ON session (steam_id);
