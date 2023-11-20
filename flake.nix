{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  inputs.drgn = {
    flake = false;
    type = "github";
    owner = "JakeHillion";
    repo = "drgn";
    rev = "f54be805ec0e8d63f54f513b5246f47e4ee100c5";
  };
  inputs.folly = {
    flake = false;
    type = "github";
    owner = "JakeHillion";
    repo = "folly";
    rev = "8db54418e3ccdd97619ac8b69bb3702f82bb0f66";
  };

  description = "A flake for building and developing Object Introspection";

  outputs = { self, nixpkgs, drgn, folly, flake-utils }: (
    flake-utils.lib.eachSystem [ flake-utils.lib.system.x86_64-linux ]
      (system:
        let
          pkgs =
            import nixpkgs
              {
                system = "x86_64-linux";
                overlays = [
                  (self: super: {
                    boost = super.boost.override { enableShared = false; enableStatic = true; };
                    rocksdb = super.rocksdb.overrideAttrs ( old: rec {
                      version = "8.5.3";
                      src = super.fetchFromGitHub {
                        owner = "facebook";
                        repo = "rocksdb";
                        rev = "v${version}";
                        sha256 = "sha256-Qa4bAprXptA79ilNE5KSfggEDvNFHdrvDQ6SvzWMQus=";
                      };
                    });
                  })
                ];
              };
        in
        {
          packages.oid = with pkgs;
            pkgs.llvmPackages_12.stdenv.mkDerivation {
              name = "oid";
              src = self;

              postPatch = with pkgs; ''
                substituteInPlace CMakeLists.txt --replace '@gtest_src@' '${gtest.src}'
                substituteInPlace CMakeLists.txt --replace '@folly_src@' '${folly}'
              '';

              cmakeFlags = [
                "-Ddrgn_SOURCE_DIR=${drgn}"
                "-DGIT_SUBMODULES=Off"
                "-DWARNINGS_AS_ERRORS=Off"
              ];

              installPhase = ''
                mkdir -p $out/bin
                install -t $out/bin /build/source/build/oid
              '';

              nativeBuildInputs = with pkgs; [
                autoconf
                automake
                cmake
                gettext
                git
                libtool
                ninja
                pkgconf

                python310
                python310Packages.toml
                python310Packages.setuptools
              ];
              buildInputs = with pkgs; [
                bison
                boost
                bzip2
                curl
                double-conversion
                elfutils
                flex
                fmt
                gflags
                glog
                gtest
                icu
                jemalloc
                libarchive
                libmicrohttpd
                llvmPackages_15.libclang
                llvmPackages_15.llvm
                lzma
                msgpack
                range-v3
                rocksdb
                sqlite
                tomlplusplus
                libxml2
                zstd
              ];
            };

          packages.default = self.packages.${system}.oid;
        }) // flake-utils.lib.eachSystem flake-utils.lib.allSystems (system: {
      formatter = nixpkgs.legacyPackages.${system}.nixpkgs-fmt;
    })
  );
}

