#include "utils/domain_utils.hpp"
#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/types/info_hash_metadata.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cassert>

using namespace dht_hunter::utility;
using namespace dht_hunter::types;

/**
 * @brief Test the network utilities
 * @return True if all tests pass, false otherwise
 */
bool testNetworkUtils() {
    std::cout << "Testing network utilities..." << std::endl;
    
    // Test getUserAgent
    std::string userAgent = network::getUserAgent();
    if (userAgent.empty()) {
        std::cerr << "getUserAgent returned empty string" << std::endl;
        return false;
    }
    
    std::cout << "Network utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the node utilities
 * @return True if all tests pass, false otherwise
 */
bool testNodeUtils() {
    std::cout << "Testing node utilities..." << std::endl;
    
    // Create some test nodes
    NodeID targetID = NodeID::random();
    
    std::vector<std::shared_ptr<Node>> nodes;
    for (int i = 0; i < 5; ++i) {
        NodeID nodeID = NodeID::random();
        NetworkAddress addr("192.168.1." + std::to_string(i + 1));
        EndPoint endpoint(addr, 6881);
        auto node = std::make_shared<Node>(nodeID, endpoint);
        nodes.push_back(node);
    }
    
    // Test sortNodesByDistance
    auto sortedNodes = node::sortNodesByDistance(nodes, targetID);
    if (sortedNodes.size() != nodes.size()) {
        std::cerr << "sortNodesByDistance returned incorrect number of nodes" << std::endl;
        return false;
    }
    
    // Verify that nodes are sorted by distance
    for (size_t i = 0; i < sortedNodes.size() - 1; ++i) {
        NodeID distance1 = sortedNodes[i]->getID().distanceTo(targetID);
        NodeID distance2 = sortedNodes[i + 1]->getID().distanceTo(targetID);
        if (!(distance1 < distance2)) {
            std::cerr << "sortNodesByDistance did not sort nodes correctly" << std::endl;
            return false;
        }
    }
    
    // Test findNodeByEndpoint
    EndPoint testEndpoint = nodes[2]->getEndpoint();
    auto foundNode = node::findNodeByEndpoint(nodes, testEndpoint);
    if (!foundNode) {
        std::cerr << "findNodeByEndpoint failed to find node" << std::endl;
        return false;
    }
    if (!(foundNode->getEndpoint() == testEndpoint)) {
        std::cerr << "findNodeByEndpoint returned incorrect node" << std::endl;
        return false;
    }
    
    // Test findNodeByID
    NodeID testID = nodes[3]->getID();
    foundNode = node::findNodeByID(nodes, testID);
    if (!foundNode) {
        std::cerr << "findNodeByID failed to find node" << std::endl;
        return false;
    }
    if (!(foundNode->getID() == testID)) {
        std::cerr << "findNodeByID returned incorrect node" << std::endl;
        return false;
    }
    
    // Test generateNodeLookupID
    std::string lookupID = node::generateNodeLookupID(targetID);
    if (lookupID.empty()) {
        std::cerr << "generateNodeLookupID returned empty string" << std::endl;
        return false;
    }
    if (lookupID.find("node_lookup_") != 0) {
        std::cerr << "generateNodeLookupID returned incorrect format" << std::endl;
        return false;
    }
    
    // Test generatePeerLookupID
    InfoHash infoHash;
    for (size_t i = 0; i < infoHash.size(); ++i) {
        infoHash[i] = static_cast<uint8_t>(i);
    }
    
    std::string peerLookupID = node::generatePeerLookupID(infoHash);
    if (peerLookupID.empty()) {
        std::cerr << "generatePeerLookupID returned empty string" << std::endl;
        return false;
    }
    if (peerLookupID.find("peer_lookup_") != 0) {
        std::cerr << "generatePeerLookupID returned incorrect format" << std::endl;
        return false;
    }
    
    std::cout << "Node utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the metadata utilities
 * @return True if all tests pass, false otherwise
 */
bool testMetadataUtils() {
    std::cout << "Testing metadata utilities..." << std::endl;
    
    // Create a test info hash
    InfoHash infoHash;
    for (size_t i = 0; i < infoHash.size(); ++i) {
        infoHash[i] = static_cast<uint8_t>(i);
    }
    
    // Test setInfoHashName
    std::string testName = "Test Torrent";
    bool result = metadata::setInfoHashName(infoHash, testName);
    if (!result) {
        std::cerr << "setInfoHashName failed" << std::endl;
        return false;
    }
    
    // Test getInfoHashName
    std::string name = metadata::getInfoHashName(infoHash);
    if (name != testName) {
        std::cerr << "getInfoHashName returned incorrect name" << std::endl;
        std::cerr << "Expected: '" << testName << "', Got: '" << name << "'" << std::endl;
        return false;
    }
    
    // Test addFileToInfoHash
    std::string testFilePath = "test/file.txt";
    uint64_t testFileSize = 1024;
    result = metadata::addFileToInfoHash(infoHash, testFilePath, testFileSize);
    if (!result) {
        std::cerr << "addFileToInfoHash failed" << std::endl;
        return false;
    }
    
    // Test getInfoHashFiles
    auto files = metadata::getInfoHashFiles(infoHash);
    if (files.empty()) {
        std::cerr << "getInfoHashFiles returned empty vector" << std::endl;
        return false;
    }
    if (files.size() != 1) {
        std::cerr << "getInfoHashFiles returned incorrect number of files" << std::endl;
        std::cerr << "Expected: 1, Got: " << files.size() << std::endl;
        return false;
    }
    if (files[0].path != testFilePath) {
        std::cerr << "getInfoHashFiles returned incorrect file path" << std::endl;
        std::cerr << "Expected: '" << testFilePath << "', Got: '" << files[0].path << "'" << std::endl;
        return false;
    }
    if (files[0].size != testFileSize) {
        std::cerr << "getInfoHashFiles returned incorrect file size" << std::endl;
        std::cerr << "Expected: " << testFileSize << ", Got: " << files[0].size << std::endl;
        return false;
    }
    
    // Test getInfoHashTotalSize
    uint64_t totalSize = metadata::getInfoHashTotalSize(infoHash);
    if (totalSize != testFileSize) {
        std::cerr << "getInfoHashTotalSize returned incorrect size" << std::endl;
        std::cerr << "Expected: " << testFileSize << ", Got: " << totalSize << std::endl;
        return false;
    }
    
    // Test getInfoHashMetadata
    auto metadataObj = metadata::getInfoHashMetadata(infoHash);
    if (!metadataObj) {
        std::cerr << "getInfoHashMetadata returned nullptr" << std::endl;
        return false;
    }
    if (metadataObj->getName() != testName) {
        std::cerr << "getInfoHashMetadata returned incorrect name" << std::endl;
        std::cerr << "Expected: '" << testName << "', Got: '" << metadataObj->getName() << "'" << std::endl;
        return false;
    }
    
    // Test getAllMetadata
    auto allMetadata = metadata::getAllMetadata();
    if (allMetadata.empty()) {
        std::cerr << "getAllMetadata returned empty vector" << std::endl;
        return false;
    }
    
    std::cout << "Metadata utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testNetworkUtils();
    allTestsPassed &= testNodeUtils();
    allTestsPassed &= testMetadataUtils();
    
    if (allTestsPassed) {
        std::cout << "All Domain Utils tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some Domain Utils tests failed!" << std::endl;
        return 1;
    }
}
