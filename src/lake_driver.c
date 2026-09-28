#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "php.h"
#include "ext/json/php_json.h"
#include "ext/pdo/php_pdo.h"
#include "ext/pdo/php_pdo_driver.h"
#include "ext/pdo/php_pdo_error.h"
#include "php_pdo_lake.h"
#include "php_pdo_lake_int.h"

void pdo_lake_raise(pdo_dbh_t *dbh, pdo_stmt_t *stmt)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	pdo_error_type *slot = stmt ? &stmt->error_code : &dbh->error_code;
	const char *state = H->err.sqlstate[0] ? H->err.sqlstate : "HY000";

	memcpy(*slot, state, 5);
	(*slot)[5] = '\0';

	if (!dbh->methods) {
		pdo_throw_exception((unsigned int) H->err.code, H->err.message.ptr ? H->err.message.ptr : "unknown error", &dbh->error_code);
	}
}

bool pdo_lake_info(pdo_dbh_t *dbh, zval *out)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	lake_owned json = {0};
	bool ok;

	if (!lake_conn_info(H->conn, &json, &H->err)) {
		pdo_lake_raise(dbh, NULL);
		return false;
	}
	ok = php_json_decode_ex(out, json.ptr, json.len, PHP_JSON_OBJECT_AS_ARRAY, PHP_JSON_PARSER_DEFAULT_DEPTH) == SUCCESS;
	lake_owned_free(&json);
	return ok;
}

static void lake_handle_closer(pdo_dbh_t *dbh)
{
	pdo_lake_db_handle *H = dbh->driver_data;

	if (!H) {
		return;
	}
	if (H->conn) {
		lake_conn_free(H->conn);
		H->conn = NULL;
	}
	lake_error_clear(&H->err);
	pefree(H, dbh->is_persistent);
	dbh->driver_data = NULL;
}

static bool lake_handle_preparer(pdo_dbh_t *dbh, zend_string *sql, pdo_stmt_t *stmt, zval *driver_options)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	pdo_lake_stmt *S = ecalloc(1, sizeof(*S));
	zend_string *nsql = NULL;
	int ret;

	S->H = H;
	stmt->driver_data = S;
	stmt->methods = &lake_stmt_methods;

	if (!H->server_params || pdo_attr_lval(driver_options, PDO_ATTR_EMULATE_PREPARES, H->emulate_prepares)) {
		stmt->supports_placeholders = PDO_PLACEHOLDER_NONE;
		return true;
	}

	stmt->supports_placeholders = PDO_PLACEHOLDER_POSITIONAL;
	ret = pdo_parse_params(stmt, sql, &nsql);
	if (ret == -1) {
		strcpy(dbh->error_code, stmt->error_code);
		return false;
	}
	if (ret == 1) {
		S->native_sql = nsql;
	}
	if (stmt->bound_param_map) {
		/* The positions are resolved from bound_param_map at execute time, so one :name may appear several times. */
		stmt->named_rewrite_template = "?";
	}
	return true;
}

static zend_long lake_handle_doer(pdo_dbh_t *dbh, const zend_string *sql)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	lake_result *result;
	int64_t affected = 0;
	bool ok;

	pdo_lake_clear(H);
	result = lake_conn_query(H->conn, pdo_lake_str(sql), NULL, 0, &H->err);
	if (!result) {
		pdo_lake_raise(dbh, NULL);
		return -1;
	}
	ok = lake_result_affected(result, &affected, &H->err);
	lake_result_free(result);
	if (!ok) {
		pdo_lake_raise(dbh, NULL);
		return -1;
	}
	return (zend_long) affected;
}

static zend_string *lake_quote_binary(const zend_string *unquoted)
{
	static const char hex[] = "0123456789abcdef";
	const unsigned char *src = (const unsigned char *) ZSTR_VAL(unquoted);
	size_t len = ZSTR_LEN(unquoted);
	zend_string *out = zend_string_safe_alloc(2, len, sizeof("FROM_HEX('')") - 1, 0);
	char *d = ZSTR_VAL(out);

	memcpy(d, "FROM_HEX('", sizeof("FROM_HEX('") - 1);
	d += sizeof("FROM_HEX('") - 1;
	for (size_t i = 0; i < len; i++) {
		*d++ = hex[src[i] >> 4];
		*d++ = hex[src[i] & 0x0f];
	}
	*d++ = '\'';
	*d++ = ')';
	*d = '\0';
	return out;
}

