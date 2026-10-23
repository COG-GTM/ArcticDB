# Branch work log: feature/rp-36-shadowed-tests

RP-36 / upstream man-group/ArcticDB#3372: test functions shadowed by same-name redefinitions never run.

## Frozen contract (resolution per duplicate pair)

| File | Pair | Resolution |
|------|------|------------|
| `python/tests/unit/arcticdb/version_store/test_empty_column_type.py` | `TestCanUpdateEmptyColumn.test_bool` x2 | Keep the `boolean_dtype`-parametrized method as `test_bool`; rename the all-`None` method to `test_all_none` |
| `python/tests/integration/arcticdb/version_store/test_update_with_date_range.py` | `test_update_date_range_non_pandas_dataframe` x2 | True duplicate (identical data and assertions): delete the first definition, keep the second unchanged |
| `python/tests/sanitizers/arcticdb/version_store/test_mem_leaks.py` | `test_mem_leak_read_all_arctic_lib` x2 | Rename the first (`arctic_library_lmdb_100gb`, writes then reads `generate_big_dataframe()`) to `test_mem_leak_write_and_read_all_arctic_lib`; keep the second (`library_with_big_symbol_`) unchanged |

Remaining F811 hits are duplicate imports; remove the redundant import, keeping the binding that is actually used.

Acceptance: `ruff check --select F811 python/` is clean; `pytest --collect-only` lists both variants of each renamed pair.

## Log

- Froze the resolution contract above.
- WS1: renamed the all-`None` `TestCanUpdateEmptyColumn.test_bool` to `test_all_none`, reviving the `boolean_dtype` `test_bool`.
- WS2: deleted the first (identical) `test_update_date_range_non_pandas_dataframe`.
- WS3: renamed the first mem-leak test to `test_mem_leak_write_and_read_all_arctic_lib`; dropped duplicate `pytest`/`gc`/`time`/`pd` imports.
- WS4: removed the remaining duplicate imports so `ruff check --select F811 python/` is clean.
