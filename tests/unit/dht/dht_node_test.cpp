#include <gtest/gtest.h>
#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/network/platform/socket_impl.hpp"
#include <future>
#include <chrono>

namespace dht_hunter::dht::test {

class DHTNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate a random node ID for testing
        m_nodeID = generateRandomNodeID();
        m_port = 6881;
        m_dhtNode = std::make_unique<DHTNode>(m_port, m_nodeID);
    }

    void TearDown() override {
        if (m_dhtNode && m_dhtNode->isRunning()) {
            m_dhtNode->stop();
        }
    }

    NodeID m_nodeID;
    uint16_t m_port;
    std::unique_ptr<DHTNode> m_dhtNode;
};

TEST_F(DHTNodeTest, InitialState) {
    EXPECT_EQ(m_dhtNode->getNodeID(), m_nodeID);
    EXPECT_EQ(m_dhtNode->getPort(), m_port);
    EXPECT_FALSE(m_dhtNode->isRunning());
    EXPECT_TRUE(m_dhtNode->getRoutingTable().isEmpty());
}

TEST_F(DHTNodeTest, StartStop) {
    // Start the DHT node
    EXPECT_TRUE(m_dhtNode->start());
    EXPECT_TRUE(m_dhtNode->isRunning());

    // Stop the DHT node
    m_dhtNode->stop();
    EXPECT_FALSE(m_dhtNode->isRunning());
}

// Note: The following tests require a network connection and may fail if the port is in use
// or if there are firewall restrictions. They are disabled by default.

TEST_F(DHTNodeTest, DISABLED_Ping) {
    // Start the DHT node
    EXPECT_TRUE(m_dhtNode->start());

    // Create a promise to wait for the response
    std::promise<bool> responsePromise;
    auto responseFuture = responsePromise.get_future();

    // Ping a known DHT node
    auto endpoint = network::EndPoint("router.bittorrent.com:6881");
    m_dhtNode->ping(endpoint,
                   [&responsePromise](std::shared_ptr<ResponseMessage> /* response */) {
                       responsePromise.set_value(true);
                   },
                   [&responsePromise](std::shared_ptr<ErrorMessage> /* error */) {
                       responsePromise.set_value(false);
                   },
                   [&responsePromise]() {
                       responsePromise.set_value(false);
                   });

    // Wait for the response
    EXPECT_TRUE(responseFuture.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    EXPECT_TRUE(responseFuture.get());
}

TEST_F(DHTNodeTest, DISABLED_FindNode) {
    // Start the DHT node
    EXPECT_TRUE(m_dhtNode->start());

    // Create a promise to wait for the response
    std::promise<bool> responsePromise;
    auto responseFuture = responsePromise.get_future();

    // Find nodes close to a random target
    auto targetID = generateRandomNodeID();
    auto endpoint = network::EndPoint("router.bittorrent.com:6881");
    m_dhtNode->findNode(targetID, endpoint,
                       [&responsePromise](std::shared_ptr<ResponseMessage> response) {
                           auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                           responsePromise.set_value(findNodeResponse && !findNodeResponse->getNodes().empty());
                       },
                       [&responsePromise](std::shared_ptr<ErrorMessage> /* error */) {
                           responsePromise.set_value(false);
                       },
                       [&responsePromise]() {
                           responsePromise.set_value(false);
                       });

    // Wait for the response
    EXPECT_TRUE(responseFuture.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    EXPECT_TRUE(responseFuture.get());
}

TEST_F(DHTNodeTest, DISABLED_Bootstrap) {
    // Start the DHT node
    EXPECT_TRUE(m_dhtNode->start());

    // Bootstrap using a known DHT node
    auto endpoint = network::EndPoint("router.bittorrent.com:6881");
    EXPECT_TRUE(m_dhtNode->bootstrap(endpoint));

    // Check that we have some nodes in our routing table
    EXPECT_GT(m_dhtNode->getRoutingTable().size(), 0);
}

} // namespace dht_hunter::dht::test