static zend_string *lake_handle_quoter(pdo_dbh_t *dbh, const zend_string *unquoted, enum pdo_param_type paramtype)
{
	const char *src = ZSTR_VAL(unquoted);
	size_t len = ZSTR_LEN(unquoted);
	size_t extra = 0;
	zend_string *out;
	char *d;

	if (PDO_PARAM_TYPE(paramtype) == PDO_PARAM_LOB) {
		return lake_quote_binary(unquoted);
	}

	for (size_t i = 0; i < len; i++) {
		extra += src[i] == '\\' || src[i] == '\'' || src[i] == '\0';
	}

	out = zend_string_safe_alloc(1, len, extra + 2, 0);
	d = ZSTR_VAL(out);
	*d++ = '\'';
	if (!extra) {
		memcpy(d, src, len);
		d += len;
	} else {
		for (size_t i = 0; i < len; i++) {
			switch (src[i]) {
				case '\\': *d++ = '\\'; *d++ = '\\'; break;
				case '\'': *d++ = '\\'; *d++ = '\''; break;
				case '\0': *d++ = '\\'; *d++ = '0'; break;
				default: *d++ = src[i];
			}
		}
	}
	*d++ = '\'';
	*d = '\0';
	return out;
}

static bool lake_exec_simple(pdo_dbh_t *dbh, const char *sql, size_t len)
{
	pdo_lake_db_handle *H = dbh->driver_data;

	pdo_lake_clear(H);
	if (!lake_conn_exec(H->conn, pdo_lake_cstr(sql, len), &H->err)) {
		pdo_lake_raise(dbh, NULL);
		return false;
	}
	return true;
}

static bool lake_handle_begin(pdo_dbh_t *dbh)
{
	return lake_exec_simple(dbh, ZEND_STRL("BEGIN"));
}

static bool lake_handle_commit(pdo_dbh_t *dbh)
{
	return lake_exec_simple(dbh, ZEND_STRL("COMMIT"));
}

static bool lake_handle_rollback(pdo_dbh_t *dbh)
{
	return lake_exec_simple(dbh, ZEND_STRL("ROLLBACK"));
}

bool pdo_lake_set(pdo_dbh_t *dbh, int32_t what, lake_str key, lake_str value)
{
	pdo_lake_db_handle *H = dbh->driver_data;

	pdo_lake_clear(H);
	if (!lake_conn_set(H->conn, what, key, value, &H->err)) {
		pdo_lake_raise(dbh, NULL);
		return false;
	}
	return true;
}

static bool lake_set_string_attr(pdo_dbh_t *dbh, int32_t what, const char *name, zval *val)
{
	lake_str none = {NULL, 0};

	if (Z_TYPE_P(val) != IS_STRING) {
		zend_type_error("Pdo\\Lake::%s value must be of type string, %s given", name, zend_zval_value_name(val));
		return false;
	}
	return pdo_lake_set(dbh, what, none, pdo_lake_str(Z_STR_P(val)));
}

static bool lake_handle_set_attr(pdo_dbh_t *dbh, zend_long attr, zval *val)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	bool bval;

	switch (attr) {
		case PDO_ATTR_EMULATE_PREPARES:
			if (!pdo_get_bool_param(&bval, val)) {
				return false;
			}
			H->emulate_prepares = bval;
			return true;
		case PDO_LAKE_ATTR_WAREHOUSE:
			return lake_set_string_attr(dbh, LAKE_SET_WAREHOUSE, "ATTR_WAREHOUSE", val);
		case PDO_LAKE_ATTR_ROLE:
			return lake_set_string_attr(dbh, LAKE_SET_ROLE, "ATTR_ROLE", val);
		case PDO_LAKE_ATTR_DATABASE:
			return lake_set_string_attr(dbh, LAKE_SET_DATABASE, "ATTR_DATABASE", val);
		default:
			return false;
	}
}

static int lake_info_field(pdo_dbh_t *dbh, const char *key, size_t key_len, zval *val)
{
	zval info, *field;

	if (!pdo_lake_info(dbh, &info)) {
		return -1;
	}
	field = Z_TYPE(info) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL(info), key, key_len) : NULL;
	if (field) {
		ZVAL_COPY(val, field);
	} else {
		ZVAL_NULL(val);
	}
	zval_ptr_dtor(&info);
	return 1;
}

static int lake_scalar(pdo_dbh_t *dbh, const char *sql, size_t sql_len, zval *val)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	lake_result *result;
	lake_cell cell;
	int32_t more;

	pdo_lake_clear(H);
	result = lake_conn_query(H->conn, pdo_lake_cstr(sql, sql_len), NULL, 0, &H->err);
	if (!result) {
		pdo_lake_raise(dbh, NULL);
		return -1;
	}
	more = lake_result_next(result, &H->err);
	if (more < 0) {
		lake_result_free(result);
		pdo_lake_raise(dbh, NULL);
		return -1;
	}
	if (more == 1 && lake_result_cell(result, 0, &cell)) {
		pdo_lake_cell_zval(&cell, val);
	} else {
		ZVAL_NULL(val);
	}
	lake_result_free(result);
	return 1;
}

