# Using pdo_lake

## Connecting

`PDO::connect()` returns a `Pdo\Lake` instance; `new PDO(...)` returns a plain `PDO` that supports everything except the `Pdo\Lake` methods.

```php
$db = PDO::connect('lake:host=lake.example.com;port=443;dbname=sales;warehouse=etl', $user, $password, [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_TIMEOUT => 10,
]);
```

### DSN

The part after `lake:` takes one of two forms.

`key=value` pairs separated by `;`:

| Key | Meaning |
|---|---|
| `host` | Required. Server host name or address. |
| `port` | Port. The official driver's default is used when omitted. |
| `dbname`, `database` | Current database after connecting. |
| `user`, `password` | Credentials. The `$username` and `$password` arguments of the constructor take precedence. |
| `protocol` | `http` (default, also `rest`) for the REST API, `flight` (also `flightsql`, `grpc`) for FlightSQL. |
| anything else | Passed through unchanged, for example `sslmode=disable`, `warehouse=etl`, `tenant=...`, `connect_timeout=5`, or any server setting such as `max_threads=8`. |

A full driver URL, as accepted by the official drivers:

```text
lake:lake://user:password@host:8000/database?sslmode=disable
lake:lake+flight://user:password@host:8900/database?sslmode=disable
lake:databend://user:password@host:8000/database
```

The accepted schemes are `lake`, `lake+http`, `lake+https`, `lake+flight`, `lake+grpc` and the same set with `databend` in place of `lake`. Credentials in the URL are overridden by the constructor arguments when those are not empty.

`PDO::ATTR_TIMEOUT` becomes `connect_timeout` unless the DSN already sets it.

### Persistent connections

`PDO::ATTR_PERSISTENT => true` keeps the connection, its Tokio runtime handle and its HTTP client alive for the life of the PHP worker. Liveness is checked with `SELECT 1` before a persistent connection is reused.

## Attributes

| Attribute | get | set | Value |
|---|---|---|---|
| `PDO::ATTR_CLIENT_VERSION` | yes | | Version of the bundled Rust driver, for example `0.34.0`. |
| `PDO::ATTR_SERVER_VERSION` | yes | | Server version string. |
| `PDO::ATTR_SERVER_INFO` | yes | | JSON with `handler`, `host`, `port`, `user`, `catalog`, `database`, `warehouse`, `server_side_params`. |
| `PDO::ATTR_EMULATE_PREPARES` | yes | yes | `true` when parameters are substituted on the client. Always `true` when the server cannot bind parameters. |
| `Pdo\Lake::ATTR_SERVER_SIDE_PARAMS` | yes | | Whether the server accepts bound parameters (Databend 1.2.900 and newer). |
| `Pdo\Lake::ATTR_DATABASE` | yes | yes | Current database. Setting it runs `USE`. |
| `Pdo\Lake::ATTR_WAREHOUSE` | yes | yes | Current warehouse (REST only). |
| `Pdo\Lake::ATTR_ROLE` | yes | yes | Current role, read with `current_role()`. Setting it runs `SET ROLE`. |
| `Pdo\Lake::ATTR_QUERY_ID` | statement | | Server query id of the statement's last execution. |
| `Pdo\Lake::ATTR_QUERY_STATS` | statement | | Array with `total_rows`, `total_bytes`, `read_rows`, `read_bytes`, `write_rows`, `write_bytes`, `running_time_ms`, `spill_file_nums`, `spill_bytes`. |

`lastInsertId()` is not supported and raises `IM001`; the server has no auto-increment ids.

## Pdo\Lake methods

