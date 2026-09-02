#!/usr/bin/env bash
#
# build.sh -- build vclock on Linux.
#
# Configures and builds the CMake project, locating Qt automatically. Qt is
# often installed somewhere CMake does not search by default (the official
# installer drops it in ~/Qt/<version>/gcc_64), so the prefix is worked out
# here and handed to CMake rather than relying on the default search path.
#
#   ./build.sh                  release build into ./build
#   ./build.sh --type Debug     debug build
#   ./build.sh --clean          discard the build directory first
#   ./build.sh --install-deps   install the required system packages
#   ./build.sh --help           full option list
#
set -euo pipefail

SOURCE_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

BUILD_TYPE=Release
BUILD_DIR=
JOBS=$(nproc 2>/dev/null || echo 4)
QT_PREFIX=${QT_PREFIX:-}
CLEAN=0
INSTALL_DEPS=0
DO_INSTALL=0
INSTALL_PREFIX=
RUN_AFTER=0
VERBOSE=0

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_RESET=$'\033[0m'; C_BOLD=$'\033[1m'
    C_RED=$'\033[31m'; C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'; C_BLUE=$'\033[34m'
else
    C_RESET=; C_BOLD=; C_RED=; C_GREEN=; C_YELLOW=; C_BLUE=
fi

info()  { printf '%s==>%s %s\n' "$C_BLUE$C_BOLD" "$C_RESET" "$*"; }
warn()  { printf '%swarning:%s %s\n' "$C_YELLOW$C_BOLD" "$C_RESET" "$*" >&2; }
die()   { printf '%serror:%s %s\n' "$C_RED$C_BOLD" "$C_RESET" "$*" >&2; exit 1; }
ok()    { printf '%s%s%s\n' "$C_GREEN" "$*" "$C_RESET"; }

usage() {
    cat <<EOF
${C_BOLD}Usage:${C_RESET} ./build.sh [options]

Build the vclock Qt application on Linux.

${C_BOLD}Options:${C_RESET}
  -t, --type TYPE       Build type: Release, Debug, RelWithDebInfo, MinSizeRel
                        (default: Release)
  -b, --build-dir DIR   Build directory (default: build, or build-<type>
                        for non-Release builds)
  -j, --jobs N          Parallel compile jobs (default: $JOBS)
  -c, --clean           Delete the build directory before configuring
  -q, --qt-dir PATH     Qt prefix, e.g. ~/Qt/6.5.3/gcc_64. Overrides
                        autodetection. May also be set via \$QT_PREFIX.
      --install-deps    Install the required build packages with the system
                        package manager, then continue
      --install         Run 'cmake --install' after a successful build
      --prefix PATH     Install prefix for --install (default: /usr/local)
  -r, --run             Launch vclock after a successful build
  -v, --verbose         Show the full compiler command lines
  -h, --help            This message

${C_BOLD}Examples:${C_RESET}
  ./build.sh                                  # release build
  ./build.sh -t Debug -j 4                    # debug build, 4 jobs
  ./build.sh --clean --qt-dir ~/Qt/6.7.2/gcc_64
  ./build.sh --install --prefix ~/.local      # install into ~/.local/bin
EOF
}

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case $1 in
        -t|--type)       [ $# -ge 2 ] || die "$1 requires an argument"; BUILD_TYPE=$2; shift 2 ;;
        -b|--build-dir)  [ $# -ge 2 ] || die "$1 requires an argument"; BUILD_DIR=$2; shift 2 ;;
        -j|--jobs)       [ $# -ge 2 ] || die "$1 requires an argument"; JOBS=$2; shift 2 ;;
        -q|--qt-dir)     [ $# -ge 2 ] || die "$1 requires an argument"; QT_PREFIX=$2; shift 2 ;;
        --prefix)        [ $# -ge 2 ] || die "$1 requires an argument"; INSTALL_PREFIX=$2; shift 2 ;;
        -c|--clean)      CLEAN=1; shift ;;
        --install-deps)  INSTALL_DEPS=1; shift ;;
        --install)       DO_INSTALL=1; shift ;;
        -r|--run)        RUN_AFTER=1; shift ;;
        -v|--verbose)    VERBOSE=1; shift ;;
        -h|--help)       usage; exit 0 ;;
        *)               printf 'error: unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

