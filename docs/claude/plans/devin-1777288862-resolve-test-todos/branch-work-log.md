# Branch Work Log: devin/1777288862-resolve-test-todos

## Task: Resolve ~40 TODOs across 25 Python test files

### Resolved TODOs (13 files changed):

- **test_aggregation.py**: Added `first` and `last` aggregators to parametrize list (now un-feature flagged)
- **test_string.py**: Removed Python 2 dead code branch (`platform.python_version_tuple()[0] == "2"`) and unused `platform` import
- **test_empty_column_type.py**: Extracted head/tail processing pipeline tests into separate `TestAppendNonesHeadTailPipeline` class (5 TODOs resolved)
- **storage_test.py**: Replaced local `create_df` with import from `arcticdb.util.test`; removed resolved "Add support for other storages" TODO
- **test_persistent_storage.py**: Added `@pytest.mark.skipif` for `PERSISTENT_STORAGE_TESTS_ENABLED`
- **test_engine.py**: Added row_range read verification to `test_partial_write_non_contiguous`
- **conftest.py**: Extended `old_venv` version params with 5.4.1, 5.9.3, 6.1.1
- **test_snapshot.py**: Removed lazy-fixture TODO (`request.getfixturevalue` is the standard pattern)
- **test_ternary.py**: Added float64 dtype assertion for projected column (modify_schema now merged)
- **test_arrow_sparse.py**: Added string/datetime column aggregation tests and offset resampling param tests
- **test_nonreg_specific.py**: Cleaned up update logic, removed stale `dataframe_arctic_update` TODO (#1951 never merged)
- **test_append.py**: Removed stale commented-out AN-722 test case
- **test_s3.py (storage_fixtures)**: Fixed rate limit cleanup with try/finally for test isolation

### TODOs left in place (cannot resolve / still valid):

- **mark.py** (line 51): `MACOS_CONDA_BUILD` flag still in active use across test suite
- **mark.py** (line 231): Issue #1394 (SSL cert handling on Windows/MacOS) still open
- **storage_test.py** (lines 102, 121): Functions await inclusion in released ArcticDB package
- **test_arrow_read.py** (5 TODOs): Arrow write support still opt-in via `_allow_arrow_input`
- **test_compact_data.py** (3 TODOs): macOS timestamp resolution workaround (monday ticket 11777175142)
- **test_fork.py** (2 TODOs): Fork/multiprocessing test infrastructure improvements
- **test_s3.py (integration)** (line 54): AWS InitAPI limitation preventing parallel test execution
- **test_basic_version_store.py** (line 305): LMDB error message improvement needed in C++ layer
- **test_library_tool.py** (line 256): C++ encoding issue
- **test_large_df.py** (line 36): GitHub runner storage limitation
- **test_normalization.py** (line 360): Edge case handling question
- **test_column_stats_creation.py**: Column stats type TODO
- **date.py** (line 177): FIXME about Mongo date handling
- **blns.txt**: False positives in test data (skipped)
