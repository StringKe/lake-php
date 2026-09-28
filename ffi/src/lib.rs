//! C ABI over the official `lake-driver` crate, consumed by the `pdo_lake` PHP extension.
//! The matching declarations live in `ext/lake_ffi.h`.

mod abi;
mod cell;
mod conn;
mod dsn;
mod error;
mod result;
