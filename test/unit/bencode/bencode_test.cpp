#include "dht_hunter/bencode/bencode.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

using namespace dht_hunter::bencode;

// Test fixture for bencode tests
class BencodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
    }

    void TearDown() override {
        // Clean up test data
    }
};

// Test BencodeValue constructors and type checking
TEST_F(BencodeTest, BencodeValueConstructors) {
    // String constructor
    BencodeValue strValue("test");
    EXPECT_TRUE(strValue.isString());
    EXPECT_FALSE(strValue.isInteger());
    EXPECT_FALSE(strValue.isList());
    EXPECT_FALSE(strValue.isDictionary());
    EXPECT_EQ(strValue.getString(), "test");

    // Integer constructor
    BencodeValue intValue(123);
    EXPECT_FALSE(intValue.isString());
    EXPECT_TRUE(intValue.isInteger());
    EXPECT_FALSE(intValue.isList());
    EXPECT_FALSE(intValue.isDictionary());
    EXPECT_EQ(intValue.getInteger(), 123);

    // List constructor
    BencodeList list;
    list.push_back(std::make_shared<BencodeValue>("item1"));
    list.push_back(std::make_shared<BencodeValue>(456));
    BencodeValue listValue(list);
    EXPECT_FALSE(listValue.isString());
    EXPECT_FALSE(listValue.isInteger());
    EXPECT_TRUE(listValue.isList());
    EXPECT_FALSE(listValue.isDictionary());
    EXPECT_EQ(listValue.getList().size(), 2);

    // Dictionary constructor
    BencodeDict dict;
    dict["key1"] = std::make_shared<BencodeValue>("value1");
    dict["key2"] = std::make_shared<BencodeValue>(789);
    BencodeValue dictValue(dict);
    EXPECT_FALSE(dictValue.isString());
    EXPECT_FALSE(dictValue.isInteger());
    EXPECT_FALSE(dictValue.isList());
    EXPECT_TRUE(dictValue.isDictionary());
    EXPECT_EQ(dictValue.getDictionary().size(), 2);
}

// Test BencodeValue getters with invalid types
TEST_F(BencodeTest, BencodeValueGettersInvalidType) {
    BencodeValue strValue("test");
    EXPECT_THROW(strValue.getInteger(), BencodeException);
    EXPECT_THROW(strValue.getList(), BencodeException);
    EXPECT_THROW(strValue.getDictionary(), BencodeException);

    BencodeValue intValue(123);
    EXPECT_THROW(intValue.getString(), BencodeException);
    EXPECT_THROW(intValue.getList(), BencodeException);
    EXPECT_THROW(intValue.getDictionary(), BencodeException);

    BencodeList list;
    BencodeValue listValue(list);
    EXPECT_THROW(listValue.getString(), BencodeException);
    EXPECT_THROW(listValue.getInteger(), BencodeException);
    EXPECT_THROW(listValue.getDictionary(), BencodeException);

    BencodeDict dict;
    BencodeValue dictValue(dict);
    EXPECT_THROW(dictValue.getString(), BencodeException);
    EXPECT_THROW(dictValue.getInteger(), BencodeException);
    EXPECT_THROW(dictValue.getList(), BencodeException);
}

// Test BencodeValue dictionary operations
TEST_F(BencodeTest, BencodeValueDictionaryOperations) {
    BencodeDict dict;
    BencodeValue dictValue(dict);

    // Test set methods
    dictValue.setString("str_key", "str_value");
    dictValue.setInteger("int_key", 123);
    
    BencodeList list;
    list.push_back(std::make_shared<BencodeValue>("item1"));
    dictValue.setList("list_key", list);
    
    BencodeDict innerDict;
    innerDict["inner_key"] = std::make_shared<BencodeValue>("inner_value");
    dictValue.setDictionary("dict_key", innerDict);

    // Test get methods
    EXPECT_EQ(dictValue.getString("str_key").value(), "str_value");
    EXPECT_EQ(dictValue.getInteger("int_key").value(), 123);
    EXPECT_EQ(dictValue.getList("list_key").value().size(), 1);
    EXPECT_EQ(dictValue.getDictionary("dict_key").value().size(), 1);

    // Test non-existent keys
    EXPECT_FALSE(dictValue.getString("non_existent").has_value());
    EXPECT_FALSE(dictValue.getInteger("non_existent").has_value());
    EXPECT_FALSE(dictValue.getList("non_existent").has_value());
    EXPECT_FALSE(dictValue.getDictionary("non_existent").has_value());

    // Test wrong type keys
    EXPECT_FALSE(dictValue.getString("int_key").has_value());
    EXPECT_FALSE(dictValue.getInteger("str_key").has_value());
}