static int lake_handle_get_attr(pdo_dbh_t *dbh, zend_long attr, zval *val)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	lake_owned text = {0};

	switch (attr) {
		case PDO_ATTR_EMULATE_PREPARES:
			ZVAL_BOOL(val, H->emulate_prepares || !H->server_params);
			return 1;
		case PDO_ATTR_CLIENT_VERSION:
			ZVAL_STRING(val, lake_driver_version());
			return 1;
		case PDO_ATTR_SERVER_VERSION:
			pdo_lake_clear(H);
			if (!lake_conn_version(H->conn, &text, &H->err)) {
				pdo_lake_raise(dbh, NULL);
				return -1;
			}
			ZVAL_STRINGL(val, text.ptr, text.len);
			lake_owned_free(&text);
			return 1;
		case PDO_ATTR_SERVER_INFO:
			pdo_lake_clear(H);
			if (!lake_conn_info(H->conn, &text, &H->err)) {
				pdo_lake_raise(dbh, NULL);
				return -1;
			}
			ZVAL_STRINGL(val, text.ptr, text.len);
			lake_owned_free(&text);
			return 1;
		case PDO_LAKE_ATTR_WAREHOUSE:
			return lake_info_field(dbh, ZEND_STRL("warehouse"), val);
		case PDO_LAKE_ATTR_DATABASE:
			return lake_info_field(dbh, ZEND_STRL("database"), val);
		case PDO_LAKE_ATTR_ROLE:
			return lake_scalar(dbh, ZEND_STRL("SELECT current_role()"), val);
		case PDO_LAKE_ATTR_SERVER_SIDE_PARAMS:
			ZVAL_BOOL(val, H->server_params);
			return 1;
		default:
			return 0;
	}
}

static void lake_handle_fetch_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info)
{
	pdo_lake_db_handle *H = dbh->driver_data;

	if (H && H->err.message.ptr) {
		add_next_index_long(info, (zend_long) H->err.code);
		add_next_index_stringl(info, H->err.message.ptr, H->err.message.len);
	}
}

static zend_result lake_handle_check_liveness(pdo_dbh_t *dbh)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	bool alive;

	if (!H || !H->conn) {
		return FAILURE;
	}
	alive = lake_conn_exec(H->conn, pdo_lake_cstr(ZEND_STRL("SELECT 1")), &H->err);
	pdo_lake_clear(H);
	return alive ? SUCCESS : FAILURE;
}

static const struct pdo_dbh_methods lake_methods = {
	lake_handle_closer,
	lake_handle_preparer,
	lake_handle_doer,
	lake_handle_quoter,
	lake_handle_begin,
	lake_handle_commit,
	lake_handle_rollback,
	lake_handle_set_attr,
	NULL, /* last_id: Lake has no auto-increment ids */
	lake_handle_fetch_error,
	lake_handle_get_attr,
	lake_handle_check_liveness,
	NULL, /* get_driver_methods: extensions live on Pdo\Lake and statement attributes */
	NULL, /* persistent_shutdown */
	NULL, /* in_transaction: PDO tracks it */
	NULL, /* get_gc */
	NULL  /* scanner */
};

static int pdo_lake_handle_factory(pdo_dbh_t *dbh, zval *driver_options)
{
	pdo_lake_db_handle *H = pecalloc(1, sizeof(*H), dbh->is_persistent);
	zend_long timeout = pdo_attr_lval(driver_options, PDO_ATTR_TIMEOUT, 0);
	lake_str user = {dbh->username, dbh->username ? strlen(dbh->username) : 0};
	lake_str password = {dbh->password, dbh->password ? strlen(dbh->password) : 0};
	lake_str client_name = pdo_lake_cstr(ZEND_STRL("lake-php/" PHP_PDO_LAKE_VERSION));
	int ok = 0;

	dbh->driver_data = H;
	dbh->alloc_own_columns = 1;
	dbh->max_escaped_char_length = 2;

	H->conn = lake_connect(pdo_lake_cstr(dbh->data_source, dbh->data_source_len), user, password, timeout, client_name, &H->err);
	if (!H->conn) {
		pdo_lake_raise(dbh, NULL);
		goto cleanup;
	}
	H->server_params = lake_conn_server_side_params(H->conn);
	ok = 1;

cleanup:
	dbh->methods = &lake_methods;
	if (!ok) {
		lake_handle_closer(dbh);
	}
	return ok;
}

const pdo_driver_t pdo_lake_driver = {
	PDO_DRIVER_HEADER(lake),
	pdo_lake_handle_factory
};
