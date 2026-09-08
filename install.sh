#!/usr/bin/env bash
# Install BuildnBits Usage for the current user (no system-wide sudo except packages).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
JOBS="$(nproc 2>/dev/null || echo 4)"

usage() {
    cat <<EOF
Install BuildnBits Usage into ${PREFIX}

Usage: ./install.sh [--start] [--uninstall] [--system]

  --start      Launch the tray app after install
  --uninstall  Remove files this script installed
  --system     Install to /usr (needs sudo cmake --install)
  --help       Show this help

Override prefix: PREFIX=/usr/local ./install.sh
EOF
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1
}

install_packages() {
    if [[ -f /etc/os-release ]]; then
        # shellcheck disable=SC1091
        . /etc/os-release
    fi
    case "${ID:-}" in
        arch | archlinux | manjaro | endeavour | cachyos | garuda)
            echo "Installing build dependencies with pacman..."
            sudo pacman -S --needed --noconfirm \
                cmake extra-cmake-modules qt6-base \
                kstatusnotifieritem kcoreaddons kconfig ki18n kwindowsystem \
                layer-shell-qt
            ;;
        fedora | rhel | centos | nobara)
            echo "Installing build dependencies with dnf..."
            sudo dnf install -y cmake extra-cmake-modules qt6-qtbase-devel \
                kf6-kstatusnotifieritem-devel kf6-kcoreaddons-devel \
                kf6-kconfig-devel kf6-ki18n-devel kf6-kwindowsystem-devel \
                layer-shell-qt-devel gcc-c++
            ;;
        debian | ubuntu | linuxmint | pop)
            echo "Installing build dependencies with apt..."
            sudo apt-get update
            sudo apt-get install -y cmake extra-cmake-modules g++ \
                qt6-base-dev libkf6statusnotifieritem-dev libkf6coreaddons-dev \
                libkf6config-dev libkf6i18n-dev libkf6windowsystem-dev \
                liblayershellqtinterface-dev
            ;;
        *)
            if ! need_cmd cmake; then
                echo "Unknown distro (${ID:-}). Install these, then re-run:" >&2
                echo "  cmake, extra-cmake-modules, Qt 6, KDE Frameworks 6" >&2
                echo "  (StatusNotifierItem, CoreAddons, Config, I18n, WindowSystem), LayerShellQt" >&2
                exit 1
            fi
            echo "cmake found; skipping package install for ${ID:-unknown}."
            ;;
    esac
}

do_uninstall() {
    local prefix="$1"
    echo "Removing BuildnBits Usage from ${prefix}..."
    rm -f "${prefix}/bin/buildnbits-usage"
    rm -f "${prefix}/share/applications/buildnbits-usage.desktop"
    rm -f "${prefix}/share/icons/hicolor/512x512/apps/buildnbits-usage.png"
    update-desktop-database "${prefix}/share/applications" 2>/dev/null || true
    echo "Done. Cache at ~/.local/share/BuildnBits/Usage was left in place."
}

START=0
UNINSTALL=0
SYSTEM=0
for arg in "$@"; do
    case "$arg" in
        --start) START=1 ;;
        --uninstall) UNINSTALL=1 ;;
        --system) SYSTEM=1 ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ "$SYSTEM" -eq 1 ]]; then
    PREFIX=/usr
fi

if [[ "$UNINSTALL" -eq 1 ]]; then
    do_uninstall "$PREFIX"
    exit 0
fi

install_packages

if ! need_cmd cmake; then
    echo "cmake is required." >&2
    exit 1
fi

echo "Building into ${BUILD_DIR} (prefix ${PREFIX})..."
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "Installing..."
if [[ "$SYSTEM" -eq 1 ]]; then
    sudo cmake --install "$BUILD_DIR"
else
    cmake --install "$BUILD_DIR"
fi

update-desktop-database "${PREFIX}/share/applications" 2>/dev/null || true
gtk-update-icon-cache -f -t "${PREFIX}/share/icons/hicolor" 2>/dev/null || true

BIN="${PREFIX}/bin/buildnbits-usage"
echo
echo "Installed: ${BIN}"
echo "Launcher:  BuildnBits Usage (application menu)"
if [[ ":$PATH:" != *":${PREFIX}/bin:"* ]]; then
    echo
    echo "Add this to ~/.bashrc or ~/.profile so the command is on PATH:"
    echo "  export PATH=\"${PREFIX}/bin:\$PATH\""
fi
echo
echo "Start it from the menu, or run:"
echo "  ${BIN}"
echo "Turn on “Run at Startup” from a tray square → Settings."

if [[ "$START" -eq 1 ]]; then
    if pgrep -x buildnbits-usage >/dev/null 2>&1 || pgrep -f "${BIN}" >/dev/null 2>&1; then
        echo "Already running."
    else
        nohup "$BIN" >/dev/null 2>&1 &
        echo "Started."
    fi
fi
