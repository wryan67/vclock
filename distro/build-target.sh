#!/usr/bin/env bash
#
# build-target.sh -- build one distribution package for one architecture.
#
# Called by build.sh, but usable on its own:
#
#     distro/build-target.sh deb arm64
#
# Packages are built inside containers so that the result depends on the
# distribution being targeted rather than on whatever happens to be installed
# on the machine doing the building.  Foreign architectures run under qemu,
# which is slow but produces genuine native binaries for that architecture.

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

TARGET=${1:-}
ARCH=${2:-}
[ -n "$TARGET" ] || die "usage: build-target.sh TARGET ARCH"
[ -n "$ARCH" ]   || die "usage: build-target.sh TARGET ARCH"

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
    host=$(uname -m)
    case $host in
        x86_64|amd64) host_arch=amd64 ;;
        aarch64|arm64) host_arch=arm64 ;;
        *) host_arch=$host ;;
    esac
    if [ "$ARCH" != "$host_arch" ]; then
        if ! docker run --rm --platform "$PLATFORM" \
                 "$([ "$ARCH" = arm64 ] && echo arm64v8/alpine || echo amd64/alpine)" \
                 true >/dev/null 2>&1; then
            die "this machine is $host_arch and cannot run $ARCH binaries yet.
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
