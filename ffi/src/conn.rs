use std::collections::BTreeMap;
use std::path::Path;

use lake_driver::{Client, Connection, LoadMethod, ServerStats};

use crate::abi::{LakeError, LakeOwned, LakeStr, guard, quietly, runtime, slice, write_owned};
use crate::dsn::{Credentials, build_url};
use crate::error::{CONNECT_FAILED, Failure, GENERAL, INVALID_PARAM};
use crate::result::{LakeResult, LakeStats, write_stats};

pub struct LakeConn {
    conn: Connection,
}

#[repr(C)]
pub struct LakeParam {
    pub kind: i32,
    pub int: i64,
    pub ptr: *const u8,
    pub len: usize,
}

const PARAM_NULL: i32 = 0;
const PARAM_BOOL: i32 = 1;
const PARAM_INT: i32 = 2;
const PARAM_STRING: i32 = 3;

const SET_WAREHOUSE: i32 = 0;
const SET_DATABASE: i32 = 1;
const SET_ROLE: i32 = 2;
const SET_SESSION: i32 = 3;

fn driver_error(error: lake_driver::Error) -> Failure {
    Failure::from_driver(&error, false)
}

fn live(conn: Option<&LakeConn>) -> Result<&Connection, Failure> {
    conn.map(|c| &c.conn).ok_or_else(|| Failure::new(GENERAL, "connection is closed"))
}

/// # Safety
/// Every string parameter must point to `len` readable bytes.
unsafe fn json_params(params: &[LakeParam]) -> Result<Option<serde_json::Value>, Failure> {
    if params.is_empty() {
        return Ok(None);
    }
    let mut values = Vec::with_capacity(params.len());
    for (index, p) in params.iter().enumerate() {
        values.push(match p.kind {
            PARAM_NULL => serde_json::Value::Null,
            PARAM_BOOL => serde_json::Value::Bool(p.int != 0),
            PARAM_INT => serde_json::Value::from(p.int),
            PARAM_STRING => {
                let raw = LakeStr { ptr: p.ptr, len: p.len };
                serde_json::Value::String(unsafe { raw.text("parameter") }?.unwrap_or_default().to_owned())
            }
            other => return Err(Failure::new(INVALID_PARAM, format!("parameter {} has unknown kind {other}", index + 1))),
        });
    }
    Ok(Some(serde_json::Value::Array(values)))
}

fn load_method(method: i32) -> LoadMethod {
    if method == 1 { LoadMethod::Streaming } else { LoadMethod::Stage }
}

fn string_options(json: Option<&str>, what: &str) -> Result<Option<Vec<(String, String)>>, Failure> {
    let Some(json) = json else { return Ok(None) };
    let value: serde_json::Value = serde_json::from_str(json).map_err(|e| Failure::new(GENERAL, format!("{what} is not valid JSON: {e}")))?;
    let serde_json::Value::Object(map) = value else { return Err(Failure::new(GENERAL, format!("{what} must be a JSON object"))) };
    Ok(Some(map.into_iter().map(|(k, v)| (k, json_to_text(v))).collect()))
}

fn json_to_text(value: serde_json::Value) -> String {
    match value {
        serde_json::Value::String(s) => s,
        serde_json::Value::Null => "\\N".to_string(),
        other => other.to_string(),
    }
}

fn borrowed_map(options: &Option<Vec<(String, String)>>) -> Option<BTreeMap<&str, &str>> {
    options.as_ref().map(|o| o.iter().map(|(k, v)| (k.as_str(), v.as_str())).collect())
}

