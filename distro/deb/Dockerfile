# The .deb build environment.
#
# Built on the oldest distribution vclock supports rather than the newest.
# glibc and Qt symbol versioning are backward compatible but not forward, so a
# package built on 24.04 installs on 24.04 and everything after it, while one
# built on the current release would refuse to install on anything older.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      ninja-build \
      file \
      dpkg-dev \
      fakeroot \
      qt6-base-dev \
      qt6-svg-dev \
      libgl1-mesa-dev \
      libxcb1-dev \
      pkg-config \
 && rm -rf /var/lib/apt/lists/*

COPY package.sh /usr/local/bin/package.sh
RUN chmod +x /usr/local/bin/package.sh
ENTRYPOINT ["/usr/local/bin/package.sh"]
