#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "ext/json/php_json.h"
#include "zend_smart_str.h"
#include "ext/pdo/php_pdo.h"
#include "ext/pdo/php_pdo_driver.h"
#include "ext/pdo/php_pdo_error.h"
#include "php_pdo_lake.h"
#include "php_pdo_lake_int.h"
#include "pdo_lake_arginfo.h"

static zend_class_entry *pdo_lake_ce;

#define LAKE_METHOD_INIT() \
	pdo_dbh_t *dbh = Z_PDO_DBH_P(ZEND_THIS); \
	pdo_lake_db_handle *H; \
	PDO_CONSTRUCT_CHECK; \
	H = dbh->driver_data; \
	PDO_DBH_CLEAR_ERR(); \
	pdo_lake_clear(H)

#define LAKE_METHOD_FAIL() do { \
		pdo_lake_raise(dbh, NULL); \
		PDO_HANDLE_DBH_ERR(); \
		RETURN_FALSE; \
	} while (0)

static const lake_str lake_none = {NULL, 0};

PHP_METHOD(Pdo_Lake, lastQueryId)
{
	lake_owned id = {0};

	ZEND_PARSE_PARAMETERS_NONE();
	LAKE_METHOD_INIT();

	if (!lake_conn_last_query_id(H->conn, &id)) {
		RETURN_NULL();
	}
	RETVAL_STRINGL(id.ptr, id.len);
	lake_owned_free(&id);
}

PHP_METHOD(Pdo_Lake, killQuery)
{
	zend_string *query_id;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(query_id)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	if (!lake_conn_kill_query(H->conn, pdo_lake_str(query_id), &H->err)) {
		LAKE_METHOD_FAIL();
	}
	RETURN_TRUE;
}

PHP_METHOD(Pdo_Lake, info)
{
	ZEND_PARSE_PARAMETERS_NONE();
	LAKE_METHOD_INIT();

	if (!pdo_lake_info(dbh, return_value)) {
		PDO_HANDLE_DBH_ERR();
		RETURN_FALSE;
	}
}

