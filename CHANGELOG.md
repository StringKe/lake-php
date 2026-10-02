# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [0.1.1] - 2026-10-02

### Fixed

- Panics inside the driver, including those on its runtime threads, are no longer silenced: each one is written to stderr as a single `pdo_lake panic:` line with the thread name, source location and message, so php-fpm with `catch_workers_output` records it in its log. Panics on PHP calls are still reported as PDO errors.

## [0.1.0] - 2026-09-28

First release.

### Added

- PDO driver `lake` for TiDB Cloud Lake and Databend, built on the official Rust `lake-driver` 0.34.
- REST API and FlightSQL transports; `key=value` and URL DSNs with pass-through settings.
- Prepared statements with positional and named placeholders, server-side parameters on Databend 1.2.900 and newer, client-side emulation otherwise, binary LOB parameters.
- All PDO fetch modes, streaming results, column metadata, row counts and transactions.
- `Pdo\Lake` with warehouse, database, role and setting control, query id, query statistics, `killQuery()`, `info()`, presigned URLs, stage uploads, `loadData()`, `loadFile()`, `streamLoad()`, `putFiles()` and `getFiles()`.
- SQLSTATE mapping for common server error codes, persistent connections.
- PECL `package.xml` and PIE `composer.json`.

[0.1.1]: https://github.com/stringke/lake-php/releases/tag/v0.1.1
[0.1.0]: https://github.com/stringke/lake-php/releases/tag/v0.1.0