| Method | Description |
|---|---|
| `lastQueryId(): ?string` | Query id of the last statement run on this connection. |
| `killQuery(string $queryId): bool` | Cancels a running query started by this connection. The REST kill endpoint is bound to the session that started the query; to cancel a query from another connection run `KILL QUERY '<id>'`. |
| `info(): array\|false` | The same data as `PDO::ATTR_SERVER_INFO`, decoded. |
| `useWarehouse(string $warehouse): bool` | Switches the warehouse. REST only; FlightSQL raises `HY000`. |
| `useDatabase(string $database): bool` | `USE` a database. An unknown database raises `3D000`. |
| `useRole(string $role): bool` | `SET ROLE`. |
| `setSessionSetting(string $name, string $value): bool` | `SET name = 'value'`. The name may contain letters, digits and `_` only. |
| `presign(string $operation, string $stage): array\|false` | `PRESIGN UPLOAD` or `PRESIGN DOWNLOAD`; returns `method`, `headers` and `url`. Needs storage that supports presigning (S3 compatible, not the local `fs` backend). |
| `uploadToStage(string $stage, string $data): bool` | Writes a string to a stage path such as `@my_stage/dir/a.csv`. |
| `uploadFileToStage(string $stage, string $filename): bool` | Same for a local file, streamed from disk. |
| `loadData(string $sql, string $data, int $method = Pdo\Lake::LOAD_STAGE): array\|false` | Runs `INSERT INTO t VALUES` / `REPLACE` with the given CSV payload. `LOAD_STAGE` uploads first and copies from the stage; `LOAD_STREAMING` streams the body with the request. Returns the statistics array. |
| `loadFile(string $sql, string $filename, int $method = Pdo\Lake::LOAD_STAGE, ?array $fileFormatOptions = null, ?array $copyOptions = null): array\|false` | Same for a local file. Format and copy options map to `FILE_FORMAT = (...)` and copy options, for example `['type' => 'TSV']` and `['purge' => 'true']`. |
| `streamLoad(string $sql, array $rows, int $method = Pdo\Lake::LOAD_STREAMING): array\|false` | Loads a PHP array of rows. Every row must have the same number of scalar cells; `null` becomes SQL NULL, `true` / `false` become `1` / `0`. A ragged or non-scalar row raises `ValueError` / `TypeError` before anything is sent. |
| `putFiles(string $localFiles, string $stage): array\|false` | `PUT` of local files (`file:///tmp/data/*.csv`) to a stage. Returns one row per file with `file`, `status`, `size`. |
| `getFiles(string $stage, string $localDirectory): array\|false` | `GET` of stage files into a local directory. Needs storage that supports presigning. |

## Type mapping

| Server type | PHP value |
|---|---|
| `Int8` .. `Int64`, `UInt8` .. `UInt32` | `int` |
| `UInt64` | `int`, or `string` when above `PHP_INT_MAX` |
| `Float32`, `Float64` | `float` |
| `Decimal` | `string`, exact |
| `Boolean` | `bool` |
| `String` | `string` |
| `Binary` | `string` with the raw bytes; `getColumnMeta()` reports `PDO::PARAM_LOB` |
| `Date`, `Timestamp` | `string` (`2026-09-28`, `2026-09-28 10:11:12.123456`) |
| `Array`, `Tuple`, `Map`, `Variant` | JSON text |
| `Bitmap` | placeholder text; select `bitmap_to_array(col)` to read it |
| `NULL` | `null` |

`PDO::ATTR_STRINGIFY_FETCHES` turns every non-null value into a string (`false` becomes `"0"`).

`getColumnMeta()` returns `name`, `native_type` (the server type, for example `Nullable(Int32)`), `pdo_type`, `precision`, `scale` for decimals and `flags` (`not_null`).

## Parameter binding

