#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/crawler/crawler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace dht_hunter;

// Helper function to print crawler statistics
void printCrawlerStatistics(const dht::CrawlerStatistics& stats) {
    auto now = std::chrono::steady_clock::now();
    auto runningTime = std::chrono::duration_cast<std::chrono::seconds>(now - stats.startTime).count();
    
    std::cout << "=== Crawler Statistics ===" << std::endl;
    std::cout << "Running time: " << runningTime << " seconds" << std::endl;
    std::cout << "Nodes discovered: " << stats.nodesDiscovered << std::endl;
    std::cout << "Nodes responded: " << stats.nodesResponded << std::endl;
    std::cout << "Info hashes discovered: " << stats.infoHashesDiscovered << std::endl;
    std::cout << "Peers discovered: " << stats.peersDiscovered << std::endl;
    std::cout << "Queries sent: " << stats.queriesSent << std::endl;
    std::cout << "Responses received: " << stats.responsesReceived << std::endl;
    std::cout << "Errors received: " << stats.errorsReceived << std::endl;
    std::cout << "Timeouts: " << stats.timeouts << std::endl;
    std::cout << "===========================" << std::endl;
}

// Helper function to print discovered info hashes
void printDiscoveredInfoHashes(const std::shared_ptr<dht::Crawler>& crawler, size_t maxInfoHashes = 10) {
    auto infoHashes = crawler->getDiscoveredInfoHashes(maxInfoHashes);
    
    std::cout << "=== Discovered Info Hashes ===" << std::endl;
    for (const auto& infoHash : infoHashes) {
        std::string infoHashStr = dht::infoHashToString(infoHash);
        auto peers = crawler->getPeersForInfoHash(infoHash);
        
        std::cout << infoHashStr << " (" << peers.size() << " peers)" << std::endl;
    }
    std::cout << "=============================" << std::endl;
}

int main() {
    // Create a DHT node
    dht::DHTConfig config;
    config.setPort(6881);
    dht::DHTNode node(config);
    
    // Start the node
    if (!node.start()) {
        std::cerr << "Failed to start DHT node" << std::endl;
        return 1;
    }
    
    std::cout << "DHT node started" << std::endl;
    
    // Get the crawler
    auto crawler = node.getCrawler();
    if (!crawler) {
        std::cerr << "Failed to get crawler" << std::endl;
        return 1;
    }
    
    std::cout << "Crawler obtained" << std::endl;
    
    // Monitor some popular info hashes (examples)
    dht::InfoHash ubuntuInfoHash;
    dht::infoHashFromString("08ada5a7a6183aae1e09d831df6748d566095a10", ubuntuInfoHash);
    crawler->monitorInfoHash(ubuntuInfoHash);
    
    dht::InfoHash debianInfoHash;
    dht::infoHashFromString("a3c7be4d811f0b0348f5647f88aa1f475e869bcf", debianInfoHash);
    crawler->monitorInfoHash(debianInfoHash);
    
    std::cout << "Added info hashes to monitor" << std::endl;
    
    // Run for 5 minutes, printing statistics every 30 seconds
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        auto stats = crawler->getStatistics();
        printCrawlerStatistics(stats);
        
        if (i % 2 == 0) {
            printDiscoveredInfoHashes(crawler);
        }
    }
    
    // Stop the node
    node.stop();
    
    std::cout << "DHT node stopped" << std::endl;
    
    return 0;
}