// Test BencodeValue list operations
TEST_F(BencodeTest, BencodeValueListOperations) {
    BencodeList list;
    BencodeValue listValue(list);

    // Test add methods
    listValue.addString("str_value");
    listValue.addInteger(123);
    
    BencodeList innerList;
    innerList.push_back(std::make_shared<BencodeValue>("inner_item"));
    listValue.addList(innerList);
    
    BencodeDict dict;
    dict["key"] = std::make_shared<BencodeValue>("value");
    listValue.addDictionary(dict);

    // Verify list contents
    const auto& resultList = listValue.getList();
    EXPECT_EQ(resultList.size(), 4);
    EXPECT_TRUE(resultList[0]->isString());
    EXPECT_EQ(resultList[0]->getString(), "str_value");
    EXPECT_TRUE(resultList[1]->isInteger());
    EXPECT_EQ(resultList[1]->getInteger(), 123);
    EXPECT_TRUE(resultList[2]->isList());
    EXPECT_EQ(resultList[2]->getList().size(), 1);
    EXPECT_TRUE(resultList[3]->isDictionary());
    EXPECT_EQ(resultList[3]->getDictionary().size(), 1);
}

// Test BencodeValue invalid operations
TEST_F(BencodeTest, BencodeValueInvalidOperations) {
    BencodeValue strValue("test");
    EXPECT_THROW(strValue.set("key", std::make_shared<BencodeValue>("value")), BencodeException);
    EXPECT_THROW(strValue.add(std::make_shared<BencodeValue>("value")), BencodeException);

    BencodeValue intValue(123);
    EXPECT_THROW(intValue.set("key", std::make_shared<BencodeValue>("value")), BencodeException);
    EXPECT_THROW(intValue.add(std::make_shared<BencodeValue>("value")), BencodeException);

    BencodeList list;
    BencodeValue listValue(list);
    EXPECT_THROW(listValue.set("key", std::make_shared<BencodeValue>("value")), BencodeException);

    BencodeDict dict;
    BencodeValue dictValue(dict);
    EXPECT_THROW(dictValue.add(std::make_shared<BencodeValue>("value")), BencodeException);
}

// Test BencodeEncoder
TEST_F(BencodeTest, BencodeEncoder) {
    // Test string encoding
    EXPECT_EQ(BencodeEncoder::encodeString("test"), "4:test");
    EXPECT_EQ(BencodeEncoder::encodeString(""), "0:");

    // Test integer encoding
    EXPECT_EQ(BencodeEncoder::encodeInteger(123), "i123e");
    EXPECT_EQ(BencodeEncoder::encodeInteger(-456), "i-456e");
    EXPECT_EQ(BencodeEncoder::encodeInteger(0), "i0e");

    // Test list encoding
    BencodeList list;
    list.push_back(std::make_shared<BencodeValue>("item1"));
    list.push_back(std::make_shared<BencodeValue>(456));
    EXPECT_EQ(BencodeEncoder::encodeList(list), "l5:item1i456ee");

    // Test dictionary encoding
    BencodeDict dict;
    dict["key1"] = std::make_shared<BencodeValue>("value1");
    dict["key2"] = std::make_shared<BencodeValue>(789);
    // Note: Dictionary keys are sorted in the encoder
    EXPECT_EQ(BencodeEncoder::encodeDictionary(dict), "d4:key16:value14:key2i789ee");

    // Test complex value encoding
    BencodeValue complexValue;
    complexValue.setDictionary(dict);
    EXPECT_EQ(BencodeEncoder::encode(complexValue), "d4:key16:value14:key2i789ee");
}

