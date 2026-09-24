#!/usr/bin/env bash
#
# PickUsAllBackend installer - bootstraps the self-hosted Free Tier stack
# (backend + Caddy, via docker compose) on a fresh Ubuntu/Debian VPS.
#
#   curl -fsSL https://raw.githubusercontent.com/raaveinm/picassobackend/main/install.sh | bash
#   curl -fsSL .../install.sh | PICASSO_DOMAIN=chat.example.com bash   # with a real domain -> automatic HTTPS
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
apt-get install -y -qq git ca-certificates curl >/dev/null

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

cat > .env <<EOF
PICASSO_DOMAIN=${PICASSO_DOMAIN}
PICASSO_PUBLIC_URL=${PICASSO_PUBLIC_URL}
EOF
log "Wrote $INSTALL_DIR/.env (PICASSO_DOMAIN=${PICASSO_DOMAIN})"

if command -v ufw >/dev/null 2>&1 && ufw status | grep -q "Status: active"; then
    log "ufw is active, opening 80/tcp and 443/tcp"
    ufw allow 80/tcp >/dev/null
    ufw allow 443/tcp >/dev/null
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