- `?` and `:name` placeholders are both supported, but not mixed in one statement (PDO raises `HY093`). A name may appear several times.
- With server-side parameters (Databend 1.2.900 and newer, and `PDO::ATTR_EMULATE_PREPARES` set to `false`) values travel separately from the SQL. Otherwise the driver substitutes quoted literals on the client. Both paths give the same results.
- `execute([...])` binds every value as a string, as in all PDO drivers; use `bindValue()` with `PARAM_INT`, `PARAM_BOOL` or `PARAM_NULL` for typed values.
- `PARAM_LOB` values are sent as `FROM_HEX('...')`, so arbitrary bytes round-trip unchanged. A statement with a LOB parameter always uses client-side substitution.
- A statement executed without bound parameters is sent exactly as written, so map literals such as `{'k':1}` are not taken for `:name` placeholders. When a statement does bind parameters, write such literals with a space after the colon (`{'k': 1}`).
- `PDO::quote()` escapes `\`, `'` and NUL; with `PDO::PARAM_LOB` it returns a `FROM_HEX` literal.

## Row counts

`rowCount()` is the affected row count for `INSERT`, `UPDATE`, `DELETE` and `MERGE`, and the number of rows fetched so far for `SELECT`. `REPLACE INTO` reports 0, as the server does not return a count for it.

## Errors

Server errors carry their numeric code in `errorInfo[1]` and the message in `errorInfo[2]`. The SQLSTATE in `errorInfo[0]` (and in `PDOException::getCode()` for query errors) is mapped as follows:

| Server code | Meaning | SQLSTATE |
|---|---|---|
| 1005, 1065 | syntax or semantic error | `42000` |
| 1008 | unknown function | `42000` |
| 1063 | permission denied | `42000` |
| 1025 | unknown table | `42S02` |
| 2302 | table already exists | `42S01` |
| 1003 | unknown database | `3D000` |
| 1006, 1046 | bad data, failed conversion, missing parameter | `22000` |
| 1303 | column count mismatch | `21S01` |
| any other | | `HY000` |

Rejected credentials (and HTTP 401 / 403) are `28000`. Transport failures (connection refused, TLS, timeouts, HTTP 5xx) are `08001` while connecting and `08S01` afterwards. Other HTTP errors are `HY000` with the HTTP status as the native code. As in `pdo_mysql`, an exception thrown while connecting has the native code in `getCode()`; the SQLSTATE is in `errorInfo[0]`.

All three error modes work: `ERRMODE_EXCEPTION` (the PHP 8 default), `ERRMODE_WARNING` and `ERRMODE_SILENT`.

## Transactions

`beginTransaction()`, `commit()` and `rollBack()` map to `BEGIN`, `COMMIT` and `ROLLBACK`, and `inTransaction()` reflects the state. See the server documentation for the isolation the warehouse provides.

## Laravel

The driver is a regular PDO driver, so a custom connection only needs a connector that builds the DSN. A minimal registration in a service provider:

```php
use Illuminate\Database\Connection;
use Illuminate\Database\Connectors\Connector;
use Illuminate\Database\Connectors\ConnectorInterface;

$this->app->bind('db.connector.lake', fn () => new class extends Connector implements ConnectorInterface {
    public function connect(array $config)
    {
        $dsn = "lake:host={$config['host']};port={$config['port']};dbname={$config['database']};sslmode=".($config['sslmode'] ?? 'enable');
        return $this->createConnection($dsn, $config, $this->getOptions($config));
    }
});

Connection::resolverFor('lake', fn ($pdo, $database, $prefix, $config) => new Connection($pdo, $database, $prefix, $config));
```

```php
// config/database.php
'lake' => [
    'driver' => 'lake',
    'host' => env('LAKE_HOST'),
    'port' => env('LAKE_PORT', 443),
    'database' => env('LAKE_DATABASE', 'default'),
    'username' => env('LAKE_USERNAME'),
    'password' => env('LAKE_PASSWORD'),
],
```

The base `Connection` grammar produces standard SQL; query builder features that depend on MySQL or PostgreSQL syntax (for example `upsert`) are not translated.

## Long-running workers

Octane, Swoole, RoadRunner and queue workers can keep connections open for their whole life. Each connection owns its HTTP client; all connections of a process share one Tokio runtime with a single worker thread, created on first use. A forked child gets its own runtime automatically, but sockets opened before the fork would be shared by parent and child, so open connections in the child after the fork (Octane and Laravel queue workers already do).