// Test BencodeDecoder
TEST_F(BencodeTest, BencodeDecoder) {
    // Test string decoding
    size_t pos = 0;
    std::string strData = "4:test";
    EXPECT_EQ(BencodeDecoder::decodeString(strData, pos), "test");
    EXPECT_EQ(pos, strData.size());

    // Test integer decoding
    pos = 0;
    std::string intData = "i123e";
    EXPECT_EQ(BencodeDecoder::decodeInteger(intData, pos), 123);
    EXPECT_EQ(pos, intData.size());

    // Test list decoding
    pos = 0;
    std::string listData = "l5:item1i456ee";
    auto list = BencodeDecoder::decodeList(listData, pos);
    EXPECT_EQ(pos, listData.size());
    EXPECT_EQ(list.size(), 2);
    EXPECT_TRUE(list[0]->isString());
    EXPECT_EQ(list[0]->getString(), "item1");
    EXPECT_TRUE(list[1]->isInteger());
    EXPECT_EQ(list[1]->getInteger(), 456);

    // Test dictionary decoding
    pos = 0;
    std::string dictData = "d4:key16:value14:key2i789ee";
    auto dict = BencodeDecoder::decodeDictionary(dictData, pos);
    EXPECT_EQ(pos, dictData.size());
    EXPECT_EQ(dict.size(), 2);
    EXPECT_TRUE(dict["key1"]->isString());
    EXPECT_EQ(dict["key1"]->getString(), "value1");
    EXPECT_TRUE(dict["key2"]->isInteger());
    EXPECT_EQ(dict["key2"]->getInteger(), 789);

    // Test complete decode
    std::string complexData = "d4:key16:value14:key2i789ee";
    auto complexValue = BencodeDecoder::decode(complexData);
    EXPECT_TRUE(complexValue->isDictionary());
    EXPECT_EQ(complexValue->getDictionary().size(), 2);
}

// Test error handling in BencodeDecoder
TEST_F(BencodeTest, BencodeDecoderErrors) {
    // Test invalid string format
    size_t pos = 0;
    std::string invalidString = "4test";
    EXPECT_THROW(BencodeDecoder::decodeString(invalidString, pos), BencodeException);

    // Test string length exceeds data
    pos = 0;
    std::string shortString = "10:test";
    EXPECT_THROW(BencodeDecoder::decodeString(shortString, pos), BencodeException);

    // Test invalid integer format
    pos = 0;
    std::string invalidInt = "i123";
    EXPECT_THROW(BencodeDecoder::decodeInteger(invalidInt, pos), BencodeException);

    // Test invalid list format
    pos = 0;
    std::string invalidList = "l5:item1i456e";
    EXPECT_THROW(BencodeDecoder::decodeList(invalidList, pos), BencodeException);

    // Test invalid dictionary format
    pos = 0;
    std::string invalidDict = "d4:key16:value14:key2i789e";
    EXPECT_THROW(BencodeDecoder::decodeDictionary(invalidDict, pos), BencodeException);

    // Test invalid bencode data
    std::string invalidData = "x4:test";
    EXPECT_THROW(BencodeDecoder::decode(invalidData), BencodeException);
}

// Test round-trip encoding and decoding
TEST_F(BencodeTest, RoundTripEncodingDecoding) {
    // Create a complex bencode value
    BencodeDict dict;
    dict["string"] = std::make_shared<BencodeValue>("test");
    dict["integer"] = std::make_shared<BencodeValue>(123);
    
    BencodeList innerList;
    innerList.push_back(std::make_shared<BencodeValue>("item1"));
    innerList.push_back(std::make_shared<BencodeValue>(456));
    dict["list"] = std::make_shared<BencodeValue>(innerList);
    
    BencodeDict innerDict;
    innerDict["inner_key"] = std::make_shared<BencodeValue>("inner_value");
    dict["dict"] = std::make_shared<BencodeValue>(innerDict);
    
    BencodeValue originalValue(dict);
    
    // Encode the value
    std::string encoded = BencodeEncoder::encode(originalValue);
    
    // Decode the encoded string
    auto decodedValue = BencodeDecoder::decode(encoded);
    
    // Verify the decoded value matches the original
    EXPECT_TRUE(decodedValue->isDictionary());
    const auto& decodedDict = decodedValue->getDictionary();
    EXPECT_EQ(decodedDict.size(), 4);
    
    EXPECT_TRUE(decodedDict.at("string")->isString());
    EXPECT_EQ(decodedDict.at("string")->getString(), "test");
    
    EXPECT_TRUE(decodedDict.at("integer")->isInteger());
    EXPECT_EQ(decodedDict.at("integer")->getInteger(), 123);
    
    EXPECT_TRUE(decodedDict.at("list")->isList());
    const auto& decodedList = decodedDict.at("list")->getList();
    EXPECT_EQ(decodedList.size(), 2);
    EXPECT_EQ(decodedList[0]->getString(), "item1");
    EXPECT_EQ(decodedList[1]->getInteger(), 456);
    
    EXPECT_TRUE(decodedDict.at("dict")->isDictionary());
    const auto& decodedInnerDict = decodedDict.at("dict")->getDictionary();
    EXPECT_EQ(decodedInnerDict.size(), 1);
    EXPECT_EQ(decodedInnerDict.at("inner_key")->getString(), "inner_value");
}

