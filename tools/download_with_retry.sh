#!/bin/sh

set -u

if [ "$#" -ne 2 ]; then
    printf 'Usage: %s URL OUTPUT\n' "$0" >&2
    exit 2
fi

url=$1
output=$2
retries=${DOWNLOAD_RETRIES:-5}
delay=${DOWNLOAD_RETRY_DELAY:-2}
connect_timeout=${DOWNLOAD_CONNECT_TIMEOUT:-15}
max_time=${DOWNLOAD_MAX_TIME:-60}
max_attempts=$((retries + 1))
attempt=1
tmp="${output}.part.$$"

cleanup() {
    rm -f "$tmp"
}
trap cleanup EXIT HUP INT TERM

while [ "$attempt" -le "$max_attempts" ]; do
    rm -f "$tmp"

    if command -v curl >/dev/null 2>&1; then
        curl -fsSL \
            --connect-timeout "$connect_timeout" \
            --max-time "$max_time" \
            --output "$tmp" \
            "$url"
        status=$?
    elif command -v wget >/dev/null 2>&1; then
        wget -q \
            --timeout="$max_time" \
            --tries=1 \
            --output-document="$tmp" \
            "$url"
        status=$?
    else
        printf 'Neither curl nor wget is available\n' >&2
        exit 127
    fi

    if [ "$status" -eq 0 ] && [ -s "$tmp" ]; then
        mv -f "$tmp" "$output"
        trap - EXIT HUP INT TERM
        exit 0
    fi

    rm -f "$tmp"
    if [ "$attempt" -lt "$max_attempts" ]; then
        printf 'Download failed (attempt %d/%d), retrying in %ds: %s\n' \
            "$attempt" "$max_attempts" "$delay" "$url" >&2
        sleep "$delay"
        delay=$((delay * 2))
        if [ "$delay" -gt 30 ]; then delay=30; fi
    fi
    attempt=$((attempt + 1))
done

printf 'Download failed after %d attempts: %s\n' "$max_attempts" "$url" >&2
exit 1
