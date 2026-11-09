# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [0.1.4] - 2026-11-09

### Fixed

- A result no longer fails after its last row when the TiDB Cloud Lake gateway loses the node that ran the query. The client fetches the query's `/final` URI after the last page; the gateway answered it with `Sticky session state lost: node '...' not found in cluster`, which surfaced as a query error although every row had arrived. That answer on the final request now ends the result normally; the same answer on a data page is still an error.
- Server error messages are cut before the `, request=Request {` dump the gateway appends, which carried the session cookie and internal addresses into application logs.

## [0.1.3] - 2026-11-02

### Fixed

- Server-side parameters land where they are written. Databend numbers bare `?` in syntax-tree order, so in a multi-branch `CASE` the `WHEN` values took the first parameters and the `THEN` values the rest. Placeholders are now sent as named holes numbered in text order.

## [0.1.2] - 2026-10-23

### Fixed

- On Linux the module is linked with `-z nodelete`, so it stays mapped after PHP unloads it at shutdown. Thread-local destructors registered by the Rust side (tokio, rand) run after that point and would otherwise jump into unmapped code; the same pattern made processes that had connected exit with a segmentation fault in `pdo_tidb` built with Rust 1.98.

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

[0.1.3]: https://github.com/stringke/lake-php/releases/tag/v0.1.3
[0.1.2]: https://github.com/stringke/lake-php/releases/tag/v0.1.2
[0.1.1]: https://github.com/stringke/lake-php/releases/tag/v0.1.1
[0.1.0]: https://github.com/stringke/lake-php/releases/tag/v0.1.0