static void lake_use(INTERNAL_FUNCTION_PARAMETERS, int32_t what)
{
	zend_string *value;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(value)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	if (!pdo_lake_set(dbh, what, lake_none, pdo_lake_str(value))) {
		PDO_HANDLE_DBH_ERR();
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

PHP_METHOD(Pdo_Lake, useWarehouse)
{
	lake_use(INTERNAL_FUNCTION_PARAM_PASSTHRU, LAKE_SET_WAREHOUSE);
}

PHP_METHOD(Pdo_Lake, useDatabase)
{
	lake_use(INTERNAL_FUNCTION_PARAM_PASSTHRU, LAKE_SET_DATABASE);
}

PHP_METHOD(Pdo_Lake, useRole)
{
	lake_use(INTERNAL_FUNCTION_PARAM_PASSTHRU, LAKE_SET_ROLE);
}

PHP_METHOD(Pdo_Lake, setSessionSetting)
{
	zend_string *name, *value;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(name)
		Z_PARAM_STR(value)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	if (!pdo_lake_set(dbh, LAKE_SET_SESSION, pdo_lake_str(name), pdo_lake_str(value))) {
		PDO_HANDLE_DBH_ERR();
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

PHP_METHOD(Pdo_Lake, presign)
{
	zend_string *operation, *stage;
	lake_owned json = {0};

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(operation)
		Z_PARAM_STR(stage)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	if (!lake_conn_presign(H->conn, pdo_lake_str(operation), pdo_lake_str(stage), &json, &H->err)) {
		LAKE_METHOD_FAIL();
	}
	php_json_decode_ex(return_value, json.ptr, json.len, PHP_JSON_OBJECT_AS_ARRAY, PHP_JSON_PARSER_DEFAULT_DEPTH);
	lake_owned_free(&json);
}

PHP_METHOD(Pdo_Lake, uploadToStage)
{
	zend_string *stage, *data;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(stage)
		Z_PARAM_STR(data)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	if (!lake_conn_upload_to_stage(H->conn, pdo_lake_str(stage), pdo_lake_str(data), &H->err)) {
		LAKE_METHOD_FAIL();
	}
	RETURN_TRUE;
}

PHP_METHOD(Pdo_Lake, uploadFileToStage)
{
	zend_string *stage, *filename;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(stage)
		Z_PARAM_PATH_STR(filename)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	if (!lake_conn_upload_file_to_stage(H->conn, pdo_lake_str(stage), pdo_lake_str(filename), &H->err)) {
		LAKE_METHOD_FAIL();
	}
	RETURN_TRUE;
}

static bool lake_check_method(zend_long method, uint32_t arg_num)
{
	if (method != LAKE_LOAD_STAGE && method != LAKE_LOAD_STREAMING) {
		zend_argument_value_error(arg_num, "must be Pdo\\Lake::LOAD_STAGE or Pdo\\Lake::LOAD_STREAMING");
		return false;
	}
	return true;
}

PHP_METHOD(Pdo_Lake, loadData)
{
	zend_string *sql, *data;
	zend_long method = LAKE_LOAD_STAGE;
	lake_stats stats;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_STR(sql)
		Z_PARAM_STR(data)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(method)
	ZEND_PARSE_PARAMETERS_END();
	if (!lake_check_method(method, 3)) {
		RETURN_THROWS();
	}
	LAKE_METHOD_INIT();

	if (!lake_conn_load_data(H->conn, pdo_lake_str(sql), pdo_lake_str(data), (int32_t) method, &stats, &H->err)) {
		LAKE_METHOD_FAIL();
	}
	pdo_lake_add_stats(return_value, &stats);
}

static bool lake_encode_options(HashTable *options, smart_str *out)
{
	zval tmp;

	if (!options) {
		return true;
	}
	ZVAL_ARR(&tmp, options);
	if (php_json_encode(out, &tmp, PHP_JSON_FORCE_OBJECT | PHP_JSON_UNESCAPED_UNICODE | PHP_JSON_UNESCAPED_SLASHES) != SUCCESS) {
		return false;
	}
	smart_str_0(out);
	return true;
}

static lake_str lake_smart_str(const smart_str *s)
{
	return s->s ? pdo_lake_str(s->s) : lake_none;
}

PHP_METHOD(Pdo_Lake, loadFile)
{
	zend_string *sql, *filename;
	zend_long method = LAKE_LOAD_STAGE;
	HashTable *format_options = NULL, *copy_options = NULL;
	smart_str format_json = {0}, copy_json = {0};
	lake_stats stats;
	bool ok;

	ZEND_PARSE_PARAMETERS_START(2, 5)
		Z_PARAM_STR(sql)
		Z_PARAM_PATH_STR(filename)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(method)
		Z_PARAM_ARRAY_HT_OR_NULL(format_options)
		Z_PARAM_ARRAY_HT_OR_NULL(copy_options)
	ZEND_PARSE_PARAMETERS_END();
	if (!lake_check_method(method, 3)) {
		RETURN_THROWS();
	}
	LAKE_METHOD_INIT();

	if (!lake_encode_options(format_options, &format_json) || !lake_encode_options(copy_options, &copy_json)) {
		smart_str_free(&format_json);
		smart_str_free(&copy_json);
		zend_value_error("Pdo\\Lake::loadFile(): options must be encodable as JSON");
		RETURN_THROWS();
	}
	ok = lake_conn_load_file(H->conn, pdo_lake_str(sql), pdo_lake_str(filename), (int32_t) method, lake_smart_str(&format_json), lake_smart_str(&copy_json), &stats, &H->err);
	smart_str_free(&format_json);
	smart_str_free(&copy_json);
	if (!ok) {
		LAKE_METHOD_FAIL();
	}
	pdo_lake_add_stats(return_value, &stats);
}

PHP_METHOD(Pdo_Lake, streamLoad)
{
	zend_string *sql;
	HashTable *rows;
	zend_long method = LAKE_LOAD_STREAMING;
	uint32_t row_count, column_count = 0, row_index = 0;
	lake_str *cells = NULL;
	zend_string **temps = NULL;
	size_t temp_count = 0, cell = 0;
	lake_stats stats;
	zval *row, *value;
	bool ok = false;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_STR(sql)
		Z_PARAM_ARRAY_HT(rows)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(method)
	ZEND_PARSE_PARAMETERS_END();
	if (!lake_check_method(method, 3)) {
		RETURN_THROWS();
	}
	LAKE_METHOD_INIT();

	row_count = zend_hash_num_elements(rows);
	ZEND_HASH_FOREACH_VAL(rows, row) {
		ZVAL_DEREF(row);
		if (Z_TYPE_P(row) != IS_ARRAY) {
			zend_argument_type_error(2, "must contain only arrays, %s found at row %u", zend_zval_value_name(row), row_index);
			goto cleanup;
		}
		if (row_index == 0) {
			column_count = zend_hash_num_elements(Z_ARRVAL_P(row));
			if (column_count == 0) {
				zend_argument_value_error(2, "rows must not be empty");
				goto cleanup;
			}
			cells = safe_emalloc((size_t) row_count, (size_t) column_count * sizeof(lake_str), 0);
		} else if (zend_hash_num_elements(Z_ARRVAL_P(row)) != column_count) {
			zend_argument_value_error(2, "row %u has %u values, expected %u", row_index, zend_hash_num_elements(Z_ARRVAL_P(row)), column_count);
			goto cleanup;
		}
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(row), value) {
			ZVAL_DEREF(value);
			switch (Z_TYPE_P(value)) {
				case IS_NULL:
					cells[cell] = lake_none;
					break;
				case IS_STRING:
					cells[cell] = pdo_lake_str(Z_STR_P(value));
					break;
				case IS_TRUE:
					cells[cell] = pdo_lake_cstr("1", 1);
					break;
				case IS_FALSE:
					cells[cell] = pdo_lake_cstr("0", 1);
					break;
				default: {
					zend_string *text = zval_try_get_string(value);
					if (!text) {
						goto cleanup;
					}
					if (!temps) {
						temps = safe_emalloc((size_t) row_count, (size_t) column_count * sizeof(zend_string *), 0);
					}
					temps[temp_count++] = text;
					cells[cell] = pdo_lake_str(text);
				}
			}
			cell++;
		} ZEND_HASH_FOREACH_END();
		row_index++;
	} ZEND_HASH_FOREACH_END();

	ok = lake_conn_stream_load(H->conn, pdo_lake_str(sql), cells, row_count, column_count, (int32_t) method, &stats, &H->err);
	if (!ok) {
		pdo_lake_raise(dbh, NULL);
	}

cleanup:
	for (size_t i = 0; i < temp_count; i++) {
		zend_string_release_ex(temps[i], 0);
	}
	if (temps) {
		efree(temps);
	}
	if (cells) {
		efree(cells);
	}
	if (EG(exception)) {
		RETURN_THROWS();
	}
	if (!ok) {
		PDO_HANDLE_DBH_ERR();
		RETURN_FALSE;
	}
	pdo_lake_add_stats(return_value, &stats);
}

static void lake_result_rows(pdo_dbh_t *dbh, lake_result *result, zval *return_value)
{
	pdo_lake_db_handle *H = dbh->driver_data;
	size_t columns = lake_result_column_count(result);
	zend_string **names = safe_emalloc(columns, sizeof(zend_string *), 0);
	lake_column column;
	lake_cell cell;
	zval row, value;
	int32_t more;

	for (size_t i = 0; i < columns; i++) {
		lake_result_column(result, i, &column);
		names[i] = zend_string_init(column.name, column.name_len, 0);
	}

	array_init(return_value);
	while ((more = lake_result_next(result, &H->err)) == 1) {
		array_init_size(&row, (uint32_t) columns);
		for (size_t i = 0; i < columns; i++) {
			lake_result_cell(result, i, &cell);
			pdo_lake_cell_zval(&cell, &value);
			zend_hash_update(Z_ARRVAL(row), names[i], &value);
		}
		zend_hash_next_index_insert_new(Z_ARRVAL_P(return_value), &row);
	}

	for (size_t i = 0; i < columns; i++) {
		zend_string_release_ex(names[i], 0);
	}
	efree(names);
	lake_result_free(result);

	if (more < 0) {
		zval_ptr_dtor(return_value);
		pdo_lake_raise(dbh, NULL);
		PDO_HANDLE_DBH_ERR();
		RETURN_FALSE;
	}
}

PHP_METHOD(Pdo_Lake, putFiles)
{
	zend_string *local, *stage;
	lake_result *result;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(local)
		Z_PARAM_STR(stage)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	result = lake_conn_put_files(H->conn, pdo_lake_str(local), pdo_lake_str(stage), &H->err);
	if (!result) {
		LAKE_METHOD_FAIL();
	}
	lake_result_rows(dbh, result, return_value);
}

PHP_METHOD(Pdo_Lake, getFiles)
{
	zend_string *stage, *local;
	lake_result *result;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(stage)
		Z_PARAM_STR(local)
	ZEND_PARSE_PARAMETERS_END();
	LAKE_METHOD_INIT();

	result = lake_conn_get_files(H->conn, pdo_lake_str(stage), pdo_lake_str(local), &H->err);
	if (!result) {
		LAKE_METHOD_FAIL();
	}
	lake_result_rows(dbh, result, return_value);
}

PHP_MINIT_FUNCTION(pdo_lake)
{
	pdo_lake_ce = register_class_Pdo_Lake(php_pdo_get_dbh_ce());
	pdo_lake_ce->create_object = pdo_dbh_new;

	if (php_pdo_register_driver(&pdo_lake_driver) == FAILURE) {
		return FAILURE;
	}
	return php_pdo_register_driver_specific_ce(&pdo_lake_driver, pdo_lake_ce);
}

PHP_MSHUTDOWN_FUNCTION(pdo_lake)
{
	php_pdo_unregister_driver(&pdo_lake_driver);
	return SUCCESS;
}

PHP_MINFO_FUNCTION(pdo_lake)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "PDO Driver for Lake", "enabled");
	php_info_print_table_row(2, "Extension version", PHP_PDO_LAKE_VERSION);
	php_info_print_table_row(2, "lake-driver version", lake_driver_version());
	php_info_print_table_end();
}

static const zend_module_dep pdo_lake_deps[] = {
	ZEND_MOD_REQUIRED("pdo")
	ZEND_MOD_REQUIRED("json")
	ZEND_MOD_END
};

zend_module_entry pdo_lake_module_entry = {
	STANDARD_MODULE_HEADER_EX, NULL,
	pdo_lake_deps,
	"pdo_lake",
	NULL,
	PHP_MINIT(pdo_lake),
	PHP_MSHUTDOWN(pdo_lake),
	NULL,
	NULL,
	PHP_MINFO(pdo_lake),
	PHP_PDO_LAKE_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PDO_LAKE
ZEND_GET_MODULE(pdo_lake)
#endif
