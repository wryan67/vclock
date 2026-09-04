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
#   ./build.sh --check-deps     report any missing system packages
#   ./build.sh --install-deps   install the missing system packages
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
CHECK_DEPS=0
DO_INSTALL=0
INSTALL_PREFIX=
RUN_AFTER=0
VERBOSE=0
DISTRO_TARGETS=
DISTRO_ARCHES=

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
      --check-deps      Report which build dependencies are missing and stop
      --install-deps    Install the missing build packages with the system
                        package manager, then continue
      --install         Run 'cmake --install' after a successful build
      --prefix PATH     Install prefix for --install (default: /usr/local)
  -r, --run             Launch vclock after a successful build
  -v, --verbose         Show the full compiler command lines
  -h, --help            This message

${C_BOLD}Packaging:${C_RESET}
      --distro TARGET   Build installable packages instead of building here.
                        One or more of deb, rpm, windows, macos, or 'all'.
                        Packages are built in containers, so the result
                        depends on the distribution targeted rather than on
                        this machine. Output goes to distro/out.
      --arch ARCH       Architectures to package for: amd64, arm64, or 'all'.
                        Defaults to this machine's, except with --distro all
                        which defaults to every architecture a target has.
                        x86_64 and x64 mean amd64; aarch64 means arm64.

${C_BOLD}Examples:${C_RESET}
  ./build.sh                                  # release build
  ./build.sh -t Debug -j 4                    # debug build, 4 jobs
  ./build.sh --clean --qt-dir ~/Qt/6.7.2/gcc_64
  ./build.sh --install --prefix ~/.local      # install into ~/.local/bin
  ./build.sh --distro deb                     # a .deb for this machine
  ./build.sh --distro deb --arch arm64        # a .deb for aarch64
  ./build.sh --distro all                     # everything this host can build
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
        --check-deps)    CHECK_DEPS=1; shift ;;
        --install)       DO_INSTALL=1; shift ;;
        --distro)        [ $# -ge 2 ] || die "$1 requires an argument"
                         DISTRO_TARGETS="$DISTRO_TARGETS $(echo "$2" | tr ',' ' ')"; shift 2 ;;
        --arch)          [ $# -ge 2 ] || die "$1 requires an argument"
                         DISTRO_ARCHES="$DISTRO_ARCHES $(echo "$2" | tr ',' ' ')"; shift 2 ;;
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
# Packaging
#
# --distro takes over from the ordinary build entirely: nothing is compiled on
# this machine, so none of the host dependency checking below applies.  Each
# package is built inside a container for the distribution it targets, which is
# what makes it possible to build a Fedora rpm on Ubuntu, and an aarch64 package
# on an x86_64 machine.
# ---------------------------------------------------------------------------

# Architecture names are not shared between package formats: dpkg says amd64,
# Microsoft says x64 and Apple says x86_64, all for the same processor.  One
# spelling is accepted on the command line and translated per target.
target_arch_name() {
    case "$1:$2" in
        deb:amd64|deb:arm64|rpm:amd64|rpm:arm64) printf '%s' "$2" ;;
        windows:amd64) printf 'x64' ;;
        macos:amd64)   printf 'x86_64' ;;
        macos:arm64)   printf 'arm64' ;;
        *)             printf '' ;;
    esac
}

# Why a combination cannot be built, for the summary at the end.  A run that
# quietly produces five files when eight were asked for is worse than one that
# says which three are missing and why.
skip_reason() {
    case "$1:$2" in
        windows:arm64)
            printf 'no MinGW-w64 Qt6 for aarch64 exists to cross-compile with' ;;
        macos:*)
            printf 'needs Apple hardware; run distro/macos/package.sh on a Mac, or the release workflow' ;;
        *)  printf 'unsupported combination' ;;
    esac
}

normalise_arch() {
    case $1 in
        amd64|x86_64|x64|x86-64) printf 'amd64' ;;
        arm64|aarch64)           printf 'arm64' ;;
        *)                       printf '%s' "$1" ;;
    esac
}

