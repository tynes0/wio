#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
INSTALL_ROOT="${WIO_INSTALL_ROOT:-$SCRIPT_DIR}"
KEEP_FILES=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-root)
            INSTALL_ROOT="$2"
            shift 2
            ;;
        --keep-files)
            KEEP_FILES=1
            shift
            ;;
        *)
            echo "Unknown uninstall-wio.sh argument: $1" >&2
            exit 1
            ;;
    esac
done

WIO_EXE="$INSTALL_ROOT/bin/wio"
if [ -x "$WIO_EXE" ]; then
    "$WIO_EXE" env remove --wio-root "$INSTALL_ROOT" --set-user --remove-path --no-prompt || true
fi
if [ "$KEEP_FILES" -eq 1 ]; then
    echo "Wio environment entries were removed. Files were kept at '$INSTALL_ROOT'."
    exit 0
fi

rm -rf "$INSTALL_ROOT"
echo "Wio uninstalled from '$INSTALL_ROOT'."
