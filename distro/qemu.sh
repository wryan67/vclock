# qemu.sh -- let this machine run foreign-architecture containers, for as long
# as a build needs to and no longer.  Sourced by build.sh and build-target.sh
# rather than run on its own.
#
# Building an arm64 package on an amd64 machine means running arm64 binaries,
# and the kernel will only do that if a handler for them has been registered
# with binfmt_misc.  Registering one is a host-wide change, so this puts it
# back afterwards: the machine is left as it was found, whether the build
# worked, failed, or was interrupted.
#
# It is deliberately careful about what it takes away.  A handler that was
# already registered before a build started belongs to somebody else -- the
# distribution, or the user, or a longer-lived tool -- and is left alone.

QEMU_BINFMT_IMAGE=${QEMU_BINFMT_IMAGE:-tonistiigi/binfmt}

# Architectures this shell registered itself, and so may unregister.
QEMU_REGISTERED=

# The image to prove an architecture with.  Alpine because it is small, and
# 'true' because a container that starts at all has proved the point.
qemu_probe_image() {
    case $1 in
        arm64) echo arm64v8/alpine ;;
        amd64) echo amd64/alpine ;;
        *)     return 1 ;;
    esac
}

# Can this machine run containers of $1?
qemu_runnable() {
    local image
    image=$(qemu_probe_image "$1") || return 1
    docker run --rm --platform "linux/$1" "$image" true >/dev/null 2>&1
}

# Make $1 runnable, if it is not already.  Returns non-zero when it cannot,
# leaving the caller to say why that matters for what it was doing.
qemu_register() {
    local arch=$1
    qemu_runnable "$arch" && return 0

    info "registering the qemu handler for $arch"
    docker run --privileged --rm "$QEMU_BINFMT_IMAGE" --install "$arch" \
        >/dev/null 2>&1 || return 1

    # Recorded before the check below, so that a registration which happened
    # but does not work is still taken away again rather than left behind.
    QEMU_REGISTERED="$QEMU_REGISTERED $arch"
    qemu_runnable "$arch"
}

# Take back every handler this shell registered.  Safe to call more than once,
# and safe to call when there is nothing to do, which is what makes it usable
# from a trap.
qemu_release() {
    local arch
    for arch in $QEMU_REGISTERED; do
        info "removing the qemu handler for $arch"
        docker run --privileged --rm "$QEMU_BINFMT_IMAGE" --uninstall "$arch" \
            >/dev/null 2>&1 ||
            warn "could not remove the qemu handler for $arch; remove it with
       docker run --privileged --rm $QEMU_BINFMT_IMAGE --uninstall $arch"
    done
    QEMU_REGISTERED=
}

# Why a machine that cannot be made to run $1 cannot be made to run it.  Docker
# has to be usable for any of this, so that is the likeliest answer.
qemu_register_error() {
    printf '%s' "could not register a qemu handler for $1.  This needs to run a
       privileged container, so check that docker will do that for this
       account:

           docker run --privileged --rm $QEMU_BINFMT_IMAGE --install $1"
}
