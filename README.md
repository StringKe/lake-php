# lake-php

`pdo_lake` is a native PDO driver for [TiDB Cloud Lake](https://www.pingcap.com/) and [Databend](https://databend.com/). It links the official Rust driver ([`lake-driver`](https://crates.io/crates/lake-driver) 0.34) through a small C ABI and exposes it as a regular PDO driver plus a `Pdo\Lake` subclass for the warehouse-specific features.

```php
$db = new PDO('lake:host=127.0.0.1;port=8000;dbname=default;sslmode=disable', 'root', '');
$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$st = $db->prepare('SELECT number, number * :k FROM numbers(3)');
$st->execute(['k' => 10]);
foreach ($st as $row) {
    echo $row[0], ' ', $row[1], "\n";
}
```

## Features

- REST API (`lake://`) and FlightSQL (`lake+flight://`) transports, chosen by the DSN.
- PDO style `key=value` DSNs and full driver URLs, with every unknown key passed to the server as a session setting.
- Prepared statements with `?` and `:name` placeholders, repeated names, `bindParam` by reference and `PARAM_LOB` binary values; server-side parameter binding on servers that support it, client-side emulation otherwise.
- All PDO fetch modes, `getColumnMeta()`, `columnCount()` and `rowCount()`.
- Streaming results: rows are pulled page by page, so a large result set is never held in memory at once.
- Transactions (`beginTransaction`, `commit`, `rollBack`, `inTransaction`).
- Session control: warehouse, database, role and arbitrary settings; query id, query statistics and `killQuery()`.
- Stage and bulk loading: upload strings or files to a stage, `loadData()`, `loadFile()`, `streamLoad()` of PHP arrays, `PUT` and `GET`, presigned URLs.
- SQLSTATE mapping of server error codes, all three `PDO::ATTR_ERRMODE` modes, persistent connections.
- No leaks under `leaks --atExit` with and without the Zend allocator, and a build without compiler or linker warnings.

## Requirements

- PHP 8.4 or newer with `pdo` and `json`. Development and the test suite run on PHP 8.5; 8.4 builds against the same API but is not covered by the tests yet.
- Rust 1.88 or newer (`cargo` on `PATH`) to build the Rust part, and network access to crates.io on the first build. A prebuilt `liblake_ffi.a` can be passed instead, see [docs/building.md](docs/building.md).
- A C toolchain, `autoconf` and `phpize` (the `php-dev` / `php-devel` package on Linux).
- macOS or Linux.

## Install

With PECL, from a release tarball:

```sh
pecl install https://github.com/stringke/lake-php/releases/download/v0.1.0/pdo_lake-0.1.0.tgz
```

With [PIE](https://github.com/php/pie):

```sh
pie install stringke/lake-php
```

From source:

```sh
git clone https://github.com/stringke/lake-php.git
cd lake-php
phpize
./configure
make
make install
```

Then enable the extension after `pdo`:

```ini
extension=pdo_lake
```

`php -m | grep pdo_lake` and `php -r 'var_dump(PDO::getAvailableDrivers());'` confirm it is loaded. More options, prebuilt libraries and troubleshooting are in [docs/building.md](docs/building.md).

## Documentation

- [docs/usage.md](docs/usage.md): DSN, attributes, `Pdo\Lake` methods, type mapping, parameter binding, errors, loading data, Laravel and long-running workers.
- [docs/building.md](docs/building.md): every install path, configure options and troubleshooting.
- [docs/development.md](docs/development.md): repository layout, local Databend, tests and leak checks.
- [CHANGELOG.md](CHANGELOG.md)

## License

Apache License 2.0, see [LICENSE](LICENSE).
