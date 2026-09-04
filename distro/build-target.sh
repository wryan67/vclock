#!/usr/bin/env bash
#
# build-target.sh -- build one distribution package for one architecture.
#
# Called by build.sh, but usable on its own:
#
#     distro/build-target.sh deb arm64
#     distro/build-target.sh --native deb arm64   # here, not in a container
#
# Packages are built inside containers so that the result depends on the
# distribution being targeted rather than on whatever happens to be installed
# on the machine doing the building.  Foreign architectures run under qemu,
# which is slow but produces genuine native binaries for that architecture.
#
# --native lifts that when the package is only meant for the machine building
# it: there is nothing for a container to pin down when the target and the host
# are the same architecture and the package is going straight back onto this
# machine.  build.sh passes it for --this and for nothing else, so anything
# meant to be handed to somebody else is still built the portable way.

set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "$HERE/.." && pwd)
OUT=${VCLOCK_OUT:-$HERE/out}

C_RESET=''; C_BOLD=''; C_RED=''; C_BLUE=''; C_YELLOW=''
if [ -t 1 ] && [ "${NO_COLOR:-}" = "" ]; then
    C_RESET=$'\033[0m'; C_BOLD=$'\033[1m'; C_RED=$'\033[31m'
    C_BLUE=$'\033[34m'; C_YELLOW=$'\033[33m'
fi
info() { printf '%s==>%s %s\n' "$C_BLUE$C_BOLD" "$C_RESET" "$*"; }
warn() { printf '%swarning:%s %s\n' "$C_YELLOW$C_BOLD" "$C_RESET" "$*" >&2; }
die()  { printf '%serror:%s %s\n' "$C_RED$C_BOLD" "$C_RESET" "$*" >&2; exit 1; }

ALLOW_NATIVE=0
NATIVE_BUILD_DIR=
while [ $# -gt 0 ]; do
    case $1 in
        --native)    ALLOW_NATIVE=1; shift ;;
        --container) ALLOW_NATIVE=0; shift ;;
        --) shift; break ;;
        -*) die "unknown option '$1'; expected --native or --container" ;;
        *)  break ;;
    esac
done

TARGET=${1:-}
ARCH=${2:-}
[ -n "$TARGET" ] || die "usage: build-target.sh [--native] TARGET ARCH"
[ -n "$ARCH" ]   || die "usage: build-target.sh [--native] TARGET ARCH"

# What this machine calls itself, in the names the packagers use.
case $(uname -m) in
    x86_64|amd64)  HOST_ARCH=amd64 ;;
    aarch64|arm64) HOST_ARCH=arm64 ;;
    *)             HOST_ARCH=$(uname -m) ;;
esac
# --------------------------------------------------------------- what exists

# Which architectures each target can be built for, and whether the
# architecture names the container's platform or only the output.  A Windows
# build is a cross-compile, so its container is always native.
case $TARGET in
    deb|rpm)
        case $ARCH in
            amd64|arm64) ;;
            *) die "$TARGET has no $ARCH build; it takes amd64 or arm64" ;;
        esac
        PLATFORM="linux/$ARCH"
        ;;
    windows)
        case $ARCH in
            x64) ;;
            *) die "the Windows build takes x64" ;;
        esac
        # Cross-compiled, so the container matches the host and only the
        # produced binary is for Windows.
        PLATFORM=""
        ;;
    macos)
        die "macOS packages cannot be built on Linux.  Apple's SDK licence
       restricts it to Apple hardware, and an app that is neither signed nor
       notarised is refused by Gatekeeper on arrival.  The recipe lives in
       distro/macos/package.sh and runs on a Mac; .github/workflows/release.yml
       runs it on GitHub's macOS runners." ;;
    *)
        die "unknown target '$TARGET'; expected deb, rpm, windows or macos" ;;
esac

[ -f "$HERE/$TARGET/Dockerfile" ] || die "no recipe at distro/$TARGET/Dockerfile"

# ------------------------------------------------------------------ natively
#
# Same steps the container runs, on the machine itself.  Only worth it when the
# package is for this machine: built here it carries whatever this system has,
# so a deb built on a newer Ubuntu asks for a newer glibc than an older one can
# offer.  That is exactly right for a package going straight back onto the
# machine that built it and wrong for one being handed to anybody else, which is
# why build.sh asks for this on --this and never on --distro.

