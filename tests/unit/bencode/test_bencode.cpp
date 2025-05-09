#include "dht_hunter/bencode/bencode.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>

using namespace bencode;

/**
 * @brief Test the BencodeValue constructors and type checking
 * @return True if all tests pass, false otherwise
 */
bool testBencodeValueConstructors() {
    std::cout << "Testing BencodeValue constructors..." << std::endl;
    
    // Test default constructor (creates a string value)
    BencodeValue defaultValue;
    if (!defaultValue.isString()) {
        std::cerr << "Default constructor did not create a string value" << std::endl;
        return false;
    }
    
    // Test string constructor
    BencodeValue stringValue("test");
    if (!stringValue.isString()) {
        std::cerr << "String constructor did not create a string value" << std::endl;
        return false;
    }
    if (stringValue.getString() != "test") {
        std::cerr << "String constructor created incorrect value" << std::endl;
        std::cerr << "Expected: 'test', Got: '" << stringValue.getString() << "'" << std::endl;
        return false;
    }
    
    // Test integer constructor
    BencodeValue intValue(42);
    if (!intValue.isInteger()) {
        std::cerr << "Integer constructor did not create an integer value" << std::endl;
        return false;
    }
    if (intValue.getInteger() != 42) {
        std::cerr << "Integer constructor created incorrect value" << std::endl;
        std::cerr << "Expected: 42, Got: " << intValue.getInteger() << std::endl;
        return false;
    }
    
    // Test list constructor
    BencodeValue::List list;
    list.push_back(std::make_shared<BencodeValue>("item1"));
    list.push_back(std::make_shared<BencodeValue>(123));
    BencodeValue listValue(list);
    if (!listValue.isList()) {
        std::cerr << "List constructor did not create a list value" << std::endl;
        return false;
    }
    if (listValue.getList().size() != 2) {
        std::cerr << "List constructor created incorrect list size" << std::endl;
        std::cerr << "Expected: 2, Got: " << listValue.getList().size() << std::endl;
        return false;
    }
    
    // Test dictionary constructor
    BencodeValue::Dictionary dict;
    dict["key1"] = std::make_shared<BencodeValue>("value1");
    dict["key2"] = std::make_shared<BencodeValue>(456);
    BencodeValue dictValue(dict);
    if (!dictValue.isDictionary()) {
        std::cerr << "Dictionary constructor did not create a dictionary value" << std::endl;
        return false;
    }
    if (dictValue.getDict().size() != 2) {
        std::cerr << "Dictionary constructor created incorrect dictionary size" << std::endl;
        std::cerr << "Expected: 2, Got: " << dictValue.getDict().size() << std::endl;
        return false;
    }
    
    std::cout << "BencodeValue constructors tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the BencodeValue getters and setters
 * @return True if all tests pass, false otherwise
 */
bool testBencodeValueGettersSetters() {
    std::cout << "Testing BencodeValue getters and setters..." << std::endl;
    
    // Test string getters and setters
    BencodeValue value;
    value.setString("test");
    if (value.getString() != "test") {
        std::cerr << "setString/getString failed" << std::endl;
        std::cerr << "Expected: 'test', Got: '" << value.getString() << "'" << std::endl;
        return false;
    }
    
    // Test integer getters and setters
    value.setInteger(42);
    if (value.getInteger() != 42) {
        std::cerr << "setInteger/getInteger failed" << std::endl;
        std::cerr << "Expected: 42, Got: " << value.getInteger() << std::endl;
        return false;
    }
    
    // Test list getters and setters
    BencodeValue::List list;
    list.push_back(std::make_shared<BencodeValue>("item1"));
    list.push_back(std::make_shared<BencodeValue>(123));
    value.setList(list);
    if (value.getList().size() != 2) {
        std::cerr << "setList/getList failed" << std::endl;
        std::cerr << "Expected list size: 2, Got: " << value.getList().size() << std::endl;
        return false;
    }
    
    // Test dictionary getters and setters
    BencodeValue::Dictionary dict;
    dict["key1"] = std::make_shared<BencodeValue>("value1");
    dict["key2"] = std::make_shared<BencodeValue>(456);
    value.setDict(dict);
    if (value.getDict().size() != 2) {
        std::cerr << "setDict/getDict failed" << std::endl;
        std::cerr << "Expected dictionary size: 2, Got: " << value.getDict().size() << std::endl;
        return false;
    }
    
    // Test list add methods
    value.setList(BencodeValue::List());
    value.addString("item1");
    value.addInteger(123);
    if (value.getList().size() != 2) {
        std::cerr << "addString/addInteger failed" << std::endl;
        std::cerr << "Expected list size: 2, Got: " << value.getList().size() << std::endl;
        return false;
    }
    
    // Test dictionary set methods
    value.setDict(BencodeValue::Dictionary());
    value.setString("key1", "value1");
    value.setInteger("key2", 456);
    if (value.getDict().size() != 2) {
        std::cerr << "setString/setInteger for dictionary failed" << std::endl;
        std::cerr << "Expected dictionary size: 2, Got: " << value.getDict().size() << std::endl;
        return false;
    }
    
    std::cout << "BencodeValue getters and setters tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the BencodeEncoder
 * @return True if all tests pass, false otherwise
 */
bool testBencodeEncoder() {
    std::cout << "Testing BencodeEncoder..." << std::endl;
    
    // Test string encoding
    std::string encodedString = BencodeEncoder::encodeString("test");
    if (encodedString != "4:test") {
        std::cerr << "encodeString failed" << std::endl;
        std::cerr << "Expected: '4:test', Got: '" << encodedString << "'" << std::endl;
        return false;
    }
    
    // Test integer encoding
    std::string encodedInt = BencodeEncoder::encodeInteger(42);
    if (encodedInt != "i42e") {
        std::cerr << "encodeInteger failed" << std::endl;
        std::cerr << "Expected: 'i42e', Got: '" << encodedInt << "'" << std::endl;
        return false;
    }
    
    // Test list encoding
    BencodeValue::List list;
    list.push_back(std::make_shared<BencodeValue>("item1"));
    list.push_back(std::make_shared<BencodeValue>(123));
    std::string encodedList = BencodeEncoder::encodeList(list);
    if (encodedList != "l5:item1i123ee") {
        std::cerr << "encodeList failed" << std::endl;
        std::cerr << "Expected: 'l5:item1i123ee', Got: '" << encodedList << "'" << std::endl;
        return false;
    }
    
    // Test dictionary encoding
    BencodeValue::Dictionary dict;
    dict["key1"] = std::make_shared<BencodeValue>("value1");
    dict["key2"] = std::make_shared<BencodeValue>(456);
    std::string encodedDict = BencodeEncoder::encodeDictionary(dict);
    if (encodedDict != "d4:key16:value14:key2i456ee") {
        std::cerr << "encodeDictionary failed" << std::endl;
        std::cerr << "Expected: 'd4:key16:value14:key2i456ee', Got: '" << encodedDict << "'" << std::endl;
        return false;
    }
    
    // Test BencodeValue encode
    BencodeValue value(dict);
    std::string encoded = value.encode();
    if (encoded != "d4:key16:value14:key2i456ee") {
        std::cerr << "BencodeValue::encode failed" << std::endl;
        std::cerr << "Expected: 'd4:key16:value14:key2i456ee', Got: '" << encoded << "'" << std::endl;
        return false;
    }
    
    std::cout << "BencodeEncoder tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the BencodeDecoder
 * @return True if all tests pass, false otherwise
 */
bool testBencodeDecoder() {
    std::cout << "Testing BencodeDecoder..." << std::endl;
    
    // Test string decoding
    std::string encodedString = "4:test";
    size_t pos = 0;
    std::string decodedString = BencodeDecoder::decodeString(encodedString, pos);
    if (decodedString != "test") {
        std::cerr << "decodeString failed" << std::endl;
        std::cerr << "Expected: 'test', Got: '" << decodedString << "'" << std::endl;
        return false;
    }
    
    // Test integer decoding
    std::string encodedInt = "i42e";
    pos = 0;
    int64_t decodedInt = BencodeDecoder::decodeInteger(encodedInt, pos);
    if (decodedInt != 42) {
        std::cerr << "decodeInteger failed" << std::endl;
        std::cerr << "Expected: 42, Got: " << decodedInt << std::endl;
        return false;
    }
    
    // Test list decoding
    std::string encodedList = "l5:item1i123ee";
    pos = 0;
    BencodeValue::List decodedList = BencodeDecoder::decodeList(encodedList, pos);
    if (decodedList.size() != 2) {
        std::cerr << "decodeList failed" << std::endl;
        std::cerr << "Expected list size: 2, Got: " << decodedList.size() << std::endl;
        return false;
    }
    
    // Test dictionary decoding
    std::string encodedDict = "d4:key16:value14:key2i456ee";
    pos = 0;
    BencodeValue::Dictionary decodedDict = BencodeDecoder::decodeDictionary(encodedDict, pos);
    if (decodedDict.size() != 2) {
        std::cerr << "decodeDictionary failed" << std::endl;
        std::cerr << "Expected dictionary size: 2, Got: " << decodedDict.size() << std::endl;
        return false;
    }
    
    // Test BencodeDecoder::decode
    auto decodedValue = BencodeDecoder::decode(encodedDict);
    if (!decodedValue->isDictionary()) {
        std::cerr << "BencodeDecoder::decode failed to create a dictionary value" << std::endl;
        return false;
    }
    if (decodedValue->getDict().size() != 2) {
        std::cerr << "BencodeDecoder::decode created incorrect dictionary size" << std::endl;
        std::cerr << "Expected: 2, Got: " << decodedValue->getDict().size() << std::endl;
        return false;
    }
    
    std::cout << "BencodeDecoder tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the full encode/decode cycle
 * @return True if all tests pass, false otherwise
 */
bool testEncodeDecodeCycle() {
    std::cout << "Testing encode/decode cycle..." << std::endl;
    
    // Create a complex bencode value
    BencodeValue::Dictionary dict;
    dict["name"] = std::make_shared<BencodeValue>("example");
    dict["version"] = std::make_shared<BencodeValue>(1);
    
    BencodeValue::List list;
    list.push_back(std::make_shared<BencodeValue>("item1"));
    list.push_back(std::make_shared<BencodeValue>("item2"));
    list.push_back(std::make_shared<BencodeValue>(42));
    dict["items"] = std::make_shared<BencodeValue>(list);
    
    BencodeValue::Dictionary nestedDict;
    nestedDict["key1"] = std::make_shared<BencodeValue>("value1");
    nestedDict["key2"] = std::make_shared<BencodeValue>(2);
    dict["metadata"] = std::make_shared<BencodeValue>(nestedDict);
    
    BencodeValue value(dict);
    
    // Encode the value
    std::string encoded = value.encode();
    
    // Decode the encoded string
    auto decoded = BencodeDecoder::decode(encoded);
    
    // Verify the decoded value
    if (!decoded->isDictionary()) {
        std::cerr << "Decoded value is not a dictionary" << std::endl;
        return false;
    }
    
    auto nameOpt = decoded->getString("name");
    if (!nameOpt || *nameOpt != "example") {
        std::cerr << "Decoded name is incorrect" << std::endl;
        std::cerr << "Expected: 'example', Got: " << (nameOpt ? *nameOpt : "not found") << std::endl;
        return false;
    }
    
    auto versionOpt = decoded->getInteger("version");
    if (!versionOpt || *versionOpt != 1) {
        std::cerr << "Decoded version is incorrect" << std::endl;
        std::cerr << "Expected: 1, Got: " << (versionOpt ? *versionOpt : -1) << std::endl;
        return false;
    }
    
    auto itemsOpt = decoded->getList("items");
    if (!itemsOpt || itemsOpt->size() != 3) {
        std::cerr << "Decoded items list is incorrect" << std::endl;
        std::cerr << "Expected size: 3, Got: " << (itemsOpt ? itemsOpt->size() : 0) << std::endl;
        return false;
    }
    
    auto metadataOpt = decoded->getDictionary("metadata");
    if (!metadataOpt || metadataOpt->size() != 2) {
        std::cerr << "Decoded metadata dictionary is incorrect" << std::endl;
        std::cerr << "Expected size: 2, Got: " << (metadataOpt ? metadataOpt->size() : 0) << std::endl;
        return false;
    }
    
    std::cout << "Encode/decode cycle tests passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testBencodeValueConstructors();
    allTestsPassed &= testBencodeValueGettersSetters();
    allTestsPassed &= testBencodeEncoder();
    allTestsPassed &= testBencodeDecoder();
    allTestsPassed &= testEncodeDecodeCycle();
    
    if (allTestsPassed) {
        std::cout << "All Bencode tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some Bencode tests failed!" << std::endl;
        return 1;
    }
}
