/* C declarations of the Rust library in ffi/ (lake_ffi). Keep in sync with ffi/src/{abi,conn,result}.rs. */
#ifndef LAKE_FFI_H
#define LAKE_FFI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct lake_conn lake_conn;
typedef struct lake_result lake_result;

/* Borrowed input; ptr == NULL means "absent". */
typedef struct {
	const char *ptr;
	size_t len;
} lake_str;

/* Owned by the library, NUL terminated; release with lake_owned_free(). */
typedef struct {
	char *ptr;
	size_t len;
} lake_owned;

typedef struct {
	char sqlstate[6];
	int64_t code;
	lake_owned message;
} lake_error;

enum {
	LAKE_PARAM_NULL = 0,
	LAKE_PARAM_BOOL = 1,
	LAKE_PARAM_INT = 2,
	LAKE_PARAM_STRING = 3,
};

typedef struct {
	int32_t kind;
	int64_t int_value;
	const char *ptr;
	size_t len;
} lake_param;

enum {
	LAKE_CELL_NULL = 0,
	LAKE_CELL_BOOL = 1,
	LAKE_CELL_INT = 2,
	LAKE_CELL_FLOAT = 3,
	LAKE_CELL_STRING = 4,
	LAKE_CELL_BINARY = 5,
};

/* ptr/len are valid until the next lake_result_next() or lake_result_free(). */
typedef struct {
	int32_t kind;
	int64_t int_value;
	double float_value;
	const char *ptr;
	size_t len;
} lake_cell;

/* Strings are valid until lake_result_free(). */
typedef struct {
	const char *name;
	size_t name_len;
	const char *native_type;
	size_t native_type_len;
	bool nullable;
	int32_t pdo_type;
	int64_t precision;
	int64_t scale;
} lake_column;

typedef struct {
	uint64_t total_rows;
	uint64_t total_bytes;
	uint64_t read_rows;
	uint64_t read_bytes;
	uint64_t write_rows;
	uint64_t write_bytes;
	double running_time_ms;
	uint64_t spill_file_nums;
	uint64_t spill_bytes;
} lake_stats;

enum {
	LAKE_SET_WAREHOUSE = 0,
	LAKE_SET_DATABASE = 1,
	LAKE_SET_ROLE = 2,
	LAKE_SET_SESSION = 3,
};

enum {
	LAKE_LOAD_STAGE = 0,
	LAKE_LOAD_STREAMING = 1,
};

void lake_owned_free(lake_owned *owned);
void lake_error_clear(lake_error *error);
const char *lake_driver_version(void);

lake_conn *lake_connect(lake_str data_source, lake_str user, lake_str password, int64_t timeout_secs, lake_str client_name, lake_error *error);
void lake_conn_free(lake_conn *conn);

lake_result *lake_conn_query(const lake_conn *conn, lake_str sql, const lake_param *params, size_t count, lake_error *error);
bool lake_conn_exec(const lake_conn *conn, lake_str sql, lake_error *error);
bool lake_conn_server_side_params(const lake_conn *conn);
bool lake_conn_last_query_id(const lake_conn *conn, lake_owned *out);
bool lake_conn_kill_query(const lake_conn *conn, lake_str query_id, lake_error *error);
bool lake_conn_version(const lake_conn *conn, lake_owned *out, lake_error *error);
bool lake_conn_info(const lake_conn *conn, lake_owned *out, lake_error *error);
bool lake_conn_set(const lake_conn *conn, int32_t what, lake_str key, lake_str value, lake_error *error);
bool lake_conn_presign(const lake_conn *conn, lake_str operation, lake_str stage, lake_owned *out, lake_error *error);
bool lake_conn_upload_to_stage(const lake_conn *conn, lake_str stage, lake_str data, lake_error *error);
bool lake_conn_upload_file_to_stage(const lake_conn *conn, lake_str stage, lake_str path, lake_error *error);
bool lake_conn_load_data(const lake_conn *conn, lake_str sql, lake_str data, int32_t method, lake_stats *stats, lake_error *error);
bool lake_conn_load_file(const lake_conn *conn, lake_str sql, lake_str path, int32_t method, lake_str file_format_options, lake_str copy_options, lake_stats *stats, lake_error *error);
bool lake_conn_stream_load(const lake_conn *conn, lake_str sql, const lake_str *cells, size_t rows, size_t columns, int32_t method, lake_stats *stats, lake_error *error);
lake_result *lake_conn_put_files(const lake_conn *conn, lake_str local, lake_str stage, lake_error *error);
lake_result *lake_conn_get_files(const lake_conn *conn, lake_str stage, lake_str local, lake_error *error);

size_t lake_result_column_count(const lake_result *result);
bool lake_result_column(const lake_result *result, size_t index, lake_column *out);
int32_t lake_result_next(lake_result *result, lake_error *error);
/* Column count is 0 for DML; its affected rows come from lake_result_affected(). */
bool lake_result_affected(lake_result *result, int64_t *affected, lake_error *error);
void lake_result_close(lake_result *result);
bool lake_result_cell(const lake_result *result, size_t index, lake_cell *out);
void lake_result_stats(const lake_result *result, lake_stats *out);
bool lake_result_query_id(const lake_result *result, lake_str *out);
uint64_t lake_result_fetched(const lake_result *result);
void lake_result_free(lake_result *result);

#endif
