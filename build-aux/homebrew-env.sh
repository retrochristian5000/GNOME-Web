#!/bin/sh
# Run a native build command with optional Homebrew search paths.
set -eu

if [ "$#" -eq 0 ]; then
    printf 'Usage: sh %s COMMAND [ARG ...]\n' "$0" >&2
    exit 2
fi

if [ "$(uname -s)" != Darwin ]; then
    exec "$@"
fi

# Never use native Homebrew paths for cross configuration.
for arg in "$@"; do
    case "$arg" in
        --cross-file|--cross-file=*)
            printf '%s\n' 'Use a Meson cross file without the Homebrew helper.' >&2
            exit 2
            ;;
    esac
done

if command -v brew >/dev/null 2>&1; then
    brew=$(command -v brew)
else
    case "$(uname -m)" in
        arm64) brew=/opt/homebrew/bin/brew ;;
        x86_64) brew=/usr/local/bin/brew ;;
        *) exec "$@" ;;
    esac
    [ -x "$brew" ] || exec "$@"
fi

prefix=$(HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_ANALYTICS=1 "$brew" --prefix)
case "$prefix" in
    /*) [ -d "$prefix" ] || exit 1 ;;
    *) printf '%s\n' 'Invalid Homebrew prefix.' >&2; exit 1 ;;
esac

# Append existing directories only, preserving caller precedence.
append_dir() {
    case ":$1:" in
        *":$2:"*) printf '%s' "$1" ;;
        *) if [ -d "$2" ]; then
               printf '%s%s%s' "$1" "${1:+:}" "$2"
           else
               printf '%s' "$1"
           fi ;;
    esac
}

PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}
PATH=$(append_dir "$PATH" "$prefix/bin")
PKG_CONFIG_PATH=$(append_dir "$PKG_CONFIG_PATH" "$prefix/lib/pkgconfig")
PKG_CONFIG_PATH=$(append_dir "$PKG_CONFIG_PATH" "$prefix/share/pkgconfig")

# Include relevant unlinked (keg-only) formulae without scanning every keg.
for formula in appstream blueprint-compiler cairo desktop-file-utils docutils \
    gettext glib glib-networking gmp gobject-introspection gsettings-desktop-schemas \
    gstreamer gtk4 gdk-pixbuf gcr iso-codes itstool json-glib libadwaita \
    libarchive libportal libproxy libsecret libsoup libxml2 libpwquality nettle php \
    pkgconf sqlite webkitgtk yelp-tools; do
    root=$prefix/opt/$formula
    PATH=$(append_dir "$PATH" "$root/bin")
    PKG_CONFIG_PATH=$(append_dir "$PKG_CONFIG_PATH" "$root/lib/pkgconfig")
    PKG_CONFIG_PATH=$(append_dir "$PKG_CONFIG_PATH" "$root/share/pkgconfig")
done

export PATH PKG_CONFIG_PATH
exec "$@"
