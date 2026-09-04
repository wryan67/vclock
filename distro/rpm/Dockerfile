# The .rpm build environment.
#
# Fedora rather than an Enterprise Linux base because EL only carries Qt6
# through EPEL, where the package names and versions move independently of the
# distribution.  The resulting rpm installs on Fedora and on EL with EPEL
# enabled, which is where its dependencies come from either way.
FROM fedora:41

RUN dnf install -y \
      gcc-c++ \
      cmake \
      ninja-build \
      make \
      rpm-build \
      file \
      qt6-qtbase-devel \
      qt6-qtsvg-devel \
      libxcb-devel \
      pkgconf-pkg-config \
 && dnf clean all

COPY package.sh /usr/local/bin/package.sh
RUN chmod +x /usr/local/bin/package.sh
ENTRYPOINT ["/usr/local/bin/package.sh"]
