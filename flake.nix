{
  description = "NASA cFS on the Atmosphere microkernel";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      devShells = forAllSystems (pkgs:
        let
          # GNU binutils for x86-64 ELF. The kernel's build script calls `ld`
          # and `objcopy` by their plain names, so wrap them under those names.
          binutils = pkgs.pkgsCross.x86_64-embedded.buildPackages.binutils;
          x86Tool = name: pkgs.writeShellScriptBin name ''
            exec ${binutils}/bin/x86_64-elf-${name} "$@"
          '';

          # clang as a cross compiler for freestanding x86-64 code. The
          # wrapped clang adds flags for the host; the bare one needs to be
          # told where its own headers (stddef.h, stdint.h) are.
          llvm = pkgs.llvmPackages;
          atmoCC = pkgs.writeShellScriptBin "atmo-cc" ''
            exec ${llvm.clang-unwrapped}/bin/clang \
              -resource-dir ${llvm.clang-unwrapped.lib}/lib/clang/${nixpkgs.lib.versions.major llvm.clang-unwrapped.version} \
              "$@"
          '';
        in
        {
          default = pkgs.mkShell {
            packages = [
              # Rust comes from rustup, which reads atmosphere/rust-toolchain.toml.
              # Verus pins a different toolchain the same way.
              pkgs.rustup
              atmoCC
              pkgs.cmake
              pkgs.gnumake
              pkgs.nasm
              pkgs.qemu
              pkgs.xxd
              pkgs.python3
              pkgs.git
              pkgs.curl
              pkgs.unzip
            ] ++ map x86Tool [ "ld" "objcopy" "objdump" "nm" "readelf" ];

            # tools/env.sh and the CMake toolchain file read this.
            ATMO_CC = "atmo-cc";
          };
        });
    };
}