/// # Safety
/// Every `LakeStr` must be null or point to `len` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_connect(data_source: LakeStr, user: LakeStr, password: LakeStr, timeout_secs: i64, client_name: LakeStr, error: Option<&mut LakeError>) -> Option<Box<LakeConn>> {
    guard(error, None, || {
        let (data_source, user, password, client_name) = unsafe { (data_source.required("DSN")?, user.text("user name")?, password.text("password")?, client_name.text("client name")?) };
        let url = build_url(data_source, Credentials { user, password }, timeout_secs).map_err(|m| Failure::new(CONNECT_FAILED, m))?;
        let mut client = Client::new(url);
        if let Some(name) = client_name {
            client = client.with_name(name.to_owned());
        }
        let conn = runtime().block_on(client.get_conn()).map_err(|e| Failure::from_driver(&e, true))?;
        Ok(Some(Box::new(LakeConn { conn })))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_conn_free(conn: Option<Box<LakeConn>>) {
    let Some(conn) = conn else { return };
    quietly(move || {
        let runtime = runtime();
        let _ = runtime.block_on(conn.conn.close());
        let _entered = runtime.enter();
        drop(conn);
    });
}

/// Starts a statement and returns its result stream; `params` travel as server-side parameters.
///
/// # Safety
/// `sql` must be readable and `params` null or `count` readable items whose strings are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_query(conn: Option<&LakeConn>, sql: LakeStr, params: *const LakeParam, count: usize, error: Option<&mut LakeError>) -> Option<Box<LakeResult>> {
    guard(error, None, || {
        let conn = live(conn)?;
        let (sql, params) = unsafe { (sql.required("SQL")?, json_params(slice(params, count))?) };
        if params.is_some() && !conn.inner().supports_server_side_params() {
            return Err(Failure::new(INVALID_PARAM, "server does not support server-side parameters; bind through PDO emulation"));
        }
        let iter = match params {
            Some(p) => runtime().block_on(conn.inner().query_iter_ext_with_params(sql, Some(p))),
            None => runtime().block_on(conn.query_iter_ext(sql)),
        }
        .map_err(driver_error)?;
        Ok(Some(Box::new(LakeResult::new(iter, conn.last_query_id()))))
    })
}

/// Runs a statement whose result is not needed (transaction control).
///
/// # Safety
/// `sql` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_exec(conn: Option<&LakeConn>, sql: LakeStr, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let sql = unsafe { sql.required("SQL") }?;
        runtime().block_on(conn.exec(sql).execute()).map_err(driver_error)?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_conn_server_side_params(conn: Option<&LakeConn>) -> bool {
    conn.is_some_and(|c| c.conn.inner().supports_server_side_params())
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_conn_last_query_id(conn: Option<&LakeConn>, out: Option<&mut LakeOwned>) -> bool {
    let Some(id) = conn.and_then(|c| c.conn.last_query_id()) else { return false };
    write_owned(out, id);
    true
}

/// # Safety
/// `query_id` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_kill_query(conn: Option<&LakeConn>, query_id: LakeStr, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let id = unsafe { query_id.required("query id") }?;
        runtime().block_on(conn.kill_query(id)).map_err(driver_error)?;
        Ok(true)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_conn_version(conn: Option<&LakeConn>, out: Option<&mut LakeOwned>, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let version = runtime().block_on(live(conn)?.version()).map_err(driver_error)?;
        write_owned(out, version);
        Ok(true)
    })
}

/// Writes the connection info as a JSON object.
#[unsafe(no_mangle)]
pub extern "C" fn lake_conn_info(conn: Option<&LakeConn>, out: Option<&mut LakeOwned>, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let info = runtime().block_on(conn.info());
        let json = serde_json::json!({
            "handler": info.handler,
            "host": info.host,
            "port": info.port,
            "user": info.user,
            "catalog": info.catalog,
            "database": info.database,
            "warehouse": info.warehouse,
            "server_side_params": conn.inner().supports_server_side_params(),
        });
        write_owned(out, json.to_string());
        Ok(true)
    })
}

fn identifier(name: &str) -> String {
    format!("`{}`", name.replace('`', "``"))
}

fn literal(value: &str) -> String {
    format!("'{}'", value.replace('\\', "\\\\").replace('\'', "\\'"))
}

