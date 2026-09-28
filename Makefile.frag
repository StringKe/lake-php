.PHONY: lake-ffi

# cargo decides whether the static library is stale, so make always asks it.
lake-ffi:
	$(LAKE_FFI_BUILD)

$(LAKE_FFI_LIB): lake-ffi
