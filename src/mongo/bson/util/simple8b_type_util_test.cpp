/**
 *    Copyright (C) 2021-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/bson/util/simple8b_type_util.h"
#include "mongo/unittest/unittest.h"

#include <limits>

using namespace mongo;

uint8_t scaleIndexForMultiplier(double multiplier) {
    auto iterIdx = std::find(Simple8bTypeUtil::kScaleMultiplier.begin(),
                             Simple8bTypeUtil::kScaleMultiplier.end(),
                             multiplier);
    if (iterIdx != Simple8bTypeUtil::kScaleMultiplier.end()) {
        return iterIdx - Simple8bTypeUtil::kScaleMultiplier.begin();
    }
    // We should never reach this
    ASSERT(false);
    return 0;
}

TEST(Simple8bTypeUtil, EncodeAndDecodePositiveSignedInt) {
    int64_t signedVal = 1;
    uint64_t unsignedVal = Simple8bTypeUtil::encodeInt64(signedVal);
    ASSERT_EQUALS(unsignedVal, 2);
    int64_t decodedSignedVal = Simple8bTypeUtil::decodeInt64(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeNegativeSignedInt) {
    int64_t signedVal = -1;
    uint64_t unsignedVal = Simple8bTypeUtil::encodeInt64(signedVal);
    ASSERT_EQUALS(unsignedVal, 0x1);
    int64_t decodedSignedVal = Simple8bTypeUtil::decodeInt64(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeMaxPositiveSignedInt) {
    int64_t signedVal = std::numeric_limits<int64_t>::max();
    uint64_t unsignedVal = Simple8bTypeUtil::encodeInt64(signedVal);
    ASSERT_EQUALS(unsignedVal, 0xFFFFFFFFFFFFFFFE);
    int64_t decodedSignedVal = Simple8bTypeUtil::decodeInt64(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeMaxNegativeSignedInt) {
    int64_t signedVal = std::numeric_limits<int64_t>::lowest();
    uint64_t unsignedVal = Simple8bTypeUtil::encodeInt64(signedVal);
    ASSERT_EQUALS(unsignedVal, 0xFFFFFFFFFFFFFFFF);
    int64_t decodedSignedVal = Simple8bTypeUtil::decodeInt64(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeZero) {
    int64_t signedVal = 0;
    uint64_t unsignedVal = Simple8bTypeUtil::encodeInt64(signedVal);
    ASSERT_EQUALS(unsignedVal, 0x0);
    int64_t decodedSignedVal = Simple8bTypeUtil::decodeInt64(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodePositiveInt) {
    int64_t signedVal = 1234;
    // Represented as 1234 = 0...010011010010
    //  Left Shifted 0..0100110100100
    //  Right shifted 0..000000000000
    //  xor = 0..0100110100100 = 0x9A4
    uint64_t unsignedVal = Simple8bTypeUtil::encodeInt64(signedVal);
    ASSERT_EQUALS(unsignedVal, 0x9A4);
    int64_t decodedSignedVal = Simple8bTypeUtil::decodeInt64(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeNegativeInt) {
    int64_t signedVal = -1234;
    // Represented as 1234 = 1...010011010010
    //  Left Shifted 0..0100110100100
    //  Right shifted 0..000000000001
    //  xor = 0..0100110100101 = 0x9A3
    uint64_t unsignedVal = Simple8bTypeUtil::encodeInt64(signedVal);
    ASSERT_EQUALS(unsignedVal, 0x9A3);
    int64_t decodedSignedVal = Simple8bTypeUtil::decodeInt64(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8b, DecimalPositiveValue) {
    double val = 1.0;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(1));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    ASSERT_EQUALS(encodeResult.get(), 1);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, EightDigitDecimalValue) {
    double val = 1.12345678;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(100000000));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    ASSERT_EQUALS(encodeResult.get(), 112345678);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, TwoDigitDecimalValue) {
    double val = 1.12;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(100));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    ASSERT_EQUALS(encodeResult.get(), 112);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, FloatExceedDigitsValue) {
    double val = 1.123456789;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_FALSE(scalar);
}

TEST(Simple8b, SparseDecimalValue) {
    double val = 1.00000001;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(100000000));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    ASSERT_EQUALS(encodeResult.get(), 100000001);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, RoundingDecimalValue) {
    double val = 1.455454;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(100000000));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    ASSERT_EQUALS(encodeResult.get(), 145545400);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, AllNines) {
    double val = 1.99999999;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(100000000));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    ASSERT_EQUALS(encodeResult.get(), 199999999);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, 3DigitValue) {
    double val = 123.123;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(10000));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    ASSERT_EQUALS(encodeResult.get(), (1231230));
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, RoundingTooShortShouldFail) {
    double val = 1.9876543;
    boost::optional<int64_t> encodeResult =
        Simple8bTypeUtil::encodeDouble(val, scaleIndexForMultiplier(10000));
    ASSERT_FALSE(encodeResult);
}

TEST(Simple8b, TestNaNAndInfinity) {
    double val = std::numeric_limits<double>::quiet_NaN();
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_FALSE(scalar);
    boost::optional<int64_t> result =
        Simple8bTypeUtil::encodeDouble(val, scaleIndexForMultiplier(100000000));
    ASSERT_FALSE(result.has_value());
    val = std::numeric_limits<double>::infinity();
    scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_FALSE(scalar);
    result = Simple8bTypeUtil::encodeDouble(val, scaleIndexForMultiplier(100000000));
    ASSERT_FALSE(result.has_value());
}

TEST(Simple8b, TestMaxDoubleShouldFail) {
    double val = std::numeric_limits<double>::max();
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_FALSE(scalar);
    boost::optional<int64_t> result =
        Simple8bTypeUtil::encodeDouble(val, scaleIndexForMultiplier(100000000));
    ASSERT_FALSE(result.has_value());
}

TEST(Simple8b, TestMinDoubleShouldFail) {
    double val = std::numeric_limits<double>::lowest();
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_FALSE(scalar);
    boost::optional<int64_t> result =
        Simple8bTypeUtil::encodeDouble(val, scaleIndexForMultiplier(100000000));
    ASSERT_FALSE(result.has_value());
}

TEST(Simple8b, InterpretAsMemory) {
    std::vector<double> vals = {0.0,
                                1.12345678,
                                std::numeric_limits<double>::max(),
                                std::numeric_limits<double>::min(),
                                std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::signaling_NaN(),
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::denorm_min()};

    for (double val : vals) {
        boost::optional<int64_t> result =
            Simple8bTypeUtil::encodeDouble(val, Simple8bTypeUtil::kMemoryAsInteger);
        ASSERT_TRUE(result);
        ASSERT_EQ(*result, *reinterpret_cast<const int64_t*>(&val));

        // Some of the special values above does not compare equal with themselves (signaling NaN).
        // Verify that we end up with the same memory after decoding
        double decoded =
            Simple8bTypeUtil::decodeDouble(*result, Simple8bTypeUtil::kMemoryAsInteger);
        ASSERT_EQ(memcmp(&decoded, &val, sizeof(val)), 0);
    }
}

TEST(Simple8b, TestMaxInt) {
    // max int that can be stored as a double without losing precision
    double val = std::pow(2, 53);
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(1));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    // Handle negative case
    ASSERT_EQUALS(encodeResult.get(), int64_t(val));
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, NegativeValue) {
    double val = -123.123;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(10000));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    // Handle negative case
    ASSERT_EQUALS(encodeResult.get(), -1231230);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, NegativeSixDecimalValue) {
    double val = -123.123456;
    boost::optional<uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(100000000));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    // Handle negative case by subtracting 1
    ASSERT_EQUALS(encodeResult.get(), -12312345600);
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, TestMinInt) {
    // min int that can be stored as a double without losing precision
    double val = -std::pow(2, 53);
    boost::optional<std::uint8_t> scalar = Simple8bTypeUtil::calculateDecimalShiftMultiplier(val);
    ASSERT_TRUE(scalar);
    ASSERT_EQUALS(scalar.get(), scaleIndexForMultiplier(1));
    boost::optional<int64_t> encodeResult = Simple8bTypeUtil::encodeDouble(val, scalar.get());
    ASSERT_TRUE(encodeResult);
    // Handle negative case
    ASSERT_EQUALS(encodeResult.get(), int64_t(val));
    double decodeResult = Simple8bTypeUtil::decodeDouble(encodeResult.get(), scalar.get());
    ASSERT_EQUALS(val, decodeResult);
}

TEST(Simple8b, TestObjectId) {
    OID objId("112233445566778899AABBCC");
    int64_t encodedObjId = Simple8bTypeUtil::encodeObjectId(objId);

    int64_t expectedEncodedObjId = 0x11AA22BB33CC44;
    ASSERT_EQUALS(encodedObjId, expectedEncodedObjId);

    OID actualObjId = Simple8bTypeUtil::decodeObjectId(encodedObjId, objId.getInstanceUnique());
    ASSERT_EQUALS(objId, actualObjId);
}

TEST(Simple8bTypeUtil, EncodeAndDecodePositiveSignedInt128) {
    int128_t signedVal = 1;
    uint128_t unsignedVal = Simple8bTypeUtil::encodeInt128(signedVal);
    ASSERT_EQUALS(unsignedVal, 2);
    int128_t decodedSignedVal = Simple8bTypeUtil::decodeInt128(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeNegativeSignedInt128) {
    int128_t signedVal = -1;
    uint128_t unsignedVal = Simple8bTypeUtil::encodeInt128(signedVal);
    ASSERT_EQUALS(unsignedVal, 0x1);
    int128_t decodedSignedVal = Simple8bTypeUtil::decodeInt128(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeMaxPositiveSignedInt128) {
    int128_t signedVal = std::numeric_limits<int128_t>::max();
    uint128_t unsignedVal = Simple8bTypeUtil::encodeInt128(signedVal);
    uint128_t expectedVal = absl::MakeInt128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFE);
    ASSERT_EQUALS(unsignedVal, expectedVal);
    int128_t decodedSignedVal = Simple8bTypeUtil::decodeInt128(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}

TEST(Simple8bTypeUtil, EncodeAndDecodeMaxNegativeSignedInt128) {
    int128_t signedVal = std::numeric_limits<int128_t>::min();
    uint128_t unsignedVal = Simple8bTypeUtil::encodeInt128(signedVal);
    uint128_t expectedVal = absl::MakeInt128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
    ASSERT_EQUALS(unsignedVal, expectedVal);
    int128_t decodedSignedVal = Simple8bTypeUtil::decodeInt128(unsignedVal);
    ASSERT_EQUALS(decodedSignedVal, signedVal);
}
