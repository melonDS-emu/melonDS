#!/bin/bash

# Copyright (c) 2002-2003, Apple Inc.
# Copyright (c) 2004-2021, The MacPorts Project.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of Apple Inc., The MacPorts Project nor the
#    names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# ======================================================================
#
# Note: the license above does not apply to the packages built by MacPorts,
# merely to the port descriptions (i.e., Portfiles, PortGroups, etc.).  It also
# might not apply to patches included in the MacPorts ports tree, which may be
# derivative works of the ports to which they apply.  The aforementioned
# artifacts are all covered by the licenses of the respective ports.

set -e

OS_MAJOR=$(uname -r | cut -f 1 -d .)
OS_ARCH=$(uname -m)
case "$OS_ARCH" in
    i586|i686|x86_64)
        OS_ARCH=i386
        ;;
esac

echo "::group::Disabling Spotlight"
# Disable Spotlight indexing. We don't need it, and it might cost performance
sudo mdutil -a -i off
echo "::endgroup::"


echo "::group::Uninstalling Homebrew"

# Move directories to /opt/off
echo "Moving directories..."
sudo mkdir /opt/off
/usr/bin/sudo /usr/bin/find /usr/local -mindepth 1 -maxdepth 1 -type d -print -exec /bin/mv {} /opt/off/ \;

# Unlink files
echo "Removing files..."
/usr/bin/sudo /usr/bin/find /usr/local -mindepth 1 -maxdepth 1 -type f -print -delete

# Rehash to forget about the deleted files
hash -r
echo "::endgroup::"

echo "::group::Installing MacPorts"
echo "Fetching..."
# Download resources in background ASAP but use later; do this after cleaning
# up Homebrew so that we don't end up using their curl!
curl -fsSLO "https://dl.bintray.com/macports-ci-env/macports-base/MacPorts-${OS_MAJOR}.tar.bz2" &
curl_mpbase_pid=$!
curl -fsSLO "https://dl.bintray.com/macports-ci-bot/getopt/getopt-v1.1.6.tar.bz2" &
curl_getopt_pid=$!

# Download and install MacPorts built by https://github.com/macports/macports-base/tree/master/.github
wait $curl_mpbase_pid

echo "Extracting..."
sudo tar -xpf "MacPorts-${OS_MAJOR}.tar.bz2" -C /
rm -f "MacPorts-${OS_MAJOR}.tar.bz2"
echo "::endgroup::"


echo "::group::Configuring MacPorts"
# Set PATH for portindex
source /opt/local/share/macports/setupenv.bash
# Set ports tree to $PWD/ports
sudo sed -i "" "s|rsync://rsync.macports.org/macports/release/tarballs/ports.tar|file://${PWD}/ports|; /^file:/s/default/nosync,default/" /opt/local/etc/macports/sources.conf
# CI is not interactive
echo "ui_interactive no" | sudo tee -a /opt/local/etc/macports/macports.conf >/dev/null
# Only download from the CDN, not the mirrors
echo "host_blacklist *.distfiles.macports.org *.packages.macports.org" | sudo tee -a /opt/local/etc/macports/macports.conf >/dev/null
# Try downloading archives from the private server after trying the public server
echo "archive_site_local https://packages.macports.org/:tbz2 https://packages-private.macports.org/:tbz2" | sudo tee -a /opt/local/etc/macports/macports.conf >/dev/null
# Prefer to get archives from the public server instead of the private server
# preferred_hosts has no effect on archive_site_local
# See https://trac.macports.org/ticket/57720
#echo "preferred_hosts packages.macports.org" | sudo tee -a /opt/local/etc/macports/macports.conf >/dev/null
echo "::endgroup::"

echo "::group::Running postflight"
# Create macports user
echo "Postflight..."
sudo /opt/local/libexec/macports/postflight/postflight
echo "::endgroup::"

echo "::group::Installing getopt"
# Install getopt required by mpbb
wait $curl_getopt_pid
echo "Extracting..."
sudo tar -xpf "getopt-v1.1.6.tar.bz2" -C /
echo "::endgroup::"
