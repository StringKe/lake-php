#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "php.h"
#include "ext/pdo/php_pdo.h"
#include "ext/pdo/php_pdo_driver.h"
#include "ext/pdo/php_pdo_error.h"
#include "php_pdo_lake_int.h"

#define LAKE_STACK_PARAMS 16

static void lake_stmt_release_result(pdo_lake_stmt *S)
{
	if (S->result) {
		lake_result_free(S->result);
		S->result = NULL;
	}
}

static int lake_stmt_dtor(pdo_stmt_t *stmt)
{
	pdo_lake_stmt *S = stmt->driver_data;

	lake_stmt_release_result(S);
	if (S->native_sql) {
		zend_string_release_ex(S->native_sql, 0);
	}
	efree(S);
	stmt->driver_data = NULL;
	return 1;
}

static bool lake_stmt_has_lob(const pdo_stmt_t *stmt)
{
	struct pdo_bound_param_data *param;

	if (!stmt->bound_params) {
		return false;
	}
	ZEND_HASH_FOREACH_PTR(stmt->bound_params, param) {
		if (PDO_PARAM_TYPE(param->param_type) == PDO_PARAM_LOB) {
			return true;
		}
	} ZEND_HASH_FOREACH_END();
	return false;
}

static struct pdo_bound_param_data *lake_stmt_param_at(const pdo_stmt_t *stmt, size_t position)
{
	struct pdo_bound_param_data *param = NULL;
	zend_string *name;

	if (!stmt->bound_params) {
		return NULL;
	}
	if (stmt->bound_param_map && (name = zend_hash_index_find_ptr(stmt->bound_param_map, position)) != NULL) {
		param = zend_hash_find_ptr(stmt->bound_params, name);
	}
	return param ? param : zend_hash_index_find_ptr(stmt->bound_params, position);
}

static size_t lake_stmt_param_count(const pdo_stmt_t *stmt)
{
	struct pdo_bound_param_data *param;
	size_t count = 0;

	if (stmt->bound_param_map) {
		return zend_hash_num_elements(stmt->bound_param_map);
	}
	if (stmt->bound_params) {
		ZEND_HASH_FOREACH_PTR(stmt->bound_params, param) {
			if (param->paramno >= 0 && (size_t) param->paramno >= count) {
				count = (size_t) param->paramno + 1;
			}
		} ZEND_HASH_FOREACH_END();
	}
	return count;
}

/* Strings are borrowed from the bound zval; only non-string values bound as PDO_PARAM_STR produce a temporary. */
static bool lake_stmt_fill_param(struct pdo_bound_param_data *param, lake_param *out, zend_string **temp)
{
	zval *value = &param->parameter;
	zend_string *str;

	ZVAL_DEREF(value);
	memset(out, 0, sizeof(*out));
	*temp = NULL;

	if (Z_TYPE_P(value) == IS_NULL || PDO_PARAM_TYPE(param->param_type) == PDO_PARAM_NULL) {
		out->kind = LAKE_PARAM_NULL;
		return true;
	}
	switch (PDO_PARAM_TYPE(param->param_type)) {
		case PDO_PARAM_BOOL:
			out->kind = LAKE_PARAM_BOOL;
			out->int_value = zend_is_true(value);
			return true;
		case PDO_PARAM_INT:
			out->kind = LAKE_PARAM_INT;
			out->int_value = zval_get_long(value);
			return true;
		default:
			if (Z_TYPE_P(value) == IS_STRING) {
				str = Z_STR_P(value);
			} else {
				str = zval_try_get_string(value);
				if (!str) {
					return false;
				}
				*temp = str;
			}
			out->kind = LAKE_PARAM_STRING;
			out->ptr = ZSTR_VAL(str);
			out->len = ZSTR_LEN(str);
			return true;
	}
}