case $BUILD_TYPE in
    Release|Debug|RelWithDebInfo|MinSizeRel) ;;
    *) die "invalid build type '$BUILD_TYPE' (expected Release, Debug, RelWithDebInfo or MinSizeRel)" ;;
esac

case $JOBS in
    ''|*[!0-9]*) die "--jobs must be a positive integer, got '$JOBS'" ;;
    0) die "--jobs must be at least 1" ;;
esac

# Keep build types in separate trees so switching between them does not force a
# full reconfigure of the other.
if [ -z "$BUILD_DIR" ]; then
    if [ "$BUILD_TYPE" = Release ]; then
        BUILD_DIR=$SOURCE_DIR/build
    else
        BUILD_DIR=$SOURCE_DIR/build-$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')
    fi
fi
case $BUILD_DIR in /*) ;; *) BUILD_DIR=$PWD/$BUILD_DIR ;; esac

[ -f "$SOURCE_DIR/CMakeLists.txt" ] || die "CMakeLists.txt not found in $SOURCE_DIR"

# ---------------------------------------------------------------------------
# Distro-specific package names
# ---------------------------------------------------------------------------
distro_id() {
    # shellcheck disable=SC1091
    [ -r /etc/os-release ] && . /etc/os-release && echo "${ID:-unknown} ${ID_LIKE:-}"
}

# Sets PKG_INSTALL_CMD to the command that installs a toolchain plus Qt 6
# Widgets and Svg development files.
detect_package_command() {
    local ids; ids=$(distro_id)
    case " $ids " in
        *" debian "*|*" ubuntu "*|*" linuxmint "*|*" pop "*)
            PKG_INSTALL_CMD="apt-get install -y build-essential cmake qt6-base-dev qt6-svg-dev" ;;
        *" fedora "*|*" rhel "*|*" centos "*)
            PKG_INSTALL_CMD="dnf install -y gcc-c++ cmake qt6-qtbase-devel qt6-qtsvg-devel" ;;
        *" arch "*|*" manjaro "*|*" endeavouros "*)
            PKG_INSTALL_CMD="pacman -S --needed --noconfirm base-devel cmake qt6-base qt6-svg" ;;
        *" opensuse"*|*" suse "*)
            PKG_INSTALL_CMD="zypper install -y gcc-c++ cmake qt6-base-devel qt6-svg-devel" ;;
        *" alpine "*)
            PKG_INSTALL_CMD="apk add build-base cmake qt6-qtbase-dev qt6-qtsvg-dev" ;;
        *)
            PKG_INSTALL_CMD="" ;;
    esac
}

as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        die "root privileges are required and sudo is not available; run as root: $*"
    fi
}

install_dependencies() {
    detect_package_command
    [ -n "$PKG_INSTALL_CMD" ] || die "unrecognised distribution; install CMake, a C++17 compiler and the Qt 6 Widgets + Svg development packages manually"
    info "Installing build dependencies: $PKG_INSTALL_CMD"
    # shellcheck disable=SC2086
    as_root $PKG_INSTALL_CMD
}

missing_deps_hint() {
    detect_package_command
    if [ -n "$PKG_INSTALL_CMD" ]; then
        printf '\n  Install them with:\n    ./build.sh --install-deps\n  or manually:\n    sudo %s\n\n' "$PKG_INSTALL_CMD" >&2
    else
        printf '\n  Install CMake, a C++17 compiler, and the Qt 6 Widgets + Svg\n  development packages using your distribution'"'"'s package manager.\n\n' >&2
    fi
}

# ---------------------------------------------------------------------------
# Qt discovery
#
# A prefix qualifies only if BOTH the Widgets and Svg CMake packages are
# present. Svg ships separately on most distributions (qt6-svg-dev), and it is
# the component most often missing, so checking for Widgets alone would let the
# build fail later with a much less obvious message.
# ---------------------------------------------------------------------------
QT_MAJOR=

qt_prefix_has_modules() {
    local prefix=$1 libdir
    for libdir in lib lib64 lib/x86_64-linux-gnu lib/aarch64-linux-gnu; do
        if [ -d "$prefix/$libdir/cmake/Qt6Widgets" ] && [ -d "$prefix/$libdir/cmake/Qt6Svg" ]; then
            QT_MAJOR=6; return 0
        fi
    done
    for libdir in lib lib64 lib/x86_64-linux-gnu lib/aarch64-linux-gnu; do
        if [ -d "$prefix/$libdir/cmake/Qt5Widgets" ] && [ -d "$prefix/$libdir/cmake/Qt5Svg" ]; then
            QT_MAJOR=5; return 0
        fi
    done
    return 1
}

find_qt_prefix() {
    local candidate qmake

    # 1. Explicit override always wins, and a bad one is an error rather than a
    #    silent fallback to some other Qt.
    if [ -n "$QT_PREFIX" ]; then
        QT_PREFIX=${QT_PREFIX/#\~/$HOME}
        [ -d "$QT_PREFIX" ] || die "Qt prefix does not exist: $QT_PREFIX"
        qt_prefix_has_modules "$QT_PREFIX" \
            || die "no Qt Widgets + Svg CMake packages under: $QT_PREFIX"
        return 0
    fi

    # 2. A qmake on PATH knows exactly where its own Qt lives.
    for qmake in qmake6 qmake-qt6 qmake; do
        if command -v "$qmake" >/dev/null 2>&1; then
            candidate=$("$qmake" -query QT_INSTALL_PREFIX 2>/dev/null || true)
            if [ -n "$candidate" ] && qt_prefix_has_modules "$candidate"; then
                QT_PREFIX=$candidate; return 0
            fi
        fi
    done

    # 3. Distribution packages.
    for candidate in /usr /usr/local; do
        if qt_prefix_has_modules "$candidate"; then
            QT_PREFIX=$candidate; return 0
        fi
    done

    # 4. Official Qt installer layouts, newest version first.
    local dir
    while IFS= read -r dir; do
        [ -n "$dir" ] || continue
        if qt_prefix_has_modules "$dir"; then
            QT_PREFIX=$dir; return 0
        fi
    done < <(ls -d "$HOME"/Qt/*/gcc_64 "$HOME"/Qt/*/gcc_arm64 \
                   /opt/Qt/*/gcc_64 /opt/Qt*/*/gcc_64 2>/dev/null | sort -Vr)

    return 1
}

# ---------------------------------------------------------------------------
# Stale build directory detection
#
# A CMake cache pins absolute paths to the compiler, the source tree and Qt. If
# any of them has since moved or been removed, reconfiguring produces confusing
# errors or, worse, links against a Qt that is no longer there. Reconfiguring
# from scratch is cheap, so anything inconsistent is simply discarded.
# ---------------------------------------------------------------------------
cache_value() {
    local key=$1 file=$2
    sed -n "s/^${key}:[^=]*=//p" "$file" 2>/dev/null | head -n1
}

build_dir_is_stale() {
    local cache=$BUILD_DIR/CMakeCache.txt
    [ -f "$cache" ] || return 1

    local home; home=$(cache_value CMAKE_HOME_DIRECTORY "$cache")
    if [ -n "$home" ] && [ "$home" != "$SOURCE_DIR" ]; then
        STALE_REASON="it was configured for a different source directory ($home)"
        return 0
    fi

    local cxx; cxx=$(cache_value CMAKE_CXX_COMPILER "$cache")
    if [ -n "$cxx" ] && [ ! -x "$cxx" ]; then
        STALE_REASON="its C++ compiler no longer exists ($cxx)"
        return 0
    fi

    local key qtdir
    for key in Qt6_DIR Qt5_DIR QT_DIR; do
        qtdir=$(cache_value "$key" "$cache")
        case $qtdir in
            ''|*NOTFOUND) continue ;;
        esac
        if [ ! -d "$qtdir" ]; then
            STALE_REASON="its Qt installation no longer exists ($qtdir)"
            return 0
        fi
    done

    local gen; gen=$(cache_value CMAKE_GENERATOR "$cache")
    if [ -n "$gen" ] && [ "$gen" != "$GENERATOR" ]; then
        STALE_REASON="it uses a different generator ($gen, wanted $GENERATOR)"
        return 0
    fi

    return 1
}

# ---------------------------------------------------------------------------
# Preflight
# ---------------------------------------------------------------------------
[ "$INSTALL_DEPS" -eq 1 ] && install_dependencies

command -v cmake >/dev/null 2>&1 || {
    printf '%serror:%s cmake was not found on PATH.\n' "$C_RED$C_BOLD" "$C_RESET" >&2
    missing_deps_hint
    exit 1
}

if ! command -v c++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    printf '%serror:%s no C++ compiler was found on PATH.\n' "$C_RED$C_BOLD" "$C_RESET" >&2
    missing_deps_hint
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
if [ "$(printf '3.16\n%s\n' "$CMAKE_VERSION" | sort -V | head -n1)" != "3.16" ]; then
    die "CMake 3.16 or newer is required, found $CMAKE_VERSION"
fi

if ! find_qt_prefix; then
    printf '%serror:%s could not find a Qt installation with both the Widgets and Svg modules.\n' \
        "$C_RED$C_BOLD" "$C_RESET" >&2
    printf '  Searched: qmake on PATH, /usr, /usr/local, ~/Qt/*/gcc_64, /opt/Qt/*/gcc_64\n' >&2
    missing_deps_hint
    printf '  If Qt is installed elsewhere, point at it explicitly:\n    ./build.sh --qt-dir /path/to/Qt/6.5.3/gcc_64\n\n' >&2
    exit 1
fi

if command -v ninja >/dev/null 2>&1; then
    GENERATOR=Ninja
else
    GENERATOR="Unix Makefiles"
fi

# ---------------------------------------------------------------------------
# Configure
# ---------------------------------------------------------------------------
info "vclock build"
printf '    source      %s\n' "$SOURCE_DIR"
printf '    build dir   %s\n' "$BUILD_DIR"
printf '    build type  %s\n' "$BUILD_TYPE"
printf '    generator   %s\n' "$GENERATOR"
printf '    qt%s prefix  %s\n' "$QT_MAJOR" "$QT_PREFIX"
printf '    jobs        %s\n' "$JOBS"

if [ "$CLEAN" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    info "Cleaning $BUILD_DIR"
    rm -rf -- "$BUILD_DIR"
fi

STALE_REASON=
if build_dir_is_stale; then
    warn "discarding $BUILD_DIR: $STALE_REASON"
    rm -rf -- "$BUILD_DIR"
fi

CMAKE_ARGS=(
    -S "$SOURCE_DIR"
    -B "$BUILD_DIR"
    -G "$GENERATOR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)
# /usr is already searched; passing it would only add noise.
[ "$QT_PREFIX" != /usr ] && CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$QT_PREFIX")
[ -n "$INSTALL_PREFIX" ] && CMAKE_ARGS+=(-DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX")

info "Configuring"
cmake "${CMAKE_ARGS[@]}"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
BUILD_ARGS=(--build "$BUILD_DIR" --parallel "$JOBS")
[ "$VERBOSE" -eq 1 ] && BUILD_ARGS+=(--verbose)

info "Building"
cmake "${BUILD_ARGS[@]}"

BINARY=$BUILD_DIR/vclock
[ -x "$BINARY" ] || die "build reported success but $BINARY is missing"

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
info "Build complete"
ok "    $BINARY ($(du -h "$BINARY" | cut -f1))"

# The clock draws onto a translucent surface, which only works under a
# compositing window manager; warn rather than fail, since the build itself is
# fine and the app still runs (with an opaque background).
if [ -z "${WAYLAND_DISPLAY:-}" ] && [ -z "${DISPLAY:-}" ]; then
    warn "no DISPLAY or WAYLAND_DISPLAY is set; vclock needs a graphical session to run"
fi

if [ "$DO_INSTALL" -eq 1 ]; then
    info "Installing"
    prefix=$(cache_value CMAKE_INSTALL_PREFIX "$BUILD_DIR/CMakeCache.txt")
    # Only escalate when the destination is not writable by this user.
    if [ -w "${prefix:-/usr/local}" ] || [ ! -e "${prefix:-/usr/local}" ]; then
        cmake --install "$BUILD_DIR"
    else
        as_root cmake --install "$BUILD_DIR"
    fi
fi

if [ "$RUN_AFTER" -eq 1 ]; then
    info "Running $BINARY"
    exec "$BINARY"
fi

printf '\nRun it with:\n    %s\n' "$BINARY"
