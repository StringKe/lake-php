/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: a9c75ba474d0c24c0c82acb41ef4ebe74ba95def */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_lastQueryId, 0, 0, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_killQuery, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, queryId, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Pdo_Lake_info, 0, 0, MAY_BE_ARRAY|MAY_BE_FALSE)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_useWarehouse, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, warehouse, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_useDatabase, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, database, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_useRole, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, role, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_setSessionSetting, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Pdo_Lake_presign, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, operation, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, stage, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_uploadToStage, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, stage, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdo_Lake_uploadFileToStage, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, stage, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Pdo_Lake_loadData, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, method, IS_LONG, 0, "Pdo\\Lake::LOAD_STAGE")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Pdo_Lake_loadFile, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, filename, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, method, IS_LONG, 0, "Pdo\\Lake::LOAD_STAGE")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, fileFormatOptions, IS_ARRAY, 1, "null")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, copyOptions, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Pdo_Lake_streamLoad, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, rows, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, method, IS_LONG, 0, "Pdo\\Lake::LOAD_STREAMING")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Pdo_Lake_putFiles, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, localFiles, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, stage, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Pdo_Lake_getFiles, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, stage, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, localDirectory, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_METHOD(Pdo_Lake, lastQueryId);
ZEND_METHOD(Pdo_Lake, killQuery);
ZEND_METHOD(Pdo_Lake, info);
ZEND_METHOD(Pdo_Lake, useWarehouse);
ZEND_METHOD(Pdo_Lake, useDatabase);
ZEND_METHOD(Pdo_Lake, useRole);
ZEND_METHOD(Pdo_Lake, setSessionSetting);
ZEND_METHOD(Pdo_Lake, presign);
ZEND_METHOD(Pdo_Lake, uploadToStage);
ZEND_METHOD(Pdo_Lake, uploadFileToStage);
ZEND_METHOD(Pdo_Lake, loadData);
ZEND_METHOD(Pdo_Lake, loadFile);
ZEND_METHOD(Pdo_Lake, streamLoad);
ZEND_METHOD(Pdo_Lake, putFiles);
ZEND_METHOD(Pdo_Lake, getFiles);

static const zend_function_entry class_Pdo_Lake_methods[] = {
	ZEND_ME(Pdo_Lake, lastQueryId, arginfo_class_Pdo_Lake_lastQueryId, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, killQuery, arginfo_class_Pdo_Lake_killQuery, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, info, arginfo_class_Pdo_Lake_info, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, useWarehouse, arginfo_class_Pdo_Lake_useWarehouse, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, useDatabase, arginfo_class_Pdo_Lake_useDatabase, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, useRole, arginfo_class_Pdo_Lake_useRole, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, setSessionSetting, arginfo_class_Pdo_Lake_setSessionSetting, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, presign, arginfo_class_Pdo_Lake_presign, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, uploadToStage, arginfo_class_Pdo_Lake_uploadToStage, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, uploadFileToStage, arginfo_class_Pdo_Lake_uploadFileToStage, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, loadData, arginfo_class_Pdo_Lake_loadData, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, loadFile, arginfo_class_Pdo_Lake_loadFile, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, streamLoad, arginfo_class_Pdo_Lake_streamLoad, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, putFiles, arginfo_class_Pdo_Lake_putFiles, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdo_Lake, getFiles, arginfo_class_Pdo_Lake_getFiles, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_Pdo_Lake(zend_class_entry *class_entry_PDO)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Pdo", "Lake", class_Pdo_Lake_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, class_entry_PDO, ZEND_ACC_NO_DYNAMIC_PROPERTIES|ZEND_ACC_NOT_SERIALIZABLE);

	zval const_ATTR_WAREHOUSE_value;
	ZVAL_LONG(&const_ATTR_WAREHOUSE_value, PDO_LAKE_ATTR_WAREHOUSE);
	zend_string *const_ATTR_WAREHOUSE_name = zend_string_init_interned("ATTR_WAREHOUSE", sizeof("ATTR_WAREHOUSE") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_ATTR_WAREHOUSE_name, &const_ATTR_WAREHOUSE_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_ATTR_WAREHOUSE_name);

	zval const_ATTR_ROLE_value;
	ZVAL_LONG(&const_ATTR_ROLE_value, PDO_LAKE_ATTR_ROLE);
	zend_string *const_ATTR_ROLE_name = zend_string_init_interned("ATTR_ROLE", sizeof("ATTR_ROLE") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_ATTR_ROLE_name, &const_ATTR_ROLE_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_ATTR_ROLE_name);

	zval const_ATTR_DATABASE_value;
	ZVAL_LONG(&const_ATTR_DATABASE_value, PDO_LAKE_ATTR_DATABASE);
	zend_string *const_ATTR_DATABASE_name = zend_string_init_interned("ATTR_DATABASE", sizeof("ATTR_DATABASE") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_ATTR_DATABASE_name, &const_ATTR_DATABASE_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_ATTR_DATABASE_name);

	zval const_ATTR_SERVER_SIDE_PARAMS_value;
	ZVAL_LONG(&const_ATTR_SERVER_SIDE_PARAMS_value, PDO_LAKE_ATTR_SERVER_SIDE_PARAMS);
	zend_string *const_ATTR_SERVER_SIDE_PARAMS_name = zend_string_init_interned("ATTR_SERVER_SIDE_PARAMS", sizeof("ATTR_SERVER_SIDE_PARAMS") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_ATTR_SERVER_SIDE_PARAMS_name, &const_ATTR_SERVER_SIDE_PARAMS_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_ATTR_SERVER_SIDE_PARAMS_name);

	zval const_ATTR_QUERY_ID_value;
	ZVAL_LONG(&const_ATTR_QUERY_ID_value, PDO_LAKE_ATTR_QUERY_ID);
	zend_string *const_ATTR_QUERY_ID_name = zend_string_init_interned("ATTR_QUERY_ID", sizeof("ATTR_QUERY_ID") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_ATTR_QUERY_ID_name, &const_ATTR_QUERY_ID_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_ATTR_QUERY_ID_name);

	zval const_ATTR_QUERY_STATS_value;
	ZVAL_LONG(&const_ATTR_QUERY_STATS_value, PDO_LAKE_ATTR_QUERY_STATS);
	zend_string *const_ATTR_QUERY_STATS_name = zend_string_init_interned("ATTR_QUERY_STATS", sizeof("ATTR_QUERY_STATS") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_ATTR_QUERY_STATS_name, &const_ATTR_QUERY_STATS_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_ATTR_QUERY_STATS_name);

	zval const_LOAD_STAGE_value;
	ZVAL_LONG(&const_LOAD_STAGE_value, LAKE_LOAD_STAGE);
	zend_string *const_LOAD_STAGE_name = zend_string_init_interned("LOAD_STAGE", sizeof("LOAD_STAGE") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_LOAD_STAGE_name, &const_LOAD_STAGE_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_LOAD_STAGE_name);

	zval const_LOAD_STREAMING_value;
	ZVAL_LONG(&const_LOAD_STREAMING_value, LAKE_LOAD_STREAMING);
	zend_string *const_LOAD_STREAMING_name = zend_string_init_interned("LOAD_STREAMING", sizeof("LOAD_STREAMING") - 1, 1);
	zend_declare_typed_class_constant(class_entry, const_LOAD_STREAMING_name, &const_LOAD_STREAMING_value, ZEND_ACC_PUBLIC, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(const_LOAD_STREAMING_name);

	return class_entry;
}