static lake_result *lake_stmt_run_native(pdo_stmt_t *stmt, pdo_lake_stmt *S)
{
	lake_param stack_params[LAKE_STACK_PARAMS];
	zend_string *stack_temps[LAKE_STACK_PARAMS];
	size_t count = lake_stmt_param_count(stmt);
	lake_param *params = count > LAKE_STACK_PARAMS ? safe_emalloc(count, sizeof(lake_param), 0) : stack_params;
	zend_string **temps = count > LAKE_STACK_PARAMS ? safe_emalloc(count, sizeof(zend_string *), 0) : stack_temps;
	zend_string *sql = S->native_sql ? S->native_sql : stmt->query_string;
	lake_result *result = NULL;
	size_t filled = 0;

	for (; filled < count; filled++) {
		struct pdo_bound_param_data *param = lake_stmt_param_at(stmt, filled);

		if (!param) {
			char message[64];
			snprintf(message, sizeof(message), "parameter %zu was not bound", filled + 1);
			pdo_raise_impl_error(stmt->dbh, stmt, "HY093", message);
			goto done;
		}
		if (!lake_stmt_fill_param(param, &params[filled], &temps[filled])) {
			goto done;
		}
	}

	result = lake_conn_query(S->H->conn, pdo_lake_str(sql), params, count, &S->H->err);
	if (!result) {
		pdo_lake_raise(stmt->dbh, stmt);
	}

done:
	for (size_t i = 0; i < filled; i++) {
		if (temps[i]) {
			zend_string_release_ex(temps[i], 0);
		}
	}
	if (params != stack_params) {
		efree(params);
		efree(temps);
	}
	return result;
}

/* Binary values cannot travel as JSON server-side parameters, so such executions inline them through the quoter. */
static lake_result *lake_stmt_run_inlined(pdo_stmt_t *stmt, pdo_lake_stmt *S)
{
	const char *template = stmt->named_rewrite_template;
	zend_string *expanded = NULL;
	lake_result *result;
	int ret;

	stmt->supports_placeholders = PDO_PLACEHOLDER_NONE;
	stmt->named_rewrite_template = NULL;
	ret = pdo_parse_params(stmt, stmt->query_string, &expanded);
	stmt->supports_placeholders = PDO_PLACEHOLDER_POSITIONAL;
	stmt->named_rewrite_template = template;

	if (ret == -1) {
		return NULL;
	}
	result = lake_conn_query(S->H->conn, pdo_lake_str(ret == 1 ? expanded : stmt->query_string), NULL, 0, &S->H->err);
	if (!result) {
		pdo_lake_raise(stmt->dbh, stmt);
	}
	if (expanded) {
		zend_string_release_ex(expanded, 0);
	}
	return result;
}

static int lake_stmt_execute(pdo_stmt_t *stmt)
{
	pdo_lake_stmt *S = stmt->driver_data;
	pdo_lake_db_handle *H = S->H;
	size_t columns;

	lake_stmt_release_result(S);
	pdo_lake_clear(H);

	if (stmt->supports_placeholders == PDO_PLACEHOLDER_NONE) {
		S->result = lake_conn_query(H->conn, pdo_lake_str(stmt->active_query_string), NULL, 0, &H->err);
		if (!S->result) {
			pdo_lake_raise(stmt->dbh, stmt);
		}
	} else if (!stmt->bound_params || zend_hash_num_elements(stmt->bound_params) == 0) {
		/* Nothing bound: send the SQL untouched, as emulation does for PDO::query(), so `{'k':1}` map literals are not placeholders. */
		S->result = lake_conn_query(H->conn, pdo_lake_str(stmt->query_string), NULL, 0, &H->err);
		if (!S->result) {
			pdo_lake_raise(stmt->dbh, stmt);
		}
	} else if (lake_stmt_has_lob(stmt)) {
		S->result = lake_stmt_run_inlined(stmt, S);
	} else {
		S->result = lake_stmt_run_native(stmt, S);
	}
	if (!S->result) {
		return 0;
	}

	columns = lake_result_column_count(S->result);
	php_pdo_stmt_set_column_count(stmt, (int) columns);
	stmt->row_count = 0;

	if (columns == 0) {
		int64_t affected = 0;

		if (!lake_result_affected(S->result, &affected, &H->err)) {
			pdo_lake_raise(stmt->dbh, stmt);
			lake_stmt_release_result(S);
			return 0;
		}
		stmt->row_count = (zend_long) affected;
	}
	return 1;
}

