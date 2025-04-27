#include "dht_hunter/dht/services/statistics_service.hpp"
#include <sstream>
#include <iomanip>

namespace dht_hunter::dht::services {

// Initialize static members
std::shared_ptr<StatisticsService> StatisticsService::s_instance = nullptr;
std::mutex StatisticsService::s_instanceMutex;

std::shared_ptr<StatisticsService> StatisticsService::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<StatisticsService>(new StatisticsService());
    }

    return s_instance;
}

StatisticsService::StatisticsService()
    : m_eventBus(unified_event::EventBus::getInstance()),
      m_nodesDiscovered(0),
      m_nodesAdded(0),
      m_peersDiscovered(0),
      m_messagesReceived(0),
      m_messagesSent(0),
      m_errors(0),
      m_running(false),
      m_logger(event::Logger::forComponent("DHT.StatisticsService")) {
}

StatisticsService::~StatisticsService() {
    stop();

    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool StatisticsService::start() {
    if (m_running) {
        m_logger.warning("Statistics service already running");
        return true;
    }

    m_running = true;

    // Subscribe to events
    subscribeToEvents();

    m_logger.info("Statistics service started");
    return true;
}

void StatisticsService::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    // Unsubscribe from events
    for (int subscriptionId : m_eventSubscriptionIds) {
        m_eventBus->unsubscribe(subscriptionId);
    }
    m_eventSubscriptionIds.clear();

    m_logger.info("Statistics service stopped");
}

bool StatisticsService::isRunning() const {
    return m_running;
}

size_t StatisticsService::getNodesDiscovered() const {
    return m_nodesDiscovered;
}

size_t StatisticsService::getNodesAdded() const {
    return m_nodesAdded;
}

size_t StatisticsService::getPeersDiscovered() const {
    return m_peersDiscovered;
}

size_t StatisticsService::getMessagesReceived() const {
    return m_messagesReceived;
}

size_t StatisticsService::getMessagesSent() const {
    return m_messagesSent;
}

size_t StatisticsService::getErrors() const {
    return m_errors;
}

std::string StatisticsService::getStatisticsAsJson() const {
    std::stringstream ss;

    ss << "{\n";
    ss << "  \"nodesDiscovered\": " << m_nodesDiscovered << ",\n";
    ss << "  \"nodesAdded\": " << m_nodesAdded << ",\n";
    ss << "  \"peersDiscovered\": " << m_peersDiscovered << ",\n";
    ss << "  \"messagesReceived\": " << m_messagesReceived << ",\n";
    ss << "  \"messagesSent\": " << m_messagesSent << ",\n";
    ss << "  \"errors\": " << m_errors << "\n";
    ss << "}";

    return ss.str();
}

void StatisticsService::subscribeToEvents() {
    m_logger.debug("Subscribing to DHT events");

    // Subscribe to node discovered events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::NodeDiscovered,
            [this](std::shared_ptr<unified_event::Event> event) {
                handleNodeDiscoveredEvent(event);
            }
        )
    );

    // Subscribe to node added events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::NodeAdded,
            [this](std::shared_ptr<unified_event::Event> event) {
                handleNodeAddedEvent(event);
            }
        )
    );

    // Subscribe to peer discovered events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::PeerDiscovered,
            [this](std::shared_ptr<unified_event::Event> event) {
                handlePeerDiscoveredEvent(event);
            }
        )
    );

    // Subscribe to message received events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::MessageReceived,
            [this](std::shared_ptr<unified_event::Event> event) {
                handleMessageReceivedEvent(event);
            }
        )
    );

    // Subscribe to message sent events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::MessageSent,
            [this](std::shared_ptr<unified_event::Event> event) {
                handleMessageSentEvent(event);
            }
        )
    );

    // Subscribe to system error events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::SystemError,
            [this](std::shared_ptr<unified_event::Event> event) {
                handleSystemErrorEvent(event);
            }
        )
    );

    m_logger.debug("Subscribed to {} event types", m_eventSubscriptionIds.size());
}

void StatisticsService::handleNodeDiscoveredEvent(std::shared_ptr<unified_event::Event> /*event*/) {
    m_nodesDiscovered++;
    m_logger.debug("Nodes discovered: {}", m_nodesDiscovered.load());
}

void StatisticsService::handleNodeAddedEvent(std::shared_ptr<unified_event::Event> /*event*/) {
    m_nodesAdded++;
    m_logger.debug("Nodes added: {}", m_nodesAdded.load());
}

void StatisticsService::handlePeerDiscoveredEvent(std::shared_ptr<unified_event::Event> /*event*/) {
    m_peersDiscovered++;
    m_logger.debug("Peers discovered: {}", m_peersDiscovered.load());
}

void StatisticsService::handleMessageReceivedEvent(std::shared_ptr<unified_event::Event> /*event*/) {
    m_messagesReceived++;

    // Only log every 100 messages to avoid spamming the log
    if (m_messagesReceived % 100 == 0) {
        m_logger.debug("Messages received: {}", m_messagesReceived.load());
    }
}

void StatisticsService::handleMessageSentEvent(std::shared_ptr<unified_event::Event> /*event*/) {
    m_messagesSent++;

    // Only log every 100 messages to avoid spamming the log
    if (m_messagesSent % 100 == 0) {
        m_logger.debug("Messages sent: {}", m_messagesSent.load());
    }
}

void StatisticsService::handleSystemErrorEvent(std::shared_ptr<unified_event::Event> /*event*/) {
    m_errors++;
    m_logger.debug("Errors: {}", m_errors.load());
}

} // namespace dht_hunter::dht::services
