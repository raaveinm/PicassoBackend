#!/usr/bin/env bash
#
# PickUsAllBackend installer - bootstraps the self-hosted Free Tier stack
# (backend + Caddy, via docker compose) on a fresh Ubuntu/Debian VPS.
#
#   curl -fsSL https://raw.githubusercontent.com/raaveinm/picassobackend/main/install.sh | bash
#   curl -fsSL .../install.sh | PICASSO_DOMAIN=chat.example.com bash   # with a real domain -> automatic HTTPS
#
# Optional flags (environment variables; also documented in static/docs.html, keep both in sync):
#   PICASSO_DOMAIN=chat.example.com   domain already pointed at this server; blank = HTTP only, no TLS
#   PICASSO_BEHIND_CLOUDFLARE=1       the domain is proxied by Cloudflare (orange cloud): 80/443 are then
#                                     reachable from Cloudflare ranges only, and the real client IP is read
#                                     from CF-Connecting-IP. Requires PICASSO_DOMAIN. Default 0.
#   POSTGRES_USER=picasso             database user. Default: the saved value, else "picasso".
#   POSTGRES_PASSWORD=...             database password. Default: the saved value, else a random one.
#                                     Both apply only when the database volume is first created.
#   PICASSO_SKIP_FIREWALL=1           do not install the DOCKER-USER rules (you manage the firewall yourself)
#   PICASSO_SSH_PORT=22               SSH port to keep open when ufw is active
#
# the current build has known-insecure stand-ins (WS auth, RTC relay) that this installer does not and cannot fix.

set -euo pipefail

REPO_URL="${PICASSO_REPO_URL:-https://github.com/raaveinm/picassobackend.git}"
INSTALL_DIR="${PICASSO_INSTALL_DIR:-/opt/picassobackend}"
BRANCH="${PICASSO_BRANCH:-main}"

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31mERROR\033[0m %s\n' "$*" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] || die "run as root (installs system packages and Docker)."
command -v curl >/dev/null 2>&1 || die "curl is required."

OS_ID=""
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS_ID="$ID"
fi
case "$OS_ID" in
    ubuntu|debian) ;;
    *) die "supports Ubuntu/Debian only (found: ${OS_ID:-unknown}).
     Install Docker + the compose plugin yourself, then run: docker compose up -d --build" ;;
esac

log "Installing prerequisites (git, curl, ca-certificates)..."
apt-get update -qq
apt-get install -y -qq git ca-certificates curl openssl iptables >/dev/null

if ! command -v docker >/dev/null 2>&1; then
    log "Docker not found, installing via get.docker.com..."
    curl -fsSL https://get.docker.com | sh
else
    log "Docker already installed ($(docker --version))."
fi
docker compose version >/dev/null 2>&1 || die "docker compose
plugin missing even after install - check the Docker install above."

if [ -d "$INSTALL_DIR/.git" ]; then
    log "Existing install at $INSTALL_DIR, updating to origin/$BRANCH..."
    git -C "$INSTALL_DIR" fetch --depth 1 origin "$BRANCH"
    git -C "$INSTALL_DIR" reset --hard "origin/$BRANCH"
else
    log "Cloning $REPO_URL into $INSTALL_DIR..."
    git clone --depth 1 --branch "$BRANCH" "$REPO_URL" "$INSTALL_DIR"
fi
cd "$INSTALL_DIR"

# --- domain / TLS ---
if [ -z "${PICASSO_DOMAIN:-}" ] && [ -e /dev/tty ]; then
    read -r -p "Domain already pointed at this server's IP
    (blank = HTTP only, no TLS): " PICASSO_DOMAIN < /dev/tty || true
fi
PICASSO_DOMAIN="${PICASSO_DOMAIN:-:80}"

if [ "$PICASSO_DOMAIN" = ":80" ]; then
    warn "No domain set - running HTTP-only, Caddy will not request a certificate."
    warn "Re-run with PICASSO_DOMAIN=your-domain.com once DNS points here for automatic HTTPS."
    PUBLIC_IP="$(curl -fsS -4 --max-time 3 https://ifconfig.me 2>/dev/null || echo "localhost")"
    PICASSO_PUBLIC_URL="http://${PUBLIC_IP}"
else
    PICASSO_PUBLIC_URL="https://${PICASSO_DOMAIN}"
fi

# --- Cloudflare / firewall flags ---
SKIP_FIREWALL="${PICASSO_SKIP_FIREWALL:-0}"
SSH_PORT="${PICASSO_SSH_PORT:-22}"

if [ -z "${PICASSO_BEHIND_CLOUDFLARE:-}" ]; then
    if [ "$PICASSO_DOMAIN" != ":80" ] && [ -e /dev/tty ]; then
        read -r -p "Is this domain proxied by Cloudflare (orange cloud)? [y/N] " CF_ANSWER < /dev/tty || true
        case "${CF_ANSWER:-n}" in
            y|Y|yes|YES) PICASSO_BEHIND_CLOUDFLARE=1 ;;
            *) PICASSO_BEHIND_CLOUDFLARE=0 ;;
        esac
    else
        PICASSO_BEHIND_CLOUDFLARE=0
    fi
fi
if [ "$PICASSO_BEHIND_CLOUDFLARE" = "1" ] && [ "$PICASSO_DOMAIN" = ":80" ]; then
    warn "PICASSO_BEHIND_CLOUDFLARE=1 needs PICASSO_DOMAIN - ignoring it."
    PICASSO_BEHIND_CLOUDFLARE=0
fi

# --- .env: keeps existing database credentials (compose refuses to start without POSTGRES_PASSWORD) ---
env_get() { if [ -f .env ]; then grep -E "^$1=" .env | tail -n1 | cut -d= -f2- || true; fi; }