run_packaging() {
    local targets="" arches="" t a name wanted_all=0

    for t in $DISTRO_TARGETS; do
        case $t in
            all) targets="deb rpm windows macos"; wanted_all=1 ;;
            deb|rpm|windows|macos) targets="$targets $t" ;;
            *) die "unknown --distro target '$t' (expected deb, rpm, windows, macos or all)" ;;
        esac
    done

    for a in $DISTRO_ARCHES; do
        case $a in
            all) arches="amd64 arm64" ;;
            *)   name=$(normalise_arch "$a")
                 case $name in
                     amd64|arm64) arches="$arches $name" ;;
                     *) die "unknown --arch '$a' (expected amd64, arm64 or all)" ;;
                 esac ;;
        esac
    done

    if [ -z "$arches" ]; then
        if [ "$wanted_all" -eq 1 ]; then
            arches="amd64 arm64"
        else
            arches=$(normalise_arch "$(uname -m)")
        fi
    fi

    # An explicit single target with an architecture it does not have should
    # fail rather than be silently skipped: it is a typo, not a gap.
    local explicit=0
    [ "$wanted_all" -eq 0 ] && [ "$(echo $targets | wc -w)" -eq 1 ] &&
        [ "$(echo $arches | wc -w)" -eq 1 ] && explicit=1

    local built=0 skipped=""
    for t in $targets; do
        for a in $arches; do
            name=$(target_arch_name "$t" "$a")
            if [ -z "$name" ] || [ "$t" = macos ]; then
                if [ "$explicit" -eq 1 ]; then
                    die "$t has no $a build: $(skip_reason "$t" "$a")"
                fi
                skipped="$skipped
  $t $a -- $(skip_reason "$t" "$a")"
                continue
            fi
            "$SOURCE_DIR/distro/build-target.sh" "$t" "$name" || die "$t/$name failed"
            built=$((built + 1))
        done
    done

    printf '\n'
    if [ -n "$skipped" ]; then
        warn "not built on this host:$skipped"
        printf '\n'
    fi

    if [ "$built" -eq 0 ]; then
        die "nothing could be built for the targets requested"
    fi

    info "packages in $SOURCE_DIR/distro/out"
    ls -1sh "$SOURCE_DIR/distro/out" 2>/dev/null | tail -n +2 | while read -r line; do
        ok "  $line"
    done
}

if [ -n "$DISTRO_TARGETS" ]; then
    run_packaging
    exit 0
fi

[ -z "$DISTRO_ARCHES" ] || die "--arch only means something with --distro"


# ---------------------------------------------------------------------------
# Dependencies
#
# Each requirement knows how to test for itself and what it is called on each
# distribution, so a failed preflight can name the packages that are actually
# missing rather than reciting the whole list.
# ---------------------------------------------------------------------------
DISTRO_ID=
DISTRO_NAME=
DISTRO_FAMILY=
PKG_TOOL=

detect_distro() {
    local id= like= name= release=${OS_RELEASE_FILE:-/etc/os-release}

    # Sourced in a subshell so the file's own variables do not leak in here,
    # and read back as one line rather than through three separate subshells.
    # The separator is a unit separator rather than a tab, because tab is an IFS
    # whitespace character: runs of it collapse, and a distribution with no
    # ID_LIKE would lose the field after it.
    if [ -r "$release" ]; then
        IFS=$'\037' read -r id like name < <(
            # shellcheck disable=SC1090
            . "$release" 2>/dev/null
            printf '%s\037%s\037%s\n' "${ID:-}" "${ID_LIKE:-}" "${PRETTY_NAME:-${NAME:-}}"
        ) || true
    fi
    DISTRO_ID=$id
    DISTRO_NAME=${name:-unknown Linux}

    # ID_LIKE is checked as well as ID, so derivatives (Mint, Pop!_OS, Rocky,
    # Alma) are recognised without naming every one of them.
    case " $id $like " in
        *" debian "*|*" ubuntu "*)             DISTRO_FAMILY=debian ;;
        *" fedora "*|*" rhel "*|*" centos "*)  DISTRO_FAMILY=rhel ;;
        *" arch "*)                            DISTRO_FAMILY=arch ;;
        *" suse "*|*" opensuse"*)              DISTRO_FAMILY=suse ;;
        *" alpine "*)                          DISTRO_FAMILY=alpine ;;
        *)                                     DISTRO_FAMILY= ;;
    esac

    case $DISTRO_FAMILY in
        debian) PKG_TOOL="apt-get install -y" ;;
        # RHEL 8 and later ship dnf; yum is kept for CentOS 7 and its kin.
        rhel)   if command -v dnf >/dev/null 2>&1; then PKG_TOOL="dnf install -y"
                else PKG_TOOL="yum install -y"; fi ;;
        arch)   PKG_TOOL="pacman -S --needed --noconfirm" ;;
        suse)   PKG_TOOL="zypper install -y" ;;
        alpine) PKG_TOOL="apk add" ;;
        *)      PKG_TOOL= ;;
    esac
}