/// Changes the warehouse (0), database (1), role (2) or a session setting (3) used by later statements.
/// Database, role and settings go through SQL so the server validates them and FlightSQL sessions follow too;
/// the warehouse is a RestAPI request header with no SQL form.
///
/// # Safety
/// `key` and `value` must be null or readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_set(conn: Option<&LakeConn>, what: i32, key: LakeStr, value: LakeStr, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let (key, value) = unsafe { (key.text("setting name")?, value.required("value")?) };
        let sql = match what {
            SET_WAREHOUSE => {
                conn.set_warehouse(value).map_err(driver_error)?;
                return Ok(true);
            }
            SET_DATABASE => format!("USE {}", identifier(value)),
            SET_ROLE => format!("SET ROLE {}", identifier(value)),
            SET_SESSION => {
                let name = key.filter(|k| !k.is_empty() && k.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_')).ok_or_else(|| Failure::new(GENERAL, "setting name must be letters, digits and underscores"))?;
                format!("SET {name} = {}", literal(value))
            }
            other => return Err(Failure::new(GENERAL, format!("unknown setting kind {other}"))),
        };
        runtime().block_on(conn.exec(&sql).execute()).map_err(driver_error)?;
        Ok(true)
    })
}

/// Writes `{"method":..,"headers":{..},"url":..}` for a presigned UPLOAD or DOWNLOAD.
///
/// # Safety
/// `operation` and `stage` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_presign(conn: Option<&LakeConn>, operation: LakeStr, stage: LakeStr, out: Option<&mut LakeOwned>, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let (operation, stage) = unsafe { (operation.required("operation")?, stage.required("stage location")?) };
        let operation = operation.to_ascii_uppercase();
        if operation != "UPLOAD" && operation != "DOWNLOAD" {
            return Err(Failure::new(GENERAL, "presign operation must be UPLOAD or DOWNLOAD"));
        }
        let presigned = runtime().block_on(conn.get_presigned_url(&operation, stage)).map_err(driver_error)?;
        write_owned(out, serde_json::json!({ "method": presigned.method, "headers": presigned.headers, "url": presigned.url }).to_string());
        Ok(true)
    })
}

/// The data is copied once: the upload reader must be `'static` because the HTTP body may outlive a timed-out call.
///
/// # Safety
/// `stage` and `data` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_upload_to_stage(conn: Option<&LakeConn>, stage: LakeStr, data: LakeStr, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let (stage, bytes) = unsafe { (stage.required("stage location")?, data.bytes().unwrap_or_default().to_vec()) };
        let size = bytes.len() as u64;
        runtime().block_on(conn.upload_to_stage(stage, Box::new(std::io::Cursor::new(bytes)), size)).map_err(driver_error)?;
        Ok(true)
    })
}

/// Streams the file from disk.
///
/// # Safety
/// `stage` and `path` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_upload_file_to_stage(conn: Option<&LakeConn>, stage: LakeStr, path: LakeStr, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let (stage, path) = unsafe { (stage.required("stage location")?, path.required("file path")?) };
        runtime()
            .block_on(async {
                let file = tokio::fs::File::open(path).await?;
                let size = file.metadata().await?.len();
                conn.upload_to_stage(stage, Box::new(tokio::io::BufReader::new(file)), size).await
            })
            .map_err(driver_error)?;
        Ok(true)
    })
}

/// Loads `data` through `sql` (`INSERT INTO t VALUES` or `... FROM @_databend_load ...`); method 0 = stage, 1 = streaming.
///
/// # Safety
/// `sql` and `data` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_load_data(conn: Option<&LakeConn>, sql: LakeStr, data: LakeStr, method: i32, stats: Option<&mut LakeStats>, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let (sql, bytes) = unsafe { (sql.required("SQL")?, data.bytes().unwrap_or_default().to_vec()) };
        let size = bytes.len() as u64;
        let result = runtime().block_on(conn.load_data(sql, Box::new(std::io::Cursor::new(bytes)), size, load_method(method))).map_err(driver_error)?;
        write_stats(stats, &result);
        Ok(true)
    })
}

