# net

A fast C11 CLI that lists macOS network interfaces with their hardware port,
IPv4 address, MAC address, link status, max bandwidth and serial number — as
plain colored text or as a bordered ncurses table.

## Build

Three interchangeable build paths; all produce a `net` binary.

```sh
make                # hand-written GNU make build -> ./net
./build.sh          # CMake release build       -> build/net
cmake -S . -B build && cmake --build build
```

The compiler is auto-detected: `clang` is preferred, `gcc` is the fallback.

### Requirements

- macOS (uses `SystemConfiguration`, `CoreFoundation` and `IOKit`)
- `ncurses`
- `clang` or `gcc`, and `cmake` >= 3.15 for the CMake path

## Install

```sh
make install                   # /usr/local/bin/net
make install PREFIX=~/.local   # or anywhere else
```

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
Makefile         GNU make build
CMakeLists.txt   CMake build
build.sh         CMake wrapper with compiler detection
prototypes/      earlier exploratory versions (x01–x03), not built
```

## License

MIT — see [LICENSE](LICENSE).
