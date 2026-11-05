/* Copyright 2026 Man Group Operations Limited
 *
 * Use of this software is governed by the Business Source License 1.1 included in the file licenses/BSL.txt.
 *
 * As of the Change Date specified in that file, in accordance with the Business Source License, use of this software
 * will be governed by the Apache License, version 2.0.
 */
#include <gtest/gtest.h>
#include <arcticdb/codec/encoded_field_collection.hpp>

TEST(EncodedFieldCollection, AddField) {
    using namespace arcticdb;
    EncodedFieldCollection fields;
    auto* field1 = fields.add_field(3);
    auto& ndarray1 = *field1->mutable_ndarray();
    auto* block1 = ndarray1.add_shapes();
    block1->set_in_bytes(1);
    auto* block2 = ndarray1.add_values(EncodingVersion::V2);
    block2->set_in_bytes(2);
    auto* block3 = ndarray1.add_values(EncodingVersion::V2);
    block3->set_in_bytes(3);
    auto* field2 = fields.add_field(2);
    auto& ndarray2 = *field2->mutable_ndarray();
    auto* block4 = ndarray2.add_values(EncodingVersion::V2);
    block4->set_in_bytes(4);
    auto* block5 = ndarray2.add_values(EncodingVersion::V2);
    block5->set_in_bytes(5);

    const auto& read1 = fields.at(0);
    auto b1 = read1.shapes(0);
    ASSERT_EQ(b1.in_bytes(), 1);
    auto b2 = read1.values(0);
    ASSERT_EQ(b2.in_bytes(), 2);
    auto b3 = read1.values(1);
    ASSERT_EQ(b3.in_bytes(), 3);

    const auto& read2 = fields.at(1);
    auto b4 = read2.values(0);
    ASSERT_EQ(b4.in_bytes(), 4);
    auto b5 = read2.values(1);
    ASSERT_EQ(b5.in_bytes(), 5);
}
TEST(EncodedFieldCollection, RejectsBlockCountsBeyondBuffer) {
    using namespace arcticdb;
    EncodedFieldCollection fields;
    auto* field = fields.add_field(2);
    field->mutable_ndarray()->add_values(EncodingVersion::V2)->set_in_bytes(1);
    field->add_values(EncodingVersion::V2)->set_in_bytes(2);

    const auto& const_fields = fields;
    ASSERT_EQ(const_fields.at(0).values(1).in_bytes(), 2);

    field->values_count_ = 1'000'000;
    ASSERT_THROW((void)const_fields.at(0), CodecException);
    ASSERT_THROW((void)fields.at(0), CodecException);
    ASSERT_THROW((void)*fields.begin(), CodecException);

    EncodedFieldCollection regenerated{fields.release_data(), Buffer{}};
    ASSERT_THROW(regenerated.regenerate_offsets(), CodecException);
}

TEST(EncodedFieldCollection, RejectsBlockIndexBeyondCounts) {
    using namespace arcticdb;
    EncodedFieldCollection fields;
    auto* new_style = fields.add_field(3);
    new_style->mutable_ndarray()->add_shapes();
    new_style->add_values(EncodingVersion::V2);
    new_style->add_values(EncodingVersion::V2);
    ASSERT_NO_THROW((void)new_style->values(1));
    ASSERT_THROW((void)new_style->values(2), ArcticException);

    auto* old_style = fields.add_field(4);
    old_style->mutable_ndarray()->add_shapes();
    old_style->add_values(EncodingVersion::V1);
    old_style->add_shapes();
    old_style->add_values(EncodingVersion::V1);
    ASSERT_NO_THROW((void)old_style->shapes(1));
    ASSERT_THROW((void)old_style->shapes(2), ArcticException);
    ASSERT_THROW((void)old_style->values(2), ArcticException);
}
