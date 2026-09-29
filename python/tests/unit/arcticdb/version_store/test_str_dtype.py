"""
Copyright 2026 Man Group Operations Limited

Use of this software is governed by the Business Source License 1.1 included in the file licenses/BSL.txt.

As of the Change Date specified in that file, in accordance with the Business Source License, use of this software will be governed by the Apache License, version 2.0.

Tests for the pandas `str` dtype (`pd.StringDtype(na_value=np.nan)`), which is the default dtype inferred for strings
as of pandas 3, alongside `object` dtype string columns.

The `future.infer_string` option is used so that the pandas 3 default behaviour is also exercised with pandas 2.3.
"""

import numpy as np
import pandas as pd
import pytest

from arcticdb.util.test import assert_frame_equal, assert_series_equal

try:
    STR_DTYPE = pd.StringDtype(na_value=np.nan)
except (AttributeError, TypeError):
    STR_DTYPE = None

pytestmark = pytest.mark.skipif(STR_DTYPE is None, reason="The pandas `str` dtype requires pandas >= 2.3")


@pytest.fixture(params=[True, False], ids=["infer_string", "no_infer_string"])
def infer_string(request):
    with pd.option_context("future.infer_string", request.param):
        yield request.param


def assert_same_missing_values(actual, expected):
    assert len(actual) == len(expected)
    for a, e in zip(actual, expected):
        if e is None:
            assert a is None
        elif isinstance(e, float) and np.isnan(e):
            assert isinstance(a, float) and np.isnan(a)
        else:
            assert a == e


def test_write_read_str_columns(lmdb_version_store, infer_string):
    lib = lmdb_version_store
    df = pd.DataFrame(
        {"a": pd.array(["x", "yy", "zzz"], dtype=STR_DTYPE), "b": np.arange(3), "c": ["p", "q", "\u2122"]},
        index=pd.date_range("2025-01-01", periods=3, unit="ns"),
    )
    df["c"] = df["c"].astype(STR_DTYPE)
    lib.write("sym", df)
    result = lib.read("sym").data
    assert result["a"].dtype == STR_DTYPE
    assert result["c"].dtype == STR_DTYPE
    assert_frame_equal(result, df)


def test_str_columns_are_inferred_by_default_with_infer_string(lmdb_version_store):
    lib = lmdb_version_store
    with pd.option_context("future.infer_string", True):
        df = pd.DataFrame({"a": ["x", "y", None]})
        assert df["a"].dtype == STR_DTYPE
        lib.write("sym", df)
        result = lib.read("sym").data
    assert result["a"].dtype == STR_DTYPE
    assert_frame_equal(result, df)


def test_str_missing_values_read_back_as_nan(lmdb_version_store, infer_string):
    lib = lmdb_version_store
    df = pd.DataFrame({"a": pd.array(["x", None, np.nan, pd.NA, "y"], dtype=STR_DTYPE)})
    lib.write("sym", df)
    result = lib.read("sym").data
    assert result["a"].dtype == STR_DTYPE
    assert result["a"].isna().tolist() == [False, True, True, True, False]
    assert_same_missing_values(result["a"].tolist(), ["x", np.nan, np.nan, np.nan, "y"])
    assert_frame_equal(result, df)


def test_all_missing_str_column(lmdb_version_store, infer_string):
    lib = lmdb_version_store
    df = pd.DataFrame({"a": pd.array([None, np.nan], dtype=STR_DTYPE), "b": [1, 2]})
    lib.write("sym", df)
    result = lib.read("sym").data
    assert result["a"].dtype == STR_DTYPE
    assert_frame_equal(result, df)


def test_object_string_columns_unchanged(lmdb_version_store, infer_string):
    lib = lmdb_version_store
    values = ["x", None, np.nan, "y"]
    df = pd.DataFrame({"a": pd.Series(values, dtype=object)})
    lib.write("sym", df)
    result = lib.read("sym").data
    assert result["a"].dtype == object
    assert_same_missing_values(result["a"].tolist(), values)
    assert_frame_equal(result, df)


def test_str_and_object_columns_together(lmdb_version_store, infer_string):
    lib = lmdb_version_store
    df = pd.DataFrame(
        {
            "str_col": pd.array(["x", None, "z"], dtype=STR_DTYPE),
            "obj_col": pd.Series(["x", None, "z"], dtype=object),
            "int_col": [1, 2, 3],
        }
    )
    lib.write("sym", df)
    result = lib.read("sym").data
    assert result.dtypes.to_dict() == {"str_col": STR_DTYPE, "obj_col": np.dtype(object), "int_col": np.dtype("int64")}
    assert_same_missing_values(result["obj_col"].tolist(), ["x", None, "z"])
    assert_frame_equal(result, df)

    result = lib.read("sym", columns=["str_col", "obj_col"]).data
    assert_frame_equal(result, df[["str_col", "obj_col"]])