# The package that provides a given requirement on the detected distribution.
package_for() {
    case "$DISTRO_FAMILY:$1" in
        debian:compiler) echo build-essential ;;
        rhel:compiler)   echo gcc-c++ ;;
        arch:compiler)   echo base-devel ;;
        suse:compiler)   echo gcc-c++ ;;
        alpine:compiler) echo build-base ;;

        *:cmake)         echo cmake ;;

        debian:qtbase)   echo qt6-base-dev ;;
        rhel:qtbase)     echo qt6-qtbase-devel ;;
        arch:qtbase)     echo qt6-base ;;
        suse:qtbase)     echo qt6-base-devel ;;
        alpine:qtbase)   echo qt6-qtbase-dev ;;

        debian:qtsvg)    echo qt6-svg-dev ;;
        rhel:qtsvg)      echo qt6-qtsvg-devel ;;
        arch:qtsvg)      echo qt6-svg ;;
        suse:qtsvg)      echo qt6-svg-devel ;;
        alpine:qtsvg)    echo qt6-qtsvg-dev ;;

        debian:xkb)      echo libxkbcommon-dev ;;
        rhel:xkb)        echo libxkbcommon-devel ;;
        arch:xkb)        echo libxkbcommon ;;
        suse:xkb)        echo libxkbcommon-devel ;;
        alpine:xkb)      echo libxkbcommon-dev ;;

        *) echo "" ;;
    esac
}

MISSING_KEYS=()
MISSING_LABELS=()
# Things the build works without, but which CMake grumbles about when they are
# absent. Kept apart so a missing one never stops a build that would succeed.
OPTIONAL_KEYS=()
OPTIONAL_LABELS=()

want() {
    MISSING_KEYS+=("$1")
    MISSING_LABELS+=("$2")
}

want_optional() {
    OPTIONAL_KEYS+=("$1")
    OPTIONAL_LABELS+=("$2")
}