static int lake_stmt_fetch(pdo_stmt_t *stmt, enum pdo_fetch_orientation ori, zend_long offset)
{
	pdo_lake_stmt *S = stmt->driver_data;
	int32_t ret;

	if (!S->result || stmt->column_count == 0) {
		return 0;
	}
	pdo_lake_clear(S->H);
	ret = lake_result_next(S->result, &S->H->err);
	if (ret < 0) {
		pdo_lake_raise(stmt->dbh, stmt);
		return 0;
	}
	stmt->row_count = (zend_long) lake_result_fetched(S->result);
	return ret;
}

static int lake_stmt_describe(pdo_stmt_t *stmt, int colno)
{
	pdo_lake_stmt *S = stmt->driver_data;
	struct pdo_column_data *col = &stmt->columns[colno];
	lake_column c;

	if (!S->result || !lake_result_column(S->result, (size_t) colno, &c)) {
		return 0;
	}
	col->name = zend_string_init(c.name, c.name_len, 0);
	col->maxlen = SIZE_MAX;
	col->precision = c.precision > 0 ? (zend_ulong) c.precision : 0;
	return 1;
}

static int lake_stmt_get_col(pdo_stmt_t *stmt, int colno, zval *result, enum pdo_param_type *type)
{
	pdo_lake_stmt *S = stmt->driver_data;
	lake_cell c;

	if (!S->result || !lake_result_cell(S->result, (size_t) colno, &c)) {
		ZVAL_NULL(result);
		return 1;
	}
	pdo_lake_cell_zval(&c, result);
	return 1;
}

static int lake_stmt_get_column_meta(pdo_stmt_t *stmt, zend_long colno, zval *return_value)
{
	pdo_lake_stmt *S = stmt->driver_data;
	lake_column c;
	zval flags;

	if (!S->result || colno < 0 || !lake_result_column(S->result, (size_t) colno, &c)) {
		return FAILURE;
	}
	array_init(return_value);
	add_assoc_stringl(return_value, "native_type", c.native_type, c.native_type_len);
	add_assoc_long(return_value, "pdo_type", c.pdo_type);
	if (c.scale > 0) {
		add_assoc_long(return_value, "scale", (zend_long) c.scale);
	}
	array_init(&flags);
	if (!c.nullable) {
		add_next_index_string(&flags, "not_null");
	}
	add_assoc_zval(return_value, "flags", &flags);
	return SUCCESS;
}

static int lake_stmt_cursor_closer(pdo_stmt_t *stmt)
{
	pdo_lake_stmt *S = stmt->driver_data;

	lake_result_close(S->result);
	return 1;
}

static int lake_stmt_get_attr(pdo_stmt_t *stmt, zend_long attr, zval *val)
{
	pdo_lake_stmt *S = stmt->driver_data;
	lake_str id;
	lake_stats stats;

	switch (attr) {
		case PDO_LAKE_ATTR_QUERY_ID:
			if (S->result && lake_result_query_id(S->result, &id)) {
				ZVAL_STRINGL(val, id.ptr, id.len);
			} else {
				ZVAL_NULL(val);
			}
			return 1;
		case PDO_LAKE_ATTR_QUERY_STATS:
			if (S->result) {
				lake_result_stats(S->result, &stats);
				pdo_lake_add_stats(val, &stats);
			} else {
				ZVAL_NULL(val);
			}
			return 1;
		default:
			return 0;
	}
}

const struct pdo_stmt_methods lake_stmt_methods = {
	lake_stmt_dtor,
	lake_stmt_execute,
	lake_stmt_fetch,
	lake_stmt_describe,
	lake_stmt_get_col,
	NULL, /* param_hook */
	NULL, /* set_attribute */
	lake_stmt_get_attr,
	lake_stmt_get_column_meta,
	NULL, /* next_rowset */
	lake_stmt_cursor_closer
};
