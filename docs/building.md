# Building and installing pdo_lake

The extension has two parts:

- `ffi/`: a Rust static library (`liblake_ffi.a`) that wraps the official `lake-driver` crate behind a C ABI.
- `src/`: the C code of the PDO driver, built with the usual `phpize` / `configure` / `make` flow.

`make` runs `cargo build --release --locked` in `ffi/` before it links the extension, so the normal build needs nothing beyond the prerequisites below.

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| PHP | 8.4 or newer, with `pdo` and `json` | Headers and `phpize` come from `php-dev` (Debian, Ubuntu), `php-devel` (Fedora, RHEL) or Homebrew `php`. |
| Rust | 1.88 or newer | Install with [rustup](https://rustup.rs). `cargo` must be on `PATH` when `configure` runs. |
| C toolchain | any recent `cc`, `make`, `autoconf` | Xcode Command Line Tools on macOS, `build-essential autoconf` on Debian. |
| Network | first build only | Cargo downloads the crates listed in `ffi/Cargo.lock`. |

Debian or Ubuntu:

```sh
sudo apt-get install -y php-dev build-essential autoconf curl
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
. "$HOME/.cargo/env"
```

macOS with Homebrew:

```sh
xcode-select --install
brew install php autoconf rustup
rustup-init -y
```

## Option 1: PECL

Install straight from a release tarball:

```sh
pecl install https://github.com/stringke/lake-php/releases/download/v0.1.2/pdo_lake-0.1.2.tgz
```

or from a tarball you built yourself (see [development.md](development.md#packaging)):

```sh
pecl install ./pdo_lake-0.1.2.tgz
```

PECL asks for the `with-lake-ffi` directory; press Enter to keep `no` and build the Rust part with cargo. When PECL runs through `sudo`, the root environment usually has no `cargo`; either keep Rust on root's `PATH` (`sudo env "PATH=$PATH" pecl install ...`) or build as your user with option 3 and install with `sudo make install`.

## Option 2: PIE

[PIE](https://github.com/php/pie) builds from the tagged sources in this repository:

```sh
pie install stringke/lake-php
```

If the package is not on Packagist yet, register the repository first:

```sh
pie repository:add vcs https://github.com/stringke/lake-php
pie install stringke/lake-php:^0.1
```

PIE enables the extension in `php.ini` for you. A prebuilt Rust library is passed as `pie install stringke/lake-php --with-lake-ffi=/opt/lake-ffi`.

## Option 3: from source

```sh
git clone https://github.com/stringke/lake-php.git
cd lake-php
phpize
./configure
make
make test            # optional, needs a running server, see development.md
sudo make install
```

Everything the build generates is ignored by git; `phpize --clean` removes it.

When several PHP versions are installed, pick one with `phpize8.4` / `--with-php-config=/usr/bin/php-config8.4`, or the Homebrew paths under `$(brew --prefix php@8.4)/bin`.

## Enabling the extension

`make install` copies `pdo_lake.so` into `php-config --extension-dir`. Load it after `pdo`:

```ini
; for example /etc/php/8.4/mods-available/pdo_lake.ini, or conf.d/ext-pdo_lake.ini
extension=pdo_lake
```

Check:

```sh
php -m | grep pdo_lake
php --ri pdo_lake
php -r 'var_dump(in_array("lake", PDO::getAvailableDrivers()));'
```

## Configure options

| Option | Default | Meaning |
|---|---|---|
| `--enable-pdo-lake` | enabled | Build the driver. `--disable-pdo-lake` turns it off. |
| `--with-lake-ffi=DIR` | not set | Link `DIR/liblake_ffi.a` instead of building it with cargo. |

### Prebuilt Rust library

For hosts without Rust or without network access, build the library once on a machine with the same OS, architecture and C library, then link it:

```sh
# build machine
cd lake-php/ffi
cargo build --release --locked
cp target/release/liblake_ffi.a /opt/lake-ffi/

# target machine
cd lake-php
phpize
./configure --with-lake-ffi=/opt/lake-ffi
make && sudo make install
```

The library only depends on the system libraries listed in `config.m4` (`pthread`, `dl`, `m`, `rt` on Linux; `CoreFoundation`, `Security`, `SystemConfiguration`, `iconv`, `resolv` on macOS). TLS uses rustls, so OpenSSL is not needed.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `configure: error: cargo is required ...` | `cargo` is not on `PATH` for the shell that runs `configure` (often `sudo`). Install Rust or pass `--with-lake-ffi`. |
| `rustc 1.xx is not supported by ...` | Rust is older than 1.88. Run `rustup update`. |
| Cargo fails to download crates | The first build needs crates.io. Configure a mirror in `~/.cargo/config.toml`, or use a prebuilt library. |
| `phpize: Permission denied` on files such as `build/Makefile.global` | A previous `phpize` copied read-only files. Run `phpize --clean` and start again. |
| `PDO::__construct(): could not find driver` | The extension is not loaded, or it is loaded before `pdo`. Check `php --ini` and the order of the `extension=` lines. |
| `configure: line 631: [: ...: binary operator expected` | A harmless message from PHP's bundled libtool when `PATH` contains a directory with a space. It does not affect the build. |

## Uninstall

```sh
pecl uninstall pdo_lake       # when installed with PECL
rm "$(php-config --extension-dir)/pdo_lake.so"   # otherwise
```

and remove the `extension=pdo_lake` line.
