# Development

## Layout

| Path | Content |
|---|---|
| `config.m4`, `Makefile.frag` | build definition |
| `package.xml` | PECL package definition |
| `composer.json` | package metadata read by PIE |
| `src/` | C sources, stub and generated arginfo |
| `ffi/` | Rust crate producing `liblake_ffi.a` |
| `tests/` | `.phpt` tests |
| `compose.yaml` | local Databend and S3 compatible storage for the tests |
| `docs/` | documentation |

Everything `phpize`, `configure`, `make` and `cargo` generate is ignored by git. `phpize --clean` removes the PHP build files, `cargo clean --manifest-path ffi/Cargo.toml` the Rust ones.

## Local server

`compose.yaml` starts Databend together with [RustFS](https://github.com/rustfs/rustfs) as S3 storage, so presigned URLs and `GET` work. Databend shares the network namespace of the storage container, which makes the presigned `127.0.0.1:9000` URLs reachable from the host.

```sh
docker compose up -d --wait
```

| Endpoint | Address |
|---|---|
| REST API | `127.0.0.1:8000` |
| FlightSQL | `127.0.0.1:8900` |
| S3 API | `127.0.0.1:9000` |

The user is `lake` with password `lake`. `docker compose down -v` removes everything.

## Build

```sh
phpize
./configure CFLAGS="-O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
make
```

The build is expected to produce no compiler or linker warnings with these flags. `-Wpedantic` is not used because PHP's own headers trigger it.

## Tests

```sh
make test NO_INTERACTION=1
```

The tests read their connection from the environment and default to the compose setup:

| Variable | Default |
|---|---|
| `PDO_LAKE_TEST_DSN` | `lake:host=127.0.0.1;port=8000;sslmode=disable` |
| `PDO_LAKE_TEST_FLIGHT_DSN` | `lake:host=127.0.0.1;port=8900;protocol=flight;sslmode=disable` |
| `PDO_LAKE_TEST_URL_DSN` | `lake:lake://lake:lake@127.0.0.1:8000/?sslmode=disable` |
| `PDO_LAKE_TEST_USER` | `lake` |
| `PDO_LAKE_TEST_PASSWORD` | `lake` |

Tests are skipped when the server cannot be reached. `016_presign_get.phpt` is skipped on storage without presign support, and parts of `015_server_params.phpt` need Databend 1.2.900 or newer.

A single test runs with `make test TESTS=tests/015_server_params.phpt`.

Rust unit tests, formatting and lints:

```sh
cd ffi
cargo fmt --check
cargo clippy --release --all-targets --locked
cargo test --locked
```

## Leak checks

On macOS, `leaks` checks both the Rust side and PHP's allocations:

```sh
MallocStackLogging=1 leaks --atExit -- php -d extension=modules/pdo_lake.so script.php
USE_ZEND_ALLOC=0 MallocStackLogging=1 leaks --atExit -- php -d extension=modules/pdo_lake.so script.php
```

On Linux, use `valgrind --leak-check=full` with `USE_ZEND_ALLOC=0`, or run the tests with `make test TESTS="-m"`.

## Changing the stub

`src/pdo_lake.stub.php` is the source of `src/pdo_lake_arginfo.h`. After editing it, regenerate the header with PHP's `gen_stub.php`:

```sh
php "$(php-config --prefix)/lib/php/build/gen_stub.php" src/pdo_lake.stub.php
```

Distribution packages may install `gen_stub.php` elsewhere (for example `/usr/lib/php/<version>/build/`).

## Packaging

```sh
phpize --clean
pecl package package.xml
```

This writes `pdo_lake-<version>.tgz` with exactly the files listed in `package.xml`. Before a release:

1. Update the version in `src/php_pdo_lake.h`, `ffi/Cargo.toml`, `package.xml` and `CHANGELOG.md`.
2. Run the full test suite and the lints above.
3. Build the tarball, install it with `pecl install ./pdo_lake-<version>.tgz` on a clean machine and run `php --ri pdo_lake`.
4. Tag `v<version>`, push the tag and attach the tarball to the GitHub release.

## Commits

Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/) in English, for example `fix(ffi): map server error 1303 to SQLSTATE 21S01` or `docs: document presign requirements`. Scopes in use: `driver` (C sources), `ffi` (Rust crate), `build`, `tests`, `docs`.