# Precedence: environment flag > existing .env > default (user) / generated (password).
# Both values end up unquoted in .env and inside the libpq connection string, so only characters that
# need no quoting are accepted.
POSTGRES_USER="${POSTGRES_USER:-$(env_get POSTGRES_USER)}"
POSTGRES_USER="${POSTGRES_USER:-picasso}"
POSTGRES_PASSWORD="${POSTGRES_PASSWORD:-$(env_get POSTGRES_PASSWORD)}"
if [ -z "$POSTGRES_PASSWORD" ]; then
    POSTGRES_PASSWORD="$(openssl rand -hex 24)"
    log "Generated a new POSTGRES_PASSWORD (stored in $INSTALL_DIR/.env only)"
fi
for DB_VALUE in "$POSTGRES_USER" "$POSTGRES_PASSWORD"; do
    case "$DB_VALUE" in
        *[!A-Za-z0-9._~-]*) die "POSTGRES_USER/POSTGRES_PASSWORD may contain only letters, digits and . _ ~ -" ;;
    esac
done
if [ "$(env_get POSTGRES_PASSWORD)" != "" ] && [ "$(env_get POSTGRES_PASSWORD)" != "$POSTGRES_PASSWORD" ]; then
    warn "POSTGRES_PASSWORD differs from the saved one. Postgres applies it only when its data volume is first created;"
    warn "for an existing database also run: docker compose exec postgres psql -U ${POSTGRES_USER} -d picasso_database -c \"ALTER USER ${POSTGRES_USER} PASSWORD '<new>';\""
fi

umask 077
cat > .env <<EOF
POSTGRES_USER=${POSTGRES_USER}
POSTGRES_PASSWORD=${POSTGRES_PASSWORD}
PICASSO_DOMAIN=${PICASSO_DOMAIN}
PICASSO_PUBLIC_URL=${PICASSO_PUBLIC_URL}
PICASSO_BEHIND_CLOUDFLARE=${PICASSO_BEHIND_CLOUDFLARE}
EOF
umask 022
log "Wrote $INSTALL_DIR/.env (PICASSO_DOMAIN=${PICASSO_DOMAIN}, PICASSO_BEHIND_CLOUDFLARE=${PICASSO_BEHIND_CLOUDFLARE})"

# --- Caddy: trust Cloudflare as a proxy only when it really is one ---
mkdir -p config/generated
rm -f config/generated/*.caddy
if [ "$PICASSO_BEHIND_CLOUDFLARE" = "1" ]; then
    CF_RANGES="$( { curl -fsS --max-time 15 https://www.cloudflare.com/ips-v4; echo; \
                    curl -fsS --max-time 15 https://www.cloudflare.com/ips-v6; } \
                  | grep -E '^[0-9a-fA-F:.]+/[0-9]+$' | tr '\n' ' ' )" || true
    [ -n "$CF_RANGES" ] || die "could not download Cloudflare IP ranges; refusing to trust CF-Connecting-IP blindly."
    cat > config/generated/trusted_proxies.caddy <<EOF
trusted_proxies static ${CF_RANGES}
client_ip_headers CF-Connecting-IP
EOF
    log "Caddy will trust CF-Connecting-IP from Cloudflare ranges only"
fi

# --- firewall ---
# Docker-published ports bypass ufw, so the DOCKER-USER rules below are what actually restrict 80/443.
# ufw is only kept consistent for the host's own services (SSH).
if command -v ufw >/dev/null 2>&1 && ufw status | grep -q "Status: active"; then
    log "ufw is active, keeping ${SSH_PORT}/tcp open"
    ufw allow "${SSH_PORT}/tcp" >/dev/null
    if [ "$PICASSO_BEHIND_CLOUDFLARE" != "1" ]; then
        log "ufw: opening 80/tcp and 443/tcp"
        ufw allow 80/tcp >/dev/null
        ufw allow 443/tcp >/dev/null
    fi
fi

if [ "$SKIP_FIREWALL" = "1" ]; then
    warn "PICASSO_SKIP_FIREWALL=1 - DOCKER-USER rules not installed; published ports are open to the world."
else
    if [ "$PICASSO_BEHIND_CLOUDFLARE" = "1" ]; then FW_MODE="from Cloudflare only"; else FW_MODE="open to all"; fi
    log "Installing DOCKER-USER rules (80/443 ${FW_MODE})"
    chmod +x deploy/firewall/picasso-firewall.sh
    PICASSO_INSTALL_DIR="$INSTALL_DIR" deploy/firewall/picasso-firewall.sh apply
    if command -v systemctl >/dev/null 2>&1; then
        for unit in picasso-firewall.service picasso-firewall.timer; do
            sed "s|@INSTALL_DIR@|${INSTALL_DIR}|g" "deploy/firewall/${unit}" > "/etc/systemd/system/${unit}"
        done
        systemctl daemon-reload
        systemctl enable picasso-firewall.service >/dev/null 2>&1
        systemctl enable --now picasso-firewall.timer >/dev/null 2>&1
    else
        warn "systemd not found - rules will not survive a reboot; re-run deploy/firewall/picasso-firewall.sh apply."
    fi
fi

log "Building and starting (compiles the C++ server from source - a few minutes on first run)"
docker compose up -d --build --remove-orphans

log "Waiting for the stack to answer"
UP=0
# shellcheck disable=SC2034
for i in $(seq 1 30); do
    if curl -fsS -o /dev/null "http://127.0.0.1/ping"; then
        UP=1
        break
    fi
    sleep 1
done

if [ "$UP" -eq 1 ]; then
    log "Up. curl http://127.0.0.1/ping -> 200"
else
    warn "Not answering yet after 30s - check: docker compose -f $INSTALL_DIR/docker-compose.yaml logs -f"
fi
