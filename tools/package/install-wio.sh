#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_WIO_EXE="$SCRIPT_DIR/bin/wio"
INSTALL_ROOT="${WIO_INSTALL_ROOT:-$HOME/.local/share/wio}"
SKIP_PATH=0
SKIP_ENVIRONMENT=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-root)
            INSTALL_ROOT="$2"
            shift 2
            ;;
        --skip-path)
            SKIP_PATH=1
            shift
            ;;
        --skip-environment-setup)
            SKIP_ENVIRONMENT=1
            shift
            ;;
        *)
            echo "Unknown install-wio.sh argument: $1" >&2
            exit 1
            ;;
    esac
done

if [ ! -x "$PACKAGE_WIO_EXE" ]; then
    echo "The packaged wio executable was not found under '$SCRIPT_DIR/bin'." >&2
    exit 1
fi

if [ "$SCRIPT_DIR" != "$INSTALL_ROOT" ]; then
    mkdir -p "$(dirname "$INSTALL_ROOT")"
    rm -rf "$INSTALL_ROOT"
    mkdir -p "$INSTALL_ROOT"
    for entry in "$SCRIPT_DIR"/* "$SCRIPT_DIR"/.[!.]* "$SCRIPT_DIR"/..?*; do
        [ -e "$entry" ] || continue
        cp -R "$entry" "$INSTALL_ROOT"/
    done
fi

INSTALLED_WIO_EXE="$INSTALL_ROOT/bin/wio"
if [ ! -x "$INSTALLED_WIO_EXE" ]; then
    echo "The installed wio executable was not found under '$INSTALL_ROOT/bin'." >&2
    exit 1
fi

if [ "$SKIP_ENVIRONMENT" -eq 1 ]; then
    echo "Wio installed to '$INSTALL_ROOT'."
    exit 0
fi
if [ "$SKIP_PATH" -eq 1 ]; then
    exec "$INSTALLED_WIO_EXE" env setup --wio-root "$INSTALL_ROOT" --set-user --no-prompt
fi
exec "$INSTALLED_WIO_EXE" env setup --wio-root "$INSTALL_ROOT" --set-user --no-prompt --add-path
