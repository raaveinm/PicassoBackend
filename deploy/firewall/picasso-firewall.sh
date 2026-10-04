#!/usr/bin/env bash
#
# Restricts which sources may open NEW connections to Docker-published ports, via the DOCKER-USER chain.
#
#   picasso-firewall.sh apply    # (re)build the rules; safe to run repeatedly (the timer does, daily)
#   picasso-firewall.sh status   # show the chain and the hook
#   picasso-firewall.sh remove   # unhook and delete the chain
#
# Mode comes from PICASSO_BEHIND_CLOUDFLARE (environment, else $INSTALL_DIR/.env):
#   1  ->  80/443 reachable from Cloudflare ranges only (a leaked origin IP is useless to an attacker)
#   0  ->  80/443 reachable from anywhere (no domain, or a domain without the Cloudflare proxy)
# Either way every other published port is dropped from the outside.

set -euo pipefail

###############################################
### Configuration
###############################################

INSTALL_DIR="${PICASSO_INSTALL_DIR:-/opt/picassobackend}"
ENV_FILE="${INSTALL_DIR}/.env"
STATE_DIR="/var/lib/picasso"
CHAIN="PICASSO-INGRESS"
TMP_CHAIN="PICASSO-INGRESS-NEW"
WEB_PORTS=(80 443)
CF_IPS_URL="https://www.cloudflare.com"

log() { printf '[picasso-firewall] %s\n' "$*"; }
die() { printf '[picasso-firewall] ERROR %s\n' "$*" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] || die "run as root."

read_mode() {
    local value="${PICASSO_BEHIND_CLOUDFLARE:-}"
    if [ -z "$value" ] && [ -f "$ENV_FILE" ]; then
        value="$(grep -E '^PICASSO_BEHIND_CLOUDFLARE=' "$ENV_FILE" | tail -n1 | cut -d= -f2- || true)"
    fi
    [ "$value" = "1" ] && echo 1 || echo 0
}

external_interface() {
    if [ -n "${PICASSO_EXT_IF:-}" ]; then
        echo "$PICASSO_EXT_IF"
        return
    fi
    ip -o route get 1.1.1.1 | sed -n 's/.* dev \([^ ]*\).*/\1/p' | head -n1
}

###############################################
### Cloudflare ranges
###############################################

# Downloads one family's ranges (family = "v4" | "v6") into the cache and prints the cache path.
cloudflare_ranges() {
    local family="$1"
    local cache="${STATE_DIR}/cloudflare-ips-${family}.txt"
    local fresh
    fresh="$(mktemp)"
    mkdir -p "$STATE_DIR"

    if curl -fsS --max-time 15 "${CF_IPS_URL}/ips-${family}" -o "$fresh" \
        && [ "$(grep -cE '^[0-9a-fA-F:.]+/[0-9]+$' "$fresh")" -ge 5 ]; then
        grep -E '^[0-9a-fA-F:.]+/[0-9]+$' "$fresh" > "$cache"
    else
        log "could not fetch ips-${family}; falling back to cached list"
    fi
    rm -f "$fresh"

    [ -s "$cache" ] || die "no Cloudflare ${family} ranges available (fetch failed, no cache)"
    echo "$cache"
}

###############################################
### Rules
###############################################

# Builds the chain for one IP family. `fw` is iptables or ip6tables; `ranges` is a file of CIDRs or "" for
# "any source". The new chain is filled and swapped in before the old one is removed, so there is never a
# moment where the hook points at an empty chain (an empty chain would RETURN, i.e. accept everything).
apply_family() {
    local fw="$1" ranges="$2" ext_if="$3"

    "$fw" -n -L DOCKER-USER >/dev/null 2>&1 || {
        log "$fw has no DOCKER-USER chain (Docker not running for this family); skipping"
        return 0
    }

    "$fw" -F "$TMP_CHAIN" 2>/dev/null && "$fw" -X "$TMP_CHAIN" 2>/dev/null || true
    "$fw" -N "$TMP_CHAIN"

    local port cidr
    for port in "${WEB_PORTS[@]}"; do
        if [ -z "$ranges" ]; then
            "$fw" -A "$TMP_CHAIN" -p tcp -m conntrack --ctorigdstport "$port" -j RETURN
        else
            while read -r cidr; do
                "$fw" -A "$TMP_CHAIN" -s "$cidr" -p tcp -m conntrack --ctorigdstport "$port" -j RETURN
            done < "$ranges"
        fi
    done
    "$fw" -A "$TMP_CHAIN" -j DROP

    "$fw" -I DOCKER-USER 1 -i "$ext_if" -m conntrack --ctstate NEW -j "$TMP_CHAIN"
    if "$fw" -n -L "$CHAIN" >/dev/null 2>&1; then
        while "$fw" -D DOCKER-USER -i "$ext_if" -m conntrack --ctstate NEW -j "$CHAIN" 2>/dev/null; do :; done
        "$fw" -F "$CHAIN"
        "$fw" -X "$CHAIN"
    fi
    "$fw" -E "$TMP_CHAIN" "$CHAIN"
}

remove_family() {
    local fw="$1" ext_if="$2"
    "$fw" -n -L DOCKER-USER >/dev/null 2>&1 || return 0
    while "$fw" -D DOCKER-USER -i "$ext_if" -m conntrack --ctstate NEW -j "$CHAIN" 2>/dev/null; do :; done
    if "$fw" -n -L "$CHAIN" >/dev/null 2>&1; then
        "$fw" -F "$CHAIN"
        "$fw" -X "$CHAIN"
    fi
}

###############################################
### Commands
###############################################

cmd_apply() {
    command -v iptables >/dev/null 2>&1 || die "iptables not found."
    command -v curl >/dev/null 2>&1 || die "curl not found."

    local ext_if mode v4 v6
    ext_if="$(external_interface)"
    [ -n "$ext_if" ] || die "could not determine the external interface; set PICASSO_EXT_IF."
    mode="$(read_mode)"

    v4="" v6=""
    if [ "$mode" = "1" ]; then
        v4="$(cloudflare_ranges v4)"
        v6="$(cloudflare_ranges v6)"
        log "mode: Cloudflare only (interface ${ext_if})"
    else
        log "mode: open web ports (interface ${ext_if})"
    fi

    apply_family iptables "$v4" "$ext_if"
    if command -v ip6tables >/dev/null 2>&1; then
        apply_family ip6tables "$v6" "$ext_if"
    fi
    log "applied"
}

cmd_status() {
    local fw
    for fw in iptables ip6tables; do
        command -v "$fw" >/dev/null 2>&1 || continue
        echo "=== $fw DOCKER-USER ==="
        "$fw" -n -L DOCKER-USER --line-numbers 2>&1 || true
        echo "=== $fw $CHAIN ==="
        "$fw" -n -L "$CHAIN" --line-numbers 2>&1 || true
    done
}

cmd_remove() {
    local ext_if
    ext_if="$(external_interface)"
    remove_family iptables "$ext_if"
    if command -v ip6tables >/dev/null 2>&1; then
        remove_family ip6tables "$ext_if"
    fi
    log "removed"
}

case "${1:-apply}" in
    apply)  cmd_apply ;;
    status) cmd_status ;;
    remove) cmd_remove ;;
    *) die "usage: $0 [apply|status|remove]" ;;
esac
