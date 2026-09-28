#ifndef PHP_PDO_LAKE_INT_H
#define PHP_PDO_LAKE_INT_H

#include "lake_ffi.h"

typedef struct {
	lake_conn *conn;
	lake_error err;
	bool emulate_prepares;
	bool server_params;
} pdo_lake_db_handle;

typedef struct {
	pdo_lake_db_handle *H;
	lake_result *result;
	zend_string *native_sql;
} pdo_lake_stmt;

enum {
	PDO_LAKE_ATTR_WAREHOUSE = PDO_ATTR_DRIVER_SPECIFIC,
	PDO_LAKE_ATTR_ROLE,
	PDO_LAKE_ATTR_DATABASE,
	PDO_LAKE_ATTR_SERVER_SIDE_PARAMS,
	PDO_LAKE_ATTR_QUERY_ID,
	PDO_LAKE_ATTR_QUERY_STATS,
};

extern const pdo_driver_t pdo_lake_driver;
extern const struct pdo_stmt_methods lake_stmt_methods;

/* Copies the pending lake_error into the PDO error slot of stmt (or dbh when stmt is NULL). */
void pdo_lake_raise(pdo_dbh_t *dbh, pdo_stmt_t *stmt);
bool pdo_lake_info(pdo_dbh_t *dbh, zval *out);
bool pdo_lake_set(pdo_dbh_t *dbh, int32_t what, lake_str key, lake_str value);

static inline void pdo_lake_clear(pdo_lake_db_handle *H)
{
	if (H->err.message.ptr || H->err.sqlstate[0]) {
		lake_error_clear(&H->err);
	}
}

static inline lake_str pdo_lake_str(const zend_string *s)
{
	lake_str out = { s ? ZSTR_VAL(s) : NULL, s ? ZSTR_LEN(s) : 0 };
	return out;
}

static inline lake_str pdo_lake_cstr(const char *s, size_t len)
{
	lake_str out = { s, len };
	return out;
}

static inline void pdo_lake_cell_zval(const lake_cell *c, zval *out)
{
	switch (c->kind) {
		case LAKE_CELL_BOOL:
			ZVAL_BOOL(out, c->int_value != 0);
			break;
		case LAKE_CELL_INT:
			ZVAL_LONG(out, (zend_long) c->int_value);
			break;
		case LAKE_CELL_FLOAT:
			ZVAL_DOUBLE(out, c->float_value);
			break;
		case LAKE_CELL_STRING:
		case LAKE_CELL_BINARY:
			ZVAL_STRINGL_FAST(out, c->ptr, c->len);
			break;
		default:
			ZVAL_NULL(out);
	}
}

static inline void pdo_lake_add_stats(zval *out, const lake_stats *s)
{
	array_init_size(out, 9);
	add_assoc_long(out, "total_rows", (zend_long) s->total_rows);
	add_assoc_long(out, "total_bytes", (zend_long) s->total_bytes);
	add_assoc_long(out, "read_rows", (zend_long) s->read_rows);
	add_assoc_long(out, "read_bytes", (zend_long) s->read_bytes);
	add_assoc_long(out, "write_rows", (zend_long) s->write_rows);
	add_assoc_long(out, "write_bytes", (zend_long) s->write_bytes);
	add_assoc_double(out, "running_time_ms", s->running_time_ms);
	add_assoc_long(out, "spill_file_nums", (zend_long) s->spill_file_nums);
	add_assoc_long(out, "spill_bytes", (zend_long) s->spill_bytes);
}

#endif