# What stops a native build, or nothing at all if it can go ahead.  Checked in
# full rather than one at a time, so a missing pair is one round of installing
# rather than two.
native_obstacle() {
    case $TARGET in
        deb|rpm) ;;
        # The Windows installer is cross-compiled with MinGW and assembled with
        # NSIS, neither of which is what a Linux host has lying around; macOS
        # never reaches here.
        *) printf 'only deb and rpm can be built without a container'; return ;;
    esac

    # A foreign architecture is the one thing a container genuinely provides
    # that this machine cannot.
    if [ "$ARCH" != "$HOST_ARCH" ]; then
        printf 'this machine is %s and the package is for %s' "$HOST_ARCH" "$ARCH"
        return
    fi

    local missing="" tool
    for tool in cmake cpack; do
        command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
    done
    case $TARGET in
        deb) for tool in dpkg-deb dpkg-shlibdeps fakeroot; do
                 command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
             done ;;
        rpm) command -v rpmbuild >/dev/null 2>&1 || missing="$missing rpmbuild" ;;
    esac
    [ -z "$missing" ] || printf 'not installed here:%s' "$missing"
}

build_native() {
    local out_pattern
    # Out of the source tree, so it cannot be confused with the ordinary build
    # in build/ and cannot leave a half-finished one behind.  Not local: the
    # trap that removes it runs when the shell exits, by which time a local
    # would be long out of scope.
    NATIVE_BUILD_DIR=$(mktemp -d) || die "could not create a build directory"
    trap 'rm -rf "${NATIVE_BUILD_DIR:-}"' EXIT
    local build=$NATIVE_BUILD_DIR

    local generator=()
    command -v ninja >/dev/null 2>&1 && generator=(-G Ninja)

    info "building $TARGET for $ARCH here, without a container"
    cmake -S "$ROOT" -B "$build" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release ||
        die "could not configure the build.  ./build.sh --check-deps lists what
       is missing; --install-deps installs it."
    cmake --build "$build" -j"$(nproc)" || die "the build failed"

    case $TARGET in
        deb) ( cd "$build" && cpack -G DEB ) || die "cpack could not build the deb"
             out_pattern='*.deb' ;;
        rpm) ( cd "$build" && cpack -G RPM ) || die "cpack could not build the rpm"
             out_pattern='*.rpm' ;;
    esac

    local f found=0
    for f in "$build"/$out_pattern; do
        [ -e "$f" ] || continue
        # The same check the container makes: a packager that cannot read back
        # what was just written means what we are about to hand over is not a
        # package at all.
        case $TARGET in
            deb) dpkg-deb -I "$f" >/dev/null ;;
            rpm) rpm -qip "$f" >/dev/null ;;
        esac
        cp "$f" "$OUT/"
        echo "built $(basename "$f")"
        found=1
    done
    [ "$found" -eq 1 ] || die "the build finished but produced no package"
}

if [ "$ALLOW_NATIVE" -eq 1 ]; then
    mkdir -p "$OUT"
    obstacle=$(native_obstacle)
    if [ -z "$obstacle" ]; then
        build_native
        exit 0
    fi
    # Worth saying rather than silently starting docker: asking for a native
    # build and getting a container is a surprise, and the reason is the useful
    # part of it.
    warn "building in a container after all: $obstacle"
fi

# ------------------------------------------------------------- what is needed

command -v docker >/dev/null 2>&1 ||
    die "docker is needed to build packages and was not found on PATH"

docker info >/dev/null 2>&1 ||
    die "docker is installed but not usable by this account.  Either start the
       daemon, or add yourself to the docker group and log in again."

# A foreign architecture needs a qemu handler registered with the kernel.
# Without one the container starts and every process in it dies with 'exec
# format error', which is a confusing way to find out.
if [ -n "$PLATFORM" ]; then
    if [ "$ARCH" != "$HOST_ARCH" ]; then
        if ! docker run --rm --platform "$PLATFORM" \
                 "$([ "$ARCH" = arm64 ] && echo arm64v8/alpine || echo amd64/alpine)" \
                 true >/dev/null 2>&1; then
            die "this machine is $HOST_ARCH and cannot run $ARCH binaries yet.
       Register the qemu handlers once with:

           docker run --privileged --rm tonistiigi/binfmt --install $ARCH

       That is a host-wide change which survives reboot; undo it with
       --uninstall in place of --install."
        fi
    fi
fi

# ------------------------------------------------------------------- building

mkdir -p "$OUT"

IMAGE="vclock-build-$TARGET:$ARCH"

build_args=(build -t "$IMAGE" -f "$HERE/$TARGET/Dockerfile")
[ -n "$PLATFORM" ] && build_args+=(--platform "$PLATFORM")
build_args+=("$HERE/$TARGET")

info "preparing the $TARGET/$ARCH build environment"
docker "${build_args[@]}" >/dev/null || die "could not build the $TARGET image"

run_args=(run --rm)
[ -n "$PLATFORM" ] && run_args+=(--platform "$PLATFORM")
# As the invoking user, so the packages come out owned by whoever asked for
# them rather than by root, which would need privileges to clean up.
run_args+=(--user "$(id -u):$(id -g)")
run_args+=(-e HOME=/tmp)
run_args+=(-v "$ROOT:/src:ro" -v "$OUT:/out")
run_args+=("$IMAGE")

info "building $TARGET for $ARCH"
docker "${run_args[@]}" || die "the $TARGET/$ARCH build failed"
