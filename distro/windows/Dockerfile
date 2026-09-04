# The Windows installer build environment.
#
# Fedora is here for one reason: it is the only mainstream distribution that
# packages a MinGW-w64 build of Qt6.  Debian and Ubuntu ship the MinGW compiler
# but no Qt to go with it, which leaves building Qt itself as the alternative.
#
# qt6-qtbase-devel is the *native* Qt.  Cross-compiling still needs host tools --
# moc, rcc and uic have to run on the machine doing the building, not on the
# machine the result is for.
FROM fedora:41

RUN dnf install -y \
      mingw64-gcc-c++ \
      mingw64-qt6-qtbase \
      mingw64-qt6-qtsvg \
      mingw64-filesystem \
      mingw32-nsis \
      qt6-qtbase-devel \
      cmake \
      ninja-build \
      make \
      binutils \
      file \
 && dnf clean all

COPY package.sh /usr/local/bin/package.sh
RUN chmod +x /usr/local/bin/package.sh
ENTRYPOINT ["/usr/local/bin/package.sh"]