// Test edge cases
TEST_F(BencodeTest, EdgeCases) {
    // Empty string
    BencodeValue emptyStr("");
    std::string encodedEmptyStr = BencodeEncoder::encode(emptyStr);
    EXPECT_EQ(encodedEmptyStr, "0:");
    auto decodedEmptyStr = BencodeDecoder::decode(encodedEmptyStr);
    EXPECT_TRUE(decodedEmptyStr->isString());
    EXPECT_EQ(decodedEmptyStr->getString(), "");

    // Empty list
    BencodeList emptyList;
    BencodeValue emptyListValue(emptyList);
    std::string encodedEmptyList = BencodeEncoder::encode(emptyListValue);
    EXPECT_EQ(encodedEmptyList, "le");
    auto decodedEmptyList = BencodeDecoder::decode(encodedEmptyList);
    EXPECT_TRUE(decodedEmptyList->isList());
    EXPECT_EQ(decodedEmptyList->getList().size(), 0);

    // Empty dictionary
    BencodeDict emptyDict;
    BencodeValue emptyDictValue(emptyDict);
    std::string encodedEmptyDict = BencodeEncoder::encode(emptyDictValue);
    EXPECT_EQ(encodedEmptyDict, "de");
    auto decodedEmptyDict = BencodeDecoder::decode(encodedEmptyDict);
    EXPECT_TRUE(decodedEmptyDict->isDictionary());
    EXPECT_EQ(decodedEmptyDict->getDictionary().size(), 0);

    // Large integer
    BencodeValue largeInt(9223372036854775807LL); // Max int64_t
    std::string encodedLargeInt = BencodeEncoder::encode(largeInt);
    EXPECT_EQ(encodedLargeInt, "i9223372036854775807e");
    auto decodedLargeInt = BencodeDecoder::decode(encodedLargeInt);
    EXPECT_TRUE(decodedLargeInt->isInteger());
    EXPECT_EQ(decodedLargeInt->getInteger(), 9223372036854775807LL);

    // Negative integer
    BencodeValue negInt(-9223372036854775807LL);
    std::string encodedNegInt = BencodeEncoder::encode(negInt);
    EXPECT_EQ(encodedNegInt, "i-9223372036854775807e");
    auto decodedNegInt = BencodeDecoder::decode(encodedNegInt);
    EXPECT_TRUE(decodedNegInt->isInteger());
    EXPECT_EQ(decodedNegInt->getInteger(), -9223372036854775807LL);
}

// Test assignment operators
TEST_F(BencodeTest, AssignmentOperators) {
    // String assignment
    BencodeValue value;
    value = std::string("test");
    EXPECT_TRUE(value.isString());
    EXPECT_EQ(value.getString(), "test");

    // Integer assignment
    value = 123LL;
    EXPECT_TRUE(value.isInteger());
    EXPECT_EQ(value.getInteger(), 123);

    // List assignment
    BencodeList list;
    list.push_back(std::make_shared<BencodeValue>("item"));
    value = list;
    EXPECT_TRUE(value.isList());
    EXPECT_EQ(value.getList().size(), 1);

    // Dictionary assignment
    BencodeDict dict;
    dict["key"] = std::make_shared<BencodeValue>("value");
    value = dict;
    EXPECT_TRUE(value.isDictionary());
    EXPECT_EQ(value.getDictionary().size(), 1);
}

// Test BencodeParser
TEST_F(BencodeTest, BencodeParser) {
    BencodeParser parser;
    
    // Parse string
    BencodeValue strValue = parser.parse("4:test");
    EXPECT_TRUE(strValue.isString());
    EXPECT_EQ(strValue.getString(), "test");
    
    // Parse integer
    BencodeValue intValue = parser.parse("i123e");
    EXPECT_TRUE(intValue.isInteger());
    EXPECT_EQ(intValue.getInteger(), 123);
    
    // Parse list
    BencodeValue listValue = parser.parse("l5:item1i456ee");
    EXPECT_TRUE(listValue.isList());
    EXPECT_EQ(listValue.getList().size(), 2);
    
    // Parse dictionary
    BencodeValue dictValue = parser.parse("d4:key16:value14:key2i789ee");
    EXPECT_TRUE(dictValue.isDictionary());
    EXPECT_EQ(dictValue.getDictionary().size(), 2);
    
    // Test error handling
    EXPECT_THROW(parser.parse("invalid"), BencodeException);
}
