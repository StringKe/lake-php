use lake_driver::{DataType, Field, NumberDataType, NumberValue, Value};

pub const CELL_NULL: i32 = 0;
pub const CELL_BOOL: i32 = 1;
pub const CELL_INT: i32 = 2;
pub const CELL_FLOAT: i32 = 3;
pub const CELL_STRING: i32 = 4;
pub const CELL_BINARY: i32 = 5;

const PDO_PARAM_NULL: i32 = 0;
const PDO_PARAM_INT: i32 = 1;
const PDO_PARAM_STR: i32 = 2;
const PDO_PARAM_LOB: i32 = 3;
const PDO_PARAM_BOOL: i32 = 5;

pub struct Cell {
    pub kind: i32,
    pub int: i64,
    pub float: f64,
    pub bytes: Vec<u8>,
}

impl Cell {
    fn scalar(kind: i32, int: i64, float: f64) -> Self {
        Self { kind, int, float, bytes: Vec::new() }
    }

    fn text(value: String) -> Self {
        Self { kind: CELL_STRING, int: 0, float: 0.0, bytes: value.into_bytes() }
    }
}

pub fn to_cell(value: Value) -> Cell {
    match value {
        Value::Null => Cell::scalar(CELL_NULL, 0, 0.0),
        Value::Boolean(b) => Cell::scalar(CELL_BOOL, i64::from(b), 0.0),
        Value::Binary(bytes) => Cell { kind: CELL_BINARY, int: 0, float: 0.0, bytes },
        Value::String(s) | Value::Bitmap(s) | Value::Variant(s) | Value::Interval(s) => Cell::text(s),
        Value::Number(n) => number_cell(n),
        other => match other.to_json_value() {
            serde_json::Value::String(s) => Cell::text(s),
            json => Cell::text(json.to_string()),
        },
    }
}

fn number_cell(n: NumberValue) -> Cell {
    match n {
        NumberValue::Int8(v) => Cell::scalar(CELL_INT, v.into(), 0.0),
        NumberValue::Int16(v) => Cell::scalar(CELL_INT, v.into(), 0.0),
        NumberValue::Int32(v) => Cell::scalar(CELL_INT, v.into(), 0.0),
        NumberValue::Int64(v) => Cell::scalar(CELL_INT, v, 0.0),
        NumberValue::UInt8(v) => Cell::scalar(CELL_INT, v.into(), 0.0),
        NumberValue::UInt16(v) => Cell::scalar(CELL_INT, v.into(), 0.0),
        NumberValue::UInt32(v) => Cell::scalar(CELL_INT, v.into(), 0.0),
        NumberValue::UInt64(v) => match i64::try_from(v) {
            Ok(v) => Cell::scalar(CELL_INT, v, 0.0),
            Err(_) => Cell::text(v.to_string()),
        },
        NumberValue::Float32(v) => Cell::scalar(CELL_FLOAT, 0, f64::from(v)),
        NumberValue::Float64(v) => Cell::scalar(CELL_FLOAT, 0, v),
        decimal => match Value::Number(decimal).to_json_value() {
            serde_json::Value::String(s) => Cell::text(s),
            json => Cell::text(json.to_string()),
        },
    }
}

pub struct Column {
    pub name: Vec<u8>,
    pub native_type: Vec<u8>,
    pub nullable: bool,
    pub pdo_type: i32,
    pub precision: i64,
    pub scale: i64,
}

pub fn column(field: &Field) -> Column {
    let (inner, nullable) = match &field.data_type {
        DataType::Nullable(inner) => (inner.as_ref(), true),
        other => (other, matches!(other, DataType::Null)),
    };
    let (precision, scale) = match inner {
        DataType::Decimal(d) => (i64::from(d.decimal_size().precision), i64::from(d.decimal_size().scale)),
        _ => (0, 0),
    };
    Column { name: nul_terminated(field.name.clone()), native_type: nul_terminated(field.data_type.to_string()), nullable, pdo_type: pdo_type(inner), precision, scale }
}

fn pdo_type(data_type: &DataType) -> i32 {
    match data_type {
        DataType::Null => PDO_PARAM_NULL,
        DataType::Boolean => PDO_PARAM_BOOL,
        DataType::Binary => PDO_PARAM_LOB,
        DataType::Number(NumberDataType::Float32 | NumberDataType::Float64 | NumberDataType::UInt64) => PDO_PARAM_STR,
        DataType::Number(_) => PDO_PARAM_INT,
        _ => PDO_PARAM_STR,
    }
}

pub fn nul_terminated(value: String) -> Vec<u8> {
    let mut bytes = value.into_bytes();
    bytes.push(0);
    bytes
}
