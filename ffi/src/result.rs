use std::ffi::c_char;

use lake_driver::{RowStatsIterator, RowWithStats, ServerStats};
use tokio_stream::StreamExt;

use crate::abi::{LakeError, LakeStr, guard, quietly, runtime};
use crate::cell::{CELL_INT, Cell, Column, column, to_cell};
use crate::error::Failure;

pub struct LakeResult {
    iter: Option<RowStatsIterator>,
    columns: Vec<Column>,
    row: Vec<Cell>,
    stats: ServerStats,
    query_id: Option<String>,
    fetched: u64,
    summary: bool,
}

/// DML answers with one row whose columns are named `number of rows inserted|updated|deleted`.
fn is_summary(columns: &[Column]) -> bool {
    !columns.is_empty() && columns.iter().all(|c| c.name.starts_with(b"number of rows "))
}

impl LakeResult {
    pub fn new(iter: RowStatsIterator, query_id: Option<String>) -> Self {
        let columns: Vec<Column> = iter.schema().fields().iter().map(column).collect();
        let summary = is_summary(&columns);
        Self { iter: Some(iter), columns, row: Vec::new(), stats: ServerStats::default(), query_id, fetched: 0, summary }
    }

    fn advance(&mut self) -> Result<bool, Failure> {
        loop {
            let Some(iter) = self.iter.as_mut() else { return Ok(false) };
            match runtime().block_on(iter.next()) {
                None => {
                    self.finish();
                    return Ok(false);
                }
                Some(Err(e)) => {
                    self.finish();
                    return Err(Failure::from_driver(&e, false));
                }
                Some(Ok(RowWithStats::Stats(stats))) => self.stats = stats,
                Some(Ok(RowWithStats::Row(row))) => {
                    self.row.clear();
                    self.row.extend(row.into_iter().map(to_cell));
                    self.fetched += 1;
                    return Ok(true);
                }
            }
        }
    }

    /// Affected rows of a statement without a real result set: the DML summary when there is one, the write progress otherwise.
    fn affected(&mut self) -> Result<i64, Failure> {
        let mut total = 0i64;
        let summary = self.summary;
        while self.advance()? {
            if summary {
                total += self.row.iter().filter(|c| c.kind == CELL_INT).map(|c| c.int).sum::<i64>();
            }
        }
        Ok(if summary { total } else { i64::try_from(self.stats.write_rows).unwrap_or(i64::MAX) })
    }

    fn finish(&mut self) {
        self.row.clear();
        if let Some(iter) = self.iter.take() {
            let _entered = runtime().enter();
            drop(iter);
        }
    }
}

impl Drop for LakeResult {
    fn drop(&mut self) {
        self.finish();
    }
}

#[repr(C)]
pub struct LakeColumn {
    pub name: *const c_char,
    pub name_len: usize,
    pub native_type: *const c_char,
    pub native_type_len: usize,
    pub nullable: bool,
    pub pdo_type: i32,
    pub precision: i64,
    pub scale: i64,
}

#[repr(C)]
pub struct LakeCell {
    pub kind: i32,
    pub int: i64,
    pub float: f64,
    pub ptr: *const u8,
    pub len: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct LakeStats {
    pub total_rows: u64,
    pub total_bytes: u64,
    pub read_rows: u64,
    pub read_bytes: u64,
    pub write_rows: u64,
    pub write_bytes: u64,
    pub running_time_ms: f64,
    pub spill_file_nums: u64,
    pub spill_bytes: u64,
}

impl From<&ServerStats> for LakeStats {
    fn from(s: &ServerStats) -> Self {
        Self {
            total_rows: s.total_rows as u64,
            total_bytes: s.total_bytes as u64,
            read_rows: s.read_rows as u64,
            read_bytes: s.read_bytes as u64,
            write_rows: s.write_rows as u64,
            write_bytes: s.write_bytes as u64,
            running_time_ms: s.running_time_ms,
            spill_file_nums: s.spill_file_nums as u64,
            spill_bytes: s.spill_bytes as u64,
        }
    }
}

pub fn write_stats(out: Option<&mut LakeStats>, stats: &ServerStats) {
    if let Some(out) = out {
        *out = LakeStats::from(stats);
    }
}

/// Number of result columns; 0 for DML, whose summary row is consumed through `lake_result_affected`.
#[unsafe(no_mangle)]
pub extern "C" fn lake_result_column_count(result: Option<&LakeResult>) -> usize {
    result.map_or(0, |r| if r.summary { 0 } else { r.columns.len() })
}

/// The strings stay valid until the result is freed.
#[unsafe(no_mangle)]
pub extern "C" fn lake_result_column(result: Option<&LakeResult>, index: usize, out: Option<&mut LakeColumn>) -> bool {
    let (Some(result), Some(out)) = (result, out) else { return false };
    let Some(c) = result.columns.get(index) else { return false };
    *out = LakeColumn { name: c.name.as_ptr().cast(), name_len: c.name.len() - 1, native_type: c.native_type.as_ptr().cast(), native_type_len: c.native_type.len() - 1, nullable: c.nullable, pdo_type: c.pdo_type, precision: c.precision, scale: c.scale };
    true
}

/// Returns 1 when a row is available, 0 at the end and -1 on error.
#[unsafe(no_mangle)]
pub extern "C" fn lake_result_next(result: Option<&mut LakeResult>, error: Option<&mut LakeError>) -> i32 {
    let Some(result) = result else { return 0 };
    guard(error, -1, || result.advance().map(i32::from))
}

/// Reads the rest of a statement without a result set and reports its affected rows.
#[unsafe(no_mangle)]
pub extern "C" fn lake_result_affected(result: Option<&mut LakeResult>, affected: Option<&mut i64>, error: Option<&mut LakeError>) -> bool {
    let Some(result) = result else { return true };
    guard(error, false, || {
        let rows = result.affected()?;
        if let Some(affected) = affected {
            *affected = rows;
        }
        Ok(true)
    })
}

/// Stops reading: the server-side query is released while columns, statistics and the query id stay readable.
#[unsafe(no_mangle)]
pub extern "C" fn lake_result_close(result: Option<&mut LakeResult>) {
    if let Some(result) = result {
        quietly(|| result.finish());
    }
}

/// Bytes stay valid until the next `lake_result_next`.
#[unsafe(no_mangle)]
pub extern "C" fn lake_result_cell(result: Option<&LakeResult>, index: usize, out: Option<&mut LakeCell>) -> bool {
    let (Some(result), Some(out)) = (result, out) else { return false };
    let Some(cell) = result.row.get(index) else { return false };
    *out = LakeCell { kind: cell.kind, int: cell.int, float: cell.float, ptr: cell.bytes.as_ptr(), len: cell.bytes.len() };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_result_stats(result: Option<&LakeResult>, out: Option<&mut LakeStats>) {
    if let Some(result) = result {
        write_stats(out, &result.stats);
    }
}

/// The id stays valid until the result is freed.
#[unsafe(no_mangle)]
pub extern "C" fn lake_result_query_id(result: Option<&LakeResult>, out: Option<&mut LakeStr>) -> bool {
    let (Some(result), Some(out)) = (result, out) else { return false };
    let Some(id) = result.query_id.as_deref() else { return false };
    *out = LakeStr { ptr: id.as_ptr(), len: id.len() };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_result_fetched(result: Option<&LakeResult>) -> u64 {
    result.map_or(0, |r| r.fetched)
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_result_free(result: Option<Box<LakeResult>>) {
    if let Some(result) = result {
        quietly(move || drop(result));
    }
}
