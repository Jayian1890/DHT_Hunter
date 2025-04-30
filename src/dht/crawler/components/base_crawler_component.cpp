#include "dht_hunter/dht/crawler/components/base_crawler_component.hpp"

namespace dht_hunter::dht::crawler {

BaseCrawlerComponent::BaseCrawlerComponent(const std::string& name,
                                         const DHTConfig& config,
                                         const NodeID& nodeID)
    : m_name(name),
      m_config(config),
      m_nodeID(nodeID),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_initialized(false),
      m_running(false) {
}

BaseCrawlerComponent::~BaseCrawlerComponent() {
    stop();
}

std::string BaseCrawlerComponent::getName() const {
    return m_name;
}

bool BaseCrawlerComponent::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    if (!onInitialize()) {
        unified_event::logError("DHT.Crawler." + m_name, "Failed to initialize");
        return false;
    }

    m_initialized = true;
    unified_event::logInfo("DHT.Crawler." + m_name, "Initialized");
    return true;
}

bool BaseCrawlerComponent::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        unified_event::logError("DHT.Crawler." + m_name, "Not initialized");
        return false;
    }

    if (m_running) {
        return true;
    }

    if (!onStart()) {
        unified_event::logError("DHT.Crawler." + m_name, "Failed to start");
        return false;
    }

    m_running = true;
    unified_event::logInfo("DHT.Crawler." + m_name, "Started");
    return true;
}

void BaseCrawlerComponent::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    onStop();
    m_running = false;
    unified_event::logInfo("DHT.Crawler." + m_name, "Stopped");
}

bool BaseCrawlerComponent::isRunning() const {
    return m_running;
}

void BaseCrawlerComponent::crawl() {
    if (!m_initialized) {
        unified_event::logError("DHT.Crawler." + m_name, "Not initialized");
        return;
    }

    if (!m_running) {
        unified_event::logError("DHT.Crawler." + m_name, "Not running");
        return;
    }

    onCrawl();
}

bool BaseCrawlerComponent::onInitialize() {
    // Default implementation does nothing
    return true;
}

bool BaseCrawlerComponent::onStart() {
    // Default implementation does nothing
    return true;
}

void BaseCrawlerComponent::onStop() {
    // Default implementation does nothing
}

} // namespace dht_hunter::dht::crawler
