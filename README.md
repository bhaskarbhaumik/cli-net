# net

A fast C11 CLI that lists macOS network interfaces with their hardware port,
IPv4 address, MAC address, link status, max bandwidth and serial number — as
plain colored text or as a bordered ncurses table.

## Build

Three interchangeable build paths; all produce a `net` binary.

```sh
make                # hand-written GNU make build -> ./net
./build.sh          # CMake release build         -> build/net
cmake -S . -B build && cmake --build build
```

The compiler is auto-detected: `clang` is preferred, `gcc` is the fallback.

### Autotools

The autoconf/automake build must be configured **out of tree** — `configure`
refuses an in-tree run so it can never overwrite the hand-written `./Makefile`:

```sh
autoreconf --install
mkdir build-auto && cd build-auto
../configure && make
sudo make install
```

`make dist` from the build directory produces a self-contained
`net-<version>.tar.gz` release tarball.

### Requirements

- macOS (uses `SystemConfiguration`, `CoreFoundation` and `IOKit`)
- `ncurses`
- `clang` or `gcc`
- `cmake` >= 3.15 for the CMake path
- `autoconf` >= 2.69 and `automake` >= 1.16 for the autotools path

## Install

Every build path installs the binary and the `net(1)` man page.

```sh
make install                   # /usr/local/bin/net + share/man/man1/net.1
make install PREFIX=~/.local   # or anywhere else
make uninstall                 # remove both again
```

From an autotools build directory use `./configure --prefix=...` instead.

## Versioning

`VERSION` is the single source of truth. All three build systems read it and
compile the value in as `NET_VERSION`, so bumping that one file is enough.

One caveat for the autotools path: `configure.ac` reads `VERSION` via
`m4_esyscmd_s`, which is expanded when `autoconf` runs, so a version bump needs
`autoreconf --force --install` to reach the generated `configure`.

## Usage

```
Usage: net [OPTIONS]

Show network interfaces and their IPv4 addresses.

Options:
  -h, --help     Show this help message and exit
  -v, --version  Print version and exit
  -a, --all      Show all interfaces, including inactive ones
  -l, --long     Long view: all columns plus max bandwidth and serial number
                 (default shows Hardware Port and IPv4 Address only)
  -t, --table    Render a rich bordered table using ncurses
```

Colors are emitted only when stdout is a TTY, so piping the output stays clean.

## Layout

```
src/net.c        the program
VERSION          single source of truth for the version
Makefile         GNU make build
CMakeLists.txt   CMake build
build.sh         CMake wrapper with compiler detection
configure.ac     autoconf input (out-of-tree builds only)
Makefile.am      automake input
m4/              autoconf macro directory
man/net.1        man page
prototypes/      earlier exploratory versions (x01–x03), not built
```

## License

MIT — see [LICENSE](LICENSE).
