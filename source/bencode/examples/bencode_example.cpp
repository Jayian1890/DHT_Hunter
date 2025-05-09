#include "bencode/bencode.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>

int main() {
    // Create a bencode dictionary
    bencode::BencodeValue::Dictionary dict;
    
    // Add a string value
    dict["name"] = std::make_shared<bencode::BencodeValue>("example");
    
    // Add an integer value
    dict["version"] = std::make_shared<bencode::BencodeValue>(1);
    
    // Add a list value
    bencode::BencodeValue::List list;
    list.push_back(std::make_shared<bencode::BencodeValue>("item1"));
    list.push_back(std::make_shared<bencode::BencodeValue>("item2"));
    list.push_back(std::make_shared<bencode::BencodeValue>(42));
    dict["items"] = std::make_shared<bencode::BencodeValue>(list);
    
    // Add a nested dictionary
    bencode::BencodeValue::Dictionary nestedDict;
    nestedDict["key1"] = std::make_shared<bencode::BencodeValue>("value1");
    nestedDict["key2"] = std::make_shared<bencode::BencodeValue>(2);
    dict["metadata"] = std::make_shared<bencode::BencodeValue>(nestedDict);
    
    // Create a bencode value from the dictionary
    bencode::BencodeValue value(dict);
    
    // Encode the value to a string
    std::string encoded = value.encode();
    
    // Print the encoded string
    std::cout << "Encoded: " << encoded << std::endl;
    
    // Decode the string back to a bencode value
    auto decoded = bencode::BencodeDecoder::decode(encoded);
    
    // Access values from the decoded dictionary
    std::cout << "Decoded values:" << std::endl;
    std::cout << "name: " << decoded->getString("name").value_or("not found") << std::endl;
    std::cout << "version: " << decoded->getInteger("version").value_or(-1) << std::endl;
    
    // Access list values
    auto items = decoded->getList("items");
    if (items) {
        std::cout << "items:" << std::endl;
        for (size_t i = 0; i < items->size(); ++i) {
            auto item = (*items)[i];
            if (item->isString()) {
                std::cout << "  " << i << ": " << item->getString() << std::endl;
            } else if (item->isInteger()) {
                std::cout << "  " << i << ": " << item->getInteger() << std::endl;
            }
        }
    }
    
    // Access nested dictionary values
    auto metadata = decoded->getDictionary("metadata");
    if (metadata) {
        std::cout << "metadata:" << std::endl;
        for (const auto& [key, value] : *metadata) {
            if (value->isString()) {
                std::cout << "  " << key << ": " << value->getString() << std::endl;
            } else if (value->isInteger()) {
                std::cout << "  " << key << ": " << value->getInteger() << std::endl;
            }
        }
    }
    
    return 0;
}
