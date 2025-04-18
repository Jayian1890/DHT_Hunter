#include <gtest/gtest.h>
#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/message.hpp"
#include "dht_hunter/dht/query_message.hpp"
#include "dht_hunter/dht/response_message.hpp"
#include "dht_hunter/dht/error_message.hpp"

namespace dht_hunter::dht::test {

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate random node IDs and transaction ID for testing
        m_nodeID = generateRandomNodeID();
        m_targetID = generateRandomNodeID();
        m_infoHash = generateRandomNodeID(); // Reuse node ID generation for info hash
        m_transactionID = generateTransactionID();
    }
    
    NodeID m_nodeID;
    NodeID m_targetID;
    InfoHash m_infoHash;
    TransactionID m_transactionID;
};

TEST_F(MessageTest, PingQueryEncodeDecodeTest) {
    // Create a ping query
    auto query = std::make_shared<PingQuery>(m_transactionID, m_nodeID);
    
    // Encode the query
    std::string encoded = query->encodeToString();
    
    // Decode the query
    auto decoded = decodeMessage(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->getType(), MessageType::Query);
    
    // Cast to query message
    auto decodedQuery = std::dynamic_pointer_cast<QueryMessage>(decoded);
    ASSERT_NE(decodedQuery, nullptr);
    ASSERT_EQ(decodedQuery->getMethod(), QueryMethod::Ping);
    
    // Check fields
    EXPECT_EQ(decodedQuery->getTransactionID(), m_transactionID);
    EXPECT_EQ(decodedQuery->getNodeID(), m_nodeID);
}

TEST_F(MessageTest, FindNodeQueryEncodeDecodeTest) {
    // Create a find_node query
    auto query = std::make_shared<FindNodeQuery>(m_transactionID, m_nodeID, m_targetID);
    
    // Encode the query
    std::string encoded = query->encodeToString();
    
    // Decode the query
    auto decoded = decodeMessage(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->getType(), MessageType::Query);
    
    // Cast to find_node query
    auto decodedQuery = std::dynamic_pointer_cast<FindNodeQuery>(decoded);
    ASSERT_NE(decodedQuery, nullptr);
    ASSERT_EQ(decodedQuery->getMethod(), QueryMethod::FindNode);
    
    // Check fields
    EXPECT_EQ(decodedQuery->getTransactionID(), m_transactionID);
    EXPECT_EQ(decodedQuery->getNodeID(), m_nodeID);
    EXPECT_EQ(decodedQuery->getTarget(), m_targetID);
}

TEST_F(MessageTest, GetPeersQueryEncodeDecodeTest) {
    // Create a get_peers query
    auto query = std::make_shared<GetPeersQuery>(m_transactionID, m_nodeID, m_infoHash);
    
    // Encode the query
    std::string encoded = query->encodeToString();
    
    // Decode the query
    auto decoded = decodeMessage(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->getType(), MessageType::Query);
    
    // Cast to get_peers query
    auto decodedQuery = std::dynamic_pointer_cast<GetPeersQuery>(decoded);
    ASSERT_NE(decodedQuery, nullptr);
    ASSERT_EQ(decodedQuery->getMethod(), QueryMethod::GetPeers);
    
    // Check fields
    EXPECT_EQ(decodedQuery->getTransactionID(), m_transactionID);
    EXPECT_EQ(decodedQuery->getNodeID(), m_nodeID);
    EXPECT_EQ(decodedQuery->getInfoHash(), m_infoHash);
}