# Qt6Gui lists XKB as a dependency of its private interface. vclock does not
# touch that interface, so the build is fine without it, but CMake still says
# "Could NOT find XKB" every configure. The headers cost nothing and silence it.
xkb_present() {
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists xkbcommon; then
        return 0
    fi
    local dir
    for dir in /usr/include /usr/local/include; do
        [ -f "$dir/xkbcommon/xkbcommon.h" ] && return 0
    done
    return 1
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

# Fills MISSING_KEYS/MISSING_LABELS. Returns 0 when everything is present.
# Qt is probed last, because a successful probe also settles QT_PREFIX.
check_dependencies() {
    MISSING_KEYS=(); MISSING_LABELS=()
    OPTIONAL_KEYS=(); OPTIONAL_LABELS=()

    if ! xkb_present; then
        want_optional xkb "xkbcommon development files (optional; without them CMake reports \"Could NOT find XKB\")"
    fi

    if ! command -v cmake >/dev/null 2>&1; then
        want cmake "cmake (not on PATH)"
    else
        CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
        if [ "$(printf '3.16\n%s\n' "$CMAKE_VERSION" | sort -V | head -n1)" != "3.16" ]; then
            want cmake "cmake 3.16 or newer (found $CMAKE_VERSION)"
        fi
    fi

    if ! command -v c++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1 \
       && ! command -v clang++ >/dev/null 2>&1; then
        want compiler "a C++17 compiler (no c++, g++ or clang++ on PATH)"
    fi

    if ! find_qt_prefix; then
        # Widgets and Svg are packaged separately nearly everywhere, and Svg is
        # the one usually absent, so they are reported one by one.
        if ! qt_module_present Widgets; then
            want qtbase "Qt 6 Widgets development files"
        fi
        if ! qt_module_present Svg; then
            want qtsvg "Qt 6 Svg development files"
        fi
        # Both halves found separately but not together: two different Qt
        # installations, one of which is incomplete. No package fixes that.
        if [ ${#MISSING_KEYS[@]} -eq 0 ]; then
            want qt-split "a single Qt with both Widgets and Svg (they were found under different prefixes)"
        fi
    fi

    [ ${#MISSING_KEYS[@]} -eq 0 ]
}

# The packages that would satisfy the current MISSING_KEYS, deduplicated.
# With "optional" as the first argument, the optional ones instead.
missing_packages() {
    local key pkg seen=" " keys
    if [ "${1-}" = optional ]; then
        keys=(${OPTIONAL_KEYS+"${OPTIONAL_KEYS[@]}"})
    else
        keys=(${MISSING_KEYS+"${MISSING_KEYS[@]}"})
    fi
    for key in ${keys+"${keys[@]}"}; do
        pkg=$(package_for "$key")
        [ -n "$pkg" ] || continue
        case $seen in *" $pkg "*) continue ;; esac
        seen="$seen$pkg "
        printf '%s\n' "$pkg"
    done
}

report_missing() {
    local label pkgs
    printf '%smissing dependencies:%s\n' "$C_RED$C_BOLD" "$C_RESET" >&2
    for label in ${MISSING_LABELS+"${MISSING_LABELS[@]}"}; do
        printf '    %s\n' "$label" >&2
    done

    pkgs=$(missing_packages | tr '\n' ' ')
    pkgs=${pkgs% }
    printf '\n  Detected system: %s\n' "$DISTRO_NAME" >&2

    if [ -z "$PKG_TOOL" ]; then
        printf '\n  This distribution is not recognised, so no install command can be\n' >&2
        printf '  suggested. Install CMake 3.16+, a C++17 compiler, and the Qt 6\n' >&2
        printf '  Widgets and Svg development packages using its package manager.\n' >&2
    elif [ -n "$pkgs" ]; then
        printf '\n  Install them with:\n    ./build.sh --install-deps\n  or by hand:\n    sudo %s %s\n' \
            "$PKG_TOOL" "$pkgs" >&2
        # Fedora carries Qt 6 in its own repositories; RHEL and its rebuilds
        # do not, so only they are told about EPEL.
        case $DISTRO_ID in
            rhel|centos|rocky|almalinux|ol)
                printf '\n  On RHEL and its rebuilds the Qt 6 packages live in EPEL:\n    sudo %s epel-release\n' \
                    "$PKG_TOOL" >&2 ;;
        esac
    fi
    printf '\n' >&2
}

# Printed after a successful check: nothing here stops a build.
report_optional() {
    [ ${#OPTIONAL_LABELS[@]} -eq 0 ] && return 0
    local label pkgs
    printf '%snote:%s optional dependencies are missing:\n' "$C_BOLD" "$C_RESET" >&2
    for label in "${OPTIONAL_LABELS[@]}"; do
        printf '    %s\n' "$label" >&2
    done
    pkgs=$(missing_packages optional | tr '\n' ' '); pkgs=${pkgs% }
    if [ -n "$PKG_TOOL" ] && [ -n "$pkgs" ]; then
        printf '  Install with:\n    ./build.sh --install-deps\n  or by hand:\n    sudo %s %s\n' \
            "$PKG_TOOL" "$pkgs" >&2
    fi
    printf '\n' >&2
}

install_dependencies() {
    detect_distro
    check_dependencies || true

    if [ ${#MISSING_KEYS[@]} -eq 0 ] && [ ${#OPTIONAL_KEYS[@]} -eq 0 ]; then
        info "All build dependencies are already installed"
        return 0
    fi

    local pkgs
    pkgs=$( { missing_packages; missing_packages optional; } | tr '\n' ' ')
    pkgs=${pkgs% }
    if [ -z "$PKG_TOOL" ] || [ -z "$pkgs" ]; then
        report_missing
        die "cannot install automatically on this system"
    fi

    info "Installing: $pkgs"
    # shellcheck disable=SC2086
    as_root $PKG_TOOL $pkgs
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

qt_prefix_has_module() {
    local prefix=$1 module=$2 libdir major
    for major in 6 5; do
        for libdir in lib lib64 lib/x86_64-linux-gnu lib/aarch64-linux-gnu; do
            [ -d "$prefix/$libdir/cmake/Qt$major$module" ] && return 0
        done
    done
    return 1
}

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
        if ! qt_prefix_has_modules "$QT_PREFIX"; then
            # Name the half that is absent; being told "Widgets + Svg" when one
            # of the two is plainly there is no help at all.
            local absent=
            qt_prefix_has_module "$QT_PREFIX" Widgets || absent="Widgets"
            qt_prefix_has_module "$QT_PREFIX" Svg || absent="${absent:+$absent and }Svg"
            die "no Qt ${absent:-Widgets + Svg} CMake package under: $QT_PREFIX"
        fi
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
    done < <(qt_installer_prefixes)

    return 1
}

qt_installer_prefixes() {
    ls -d "$HOME"/Qt/*/gcc_64 "$HOME"/Qt/*/gcc_arm64 \
          /opt/Qt/*/gcc_64 /opt/Qt/*/gcc_arm64 \
          /opt/Qt*/*/gcc_64 /opt/Qt*/*/gcc_arm64 2>/dev/null | sort -Vr
}

# Is one Qt module -- Widgets or Svg -- installed anywhere we would look?  Used
# only to explain a failure, so it does not care whether the two modules turn up
# under the same prefix; find_qt_prefix has already established that they do not.
qt_module_present() {
    local module=$1 prefix tool candidates

    # Collected into a variable rather than piped into a loop: leaving a pipe
    # early raises SIGPIPE in the producer, which pipefail then reports as a
    # failure of the whole search.
    candidates=$(
        [ -n "$QT_PREFIX" ] && printf '%s\n' "$QT_PREFIX"
        for tool in qmake6 qmake-qt6 qmake; do
            command -v "$tool" >/dev/null 2>&1 \
                && "$tool" -query QT_INSTALL_PREFIX 2>/dev/null
        done
        printf '/usr\n/usr/local\n'
        qt_installer_prefixes
    ) || true

    while IFS= read -r prefix; do
        [ -n "$prefix" ] || continue
        qt_prefix_has_module "$prefix" "$module" && return 0
    done <<<"$candidates"

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
detect_distro

[ "$INSTALL_DEPS" -eq 1 ] && install_dependencies

# Everything is checked in one pass, so a bare machine is told about all of its
# missing pieces at once instead of one per run.
if ! check_dependencies; then
    report_missing
    for key in "${MISSING_KEYS[@]}"; do
        case $key in qtbase|qtsvg|qt-split)
            printf '  Qt was looked for via qmake on PATH, /usr, /usr/local,\n' >&2
            printf '  ~/Qt/*/gcc_64 and /opt/Qt/*/gcc_64. If it is installed\n' >&2
            printf '  elsewhere, point at it explicitly:\n' >&2
            printf '    ./build.sh --qt-dir /path/to/Qt/6.5.3/gcc_64\n\n' >&2
            break ;;
        esac
    done
    exit 1
fi

report_optional

if [ "$CHECK_DEPS" -eq 1 ]; then
    ok "All required build dependencies are present."
    printf '    system      %s\n' "$DISTRO_NAME"
    printf '    cmake       %s\n' "$CMAKE_VERSION"
    printf '    qt%s prefix  %s\n' "$QT_MAJOR" "$QT_PREFIX"
    exit 0
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