/// Loads a local file; with either options JSON object present it uses stage + COPY with those options.
///
/// # Safety
/// `sql` and `path` must be readable; the option strings null or readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_load_file(conn: Option<&LakeConn>, sql: LakeStr, path: LakeStr, method: i32, file_format_options: LakeStr, copy_options: LakeStr, stats: Option<&mut LakeStats>, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let (sql, path, format, copy) = unsafe { (sql.required("SQL")?, path.required("file path")?, file_format_options.text("file format options")?, copy_options.text("copy options")?) };
        let format = string_options(format, "file format options")?;
        let copy = string_options(copy, "copy options")?;
        let path = Path::new(path);
        let result: ServerStats = if format.is_some() || copy.is_some() { runtime().block_on(conn.load_file_with_options(sql, path, borrowed_map(&format), borrowed_map(&copy))) } else { runtime().block_on(conn.load_file(sql, path, load_method(method))) }.map_err(driver_error)?;
        write_stats(stats, &result);
        Ok(true)
    })
}

/// Loads `rows * columns` cells laid out row by row; a null cell is sent as `\N`.
/// The cells are borrowed for the duration of the call, the driver encodes them to CSV itself.
///
/// # Safety
/// `sql` must be readable; `cells` null or `rows * columns` items that are null or readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_stream_load(conn: Option<&LakeConn>, sql: LakeStr, cells: *const LakeStr, rows: usize, columns: usize, method: i32, stats: Option<&mut LakeStats>, error: Option<&mut LakeError>) -> bool {
    guard(error, false, || {
        let conn = live(conn)?;
        let total = rows.checked_mul(columns).ok_or_else(|| Failure::new(GENERAL, "too many cells"))?;
        let (sql, cells) = unsafe { (sql.required("SQL")?, slice(cells, total)) };
        let mut texts = Vec::with_capacity(total);
        for cell in cells {
            texts.push(unsafe { cell.text("cell") }?.unwrap_or("\\N"));
        }
        let borrowed: Vec<Vec<&str>> = if columns == 0 { Vec::new() } else { texts.chunks(columns).map(<[&str]>::to_vec).collect() };
        let result = runtime().block_on(conn.stream_load(sql, borrowed, load_method(method))).map_err(driver_error)?;
        write_stats(stats, &result);
        Ok(true)
    })
}

/// `PUT file:///path/glob @stage/`; returns one row per file.
///
/// # Safety
/// `local` and `stage` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_put_files(conn: Option<&LakeConn>, local: LakeStr, stage: LakeStr, error: Option<&mut LakeError>) -> Option<Box<LakeResult>> {
    guard(error, None, || {
        let conn = live(conn)?;
        let (local, stage) = unsafe { (local.required("local file URL")?, stage.required("stage location")?) };
        let iter = runtime().block_on(conn.put_files(local, stage)).map_err(driver_error)?;
        Ok(Some(Box::new(LakeResult::new(iter, None))))
    })
}

/// `GET @stage/ file:///dir/`; returns one row per file.
///
/// # Safety
/// `stage` and `local` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lake_conn_get_files(conn: Option<&LakeConn>, stage: LakeStr, local: LakeStr, error: Option<&mut LakeError>) -> Option<Box<LakeResult>> {
    guard(error, None, || {
        let conn = live(conn)?;
        let (stage, local) = unsafe { (stage.required("stage location")?, local.required("local directory URL")?) };
        let iter = runtime().block_on(conn.get_files(stage, local)).map_err(driver_error)?;
        Ok(Some(Box::new(LakeResult::new(iter, None))))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_driver_version() -> *const std::ffi::c_char {
    c"0.34.0".as_ptr()
}
