# mostly copied from nixpkgs: https://github.com/NixOS/nixpkgs/blob/a127deeb889b111138f5152e54681577706ab741/pkgs/misc/emulators/melonDS/default.nix
# grab updates from there (at master, maybe bump pinned nixpkgs too), though I don't expect any huge changes

{ pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/a127deeb889b111138f5152e54681577706ab741.tar.gz") {}
, mkDerivation ? pkgs.stdenv.mkDerivation
, cmake ? pkgs.cmake
, pkg-config ? pkgs.pkg-config
, SDL2 ? pkgs.SDL2
, qtbase ? pkgs.qt5.qtbase
, epoxy ? pkgs.epoxy
, libarchive ? pkgs.libarchive
, libpcap ? pkgs.libpcap
, libslirp ? pkgs.libslirp
, pcre ? pkgs.pcre
, wrapGAppsHook ? pkgs.wrapGAppsHook
}:

mkDerivation {
  name = "melonDS-git-ci";

  src = builtins.path { path = ./.; name = "melonDS"; }; # use the directory this file is in (as opposed to downloading a source tarball)

  nativeBuildInputs = [ cmake pkg-config wrapGAppsHook ];
  buildInputs = [
    SDL2
    qtbase
    epoxy
    libarchive
    libpcap
    libslirp
    pcre
  ];

  cmakeFlags = [ "-UUNIX_PORTABLE" ];
}