TEST_F(MessageTest, AnnouncePeerQueryEncodeDecodeTest) {
    // Create an announce_peer query
    std::string token = "test_token";
    uint16_t port = 6881;
    auto query = std::make_shared<AnnouncePeerQuery>(m_transactionID, m_nodeID, m_infoHash, port, token);
    
    // Encode the query
    std::string encoded = query->encodeToString();
    
    // Decode the query
    auto decoded = decodeMessage(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->getType(), MessageType::Query);
    
    // Cast to announce_peer query
    auto decodedQuery = std::dynamic_pointer_cast<AnnouncePeerQuery>(decoded);
    ASSERT_NE(decodedQuery, nullptr);
    ASSERT_EQ(decodedQuery->getMethod(), QueryMethod::AnnouncePeer);
    
    // Check fields
    EXPECT_EQ(decodedQuery->getTransactionID(), m_transactionID);
    EXPECT_EQ(decodedQuery->getNodeID(), m_nodeID);
    EXPECT_EQ(decodedQuery->getInfoHash(), m_infoHash);
    EXPECT_EQ(decodedQuery->getPort(), port);
    EXPECT_EQ(decodedQuery->getToken(), token);
    EXPECT_FALSE(decodedQuery->isImpliedPort());
}

TEST_F(MessageTest, PingResponseEncodeDecodeTest) {
    // Create a ping response
    auto response = std::make_shared<PingResponse>(m_transactionID, m_nodeID);
    
    // Encode the response
    std::string encoded = response->encodeToString();
    
    // Decode the response
    auto decoded = decodeMessage(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->getType(), MessageType::Response);
    
    // Cast to response message
    auto decodedResponse = std::dynamic_pointer_cast<ResponseMessage>(decoded);
    ASSERT_NE(decodedResponse, nullptr);
    
    // Check fields
    EXPECT_EQ(decodedResponse->getTransactionID(), m_transactionID);
    EXPECT_EQ(decodedResponse->getNodeID(), m_nodeID);
}

TEST_F(MessageTest, FindNodeResponseEncodeDecodeTest) {
    // Create some nodes
    std::vector<CompactNodeInfo> nodes;
    for (int i = 0; i < 3; i++) {
        CompactNodeInfo node;
        node.id = generateRandomNodeID();
        node.endpoint = network::EndPoint(network::NetworkAddress(0x7F000001), 6881 + i); // 127.0.0.1
        nodes.push_back(node);
    }
    
    // Create a find_node response
    auto response = std::make_shared<FindNodeResponse>(m_transactionID, m_nodeID, nodes);
    
    // Encode the response
    std::string encoded = response->encodeToString();
    
    // Decode the response
    auto decoded = decodeMessage(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->getType(), MessageType::Response);
    
    // Cast to find_node response
    auto decodedResponse = std::dynamic_pointer_cast<FindNodeResponse>(decoded);
    ASSERT_NE(decodedResponse, nullptr);
    
    // Check fields
    EXPECT_EQ(decodedResponse->getTransactionID(), m_transactionID);
    EXPECT_EQ(decodedResponse->getNodeID(), m_nodeID);
    ASSERT_EQ(decodedResponse->getNodes().size(), nodes.size());
    
    // Check nodes
    for (size_t i = 0; i < nodes.size(); i++) {
        EXPECT_EQ(decodedResponse->getNodes()[i].id, nodes[i].id);
        EXPECT_EQ(decodedResponse->getNodes()[i].endpoint.getAddress().getIPv4Address(), 
                 nodes[i].endpoint.getAddress().getIPv4Address());
        EXPECT_EQ(decodedResponse->getNodes()[i].endpoint.getPort(), 
                 nodes[i].endpoint.getPort());
    }
}

TEST_F(MessageTest, ErrorMessageEncodeDecodeTest) {
    // Create an error message
    int errorCode = 201;
    std::string errorMessage = "Test error message";
    auto error = std::make_shared<ErrorMessage>(m_transactionID, errorCode, errorMessage);
    
    // Encode the error
    std::string encoded = error->encodeToString();
    
    // Decode the error
    auto decoded = decodeMessage(encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->getType(), MessageType::Error);
    
    // Cast to error message
    auto decodedError = std::dynamic_pointer_cast<ErrorMessage>(decoded);
    ASSERT_NE(decodedError, nullptr);
    
    // Check fields
    EXPECT_EQ(decodedError->getTransactionID(), m_transactionID);
    EXPECT_EQ(decodedError->getCode(), errorCode);
    EXPECT_EQ(decodedError->getMessage(), errorMessage);
}

} // namespace dht_hunter::dht::test