def test_normalization_metadata_records_only_str_columns(lmdb_version_store):
    lib = lmdb_version_store
    df = pd.DataFrame(
        {"str_col": pd.array(["x"], dtype=STR_DTYPE), "obj_col": pd.Series(["x"], dtype=object), "int_col": [1]}
    )
    lib.write("sym", df)
    norm_meta = lib.get_info("sym")["normalization_metadata"].df.common
    assert list(norm_meta.str_dtype_columns) == ["str_col"]
    assert not norm_meta.index.is_str_dtype


@pytest.mark.parametrize("index_dtype", [STR_DTYPE, object])
def test_string_index(lmdb_version_store, infer_string, index_dtype):
    lib = lmdb_version_store
    df = pd.DataFrame({"a": [1, 2, 3]}, index=pd.Index(["x", "y", "z"], dtype=index_dtype, name="idx"))
    lib.write("sym", df)
    result = lib.read("sym").data
    assert result.index.dtype == index_dtype
    assert_frame_equal(result, df)


@pytest.mark.parametrize("level_dtype", [STR_DTYPE, object])
def test_string_multi_index(lmdb_version_store, infer_string, level_dtype):
    lib = lmdb_version_store
    index = pd.MultiIndex.from_arrays(
        [
            pd.Index(["x", "x", "y"], dtype=level_dtype),
            pd.Index(["p", "q", "p"], dtype=level_dtype),
            pd.date_range("2025-01-01", periods=3, unit="ns"),
        ],
        names=["l0", "l1", "l2"],
    )
    df = pd.DataFrame({"a": [1, 2, 3]}, index=index)
    lib.write("sym", df)
    result = lib.read("sym").data
    assert [result.index.levels[i].dtype for i in range(2)] == [level_dtype, level_dtype]
    assert_frame_equal(result, df)


@pytest.mark.parametrize("dtype", [STR_DTYPE, object])
def test_string_series(lmdb_version_store, infer_string, dtype):
    lib = lmdb_version_store
    series = pd.Series(["x", None, "z"], dtype=dtype, name="s")
    lib.write("sym", series)
    result = lib.read("sym").data
    assert result.dtype == dtype
    assert_series_equal(result, series)


def test_str_columns_without_consolidation(version_store_factory, monkeypatch, infer_string):
    monkeypatch.setenv("SKIP_DF_CONSOLIDATION", "true")
    lib = version_store_factory(dynamic_strings=True)
    df = pd.DataFrame(
        {
            "str_col": pd.array(["x", None, "z"], dtype=STR_DTYPE),
            "obj_col": pd.Series(["x", None, "z"], dtype=object),
            "int_col": [1, 2, 3],
        }
    )
    lib.write("sym", df)
    result = lib.read("sym").data
    assert result["str_col"].dtype == STR_DTYPE
    assert result["obj_col"].dtype == object
    assert_same_missing_values(result["obj_col"].tolist(), ["x", None, "z"])
    assert_frame_equal(result, df)


def test_append_str_columns(lmdb_version_store_dynamic_schema_v1, infer_string):
    lib = lmdb_version_store_dynamic_schema_v1
    df_1 = pd.DataFrame(
        {"a": pd.array(["x", "y"], dtype=STR_DTYPE)}, index=pd.date_range("2025-01-01", periods=2, unit="ns")
    )
    df_2 = pd.DataFrame(
        {"a": pd.array([None, "z"], dtype=STR_DTYPE), "b": pd.array(["p", "q"], dtype=STR_DTYPE)},
        index=pd.date_range("2025-01-03", periods=2, unit="ns"),
    )
    lib.write("sym", df_1)
    lib.append("sym", df_2)
    result = lib.read("sym").data
    expected = pd.concat([df_1, df_2])
    assert result["a"].dtype == STR_DTYPE
    assert result["b"].dtype == STR_DTYPE
    assert_frame_equal(result, expected)


def test_str_column_with_fixed_width_strings(lmdb_version_store_string_coercion, infer_string):
    lib = lmdb_version_store_string_coercion
    df = pd.DataFrame({"a": pd.array(["x", "yy", "zzz"], dtype=STR_DTYPE)})
    lib.write("sym", df, dynamic_strings=False)
    result = lib.read("sym").data
    assert result["a"].dtype == STR_DTYPE
    assert_frame_equal(result, df)
