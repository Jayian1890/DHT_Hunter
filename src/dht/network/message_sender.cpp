#include "dht_hunter/dht/network/message_sender.hpp"

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<MessageSender> MessageSender::s_instance = nullptr;
std::mutex MessageSender::s_instanceMutex;

std::shared_ptr<MessageSender> MessageSender::getInstance(
    const DHTConfig& config,
    std::shared_ptr<SocketManager> socketManager) {

    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<MessageSender>(new MessageSender(config, socketManager));
    }

    return s_instance;
}

MessageSender::MessageSender(const DHTConfig& config, std::shared_ptr<SocketManager> socketManager)
    : m_config(config),
      m_socketManager(socketManager),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_running(false) {
}

MessageSender::~MessageSender() {
    stop();

    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool MessageSender::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        return true;
    }

    if (!m_socketManager || !m_socketManager->isRunning()) {
        return false;
    }

    m_running = true;

    // Publish a system started event
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.MessageSender");
    m_eventBus->publish(startedEvent);
    return true;
}

void MessageSender::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    m_running = false;

    // Publish a system stopped event
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("DHT.MessageSender");
    m_eventBus->publish(stoppedEvent);
}

bool MessageSender::isRunning() const {
    return m_running;
}

bool MessageSender::sendQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& endpoint) {
    return sendMessage(query, endpoint);
}

bool MessageSender::sendResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& endpoint) {
    return sendMessage(response, endpoint);
}

bool MessageSender::sendError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& endpoint) {
    return sendMessage(error, endpoint);
}

bool MessageSender::sendMessage(std::shared_ptr<Message> message, const network::EndPoint& endpoint) {
    if (!m_running) {
        return false;
    }

    if (!m_socketManager) {
        return false;
    }

    // Encode the message
    std::vector<uint8_t> data = message->encode();

    // Send the message
    ssize_t result = m_socketManager->sendTo(data.data(), data.size(), endpoint);

    if (result < 0) {
        return false;
    }

    // Publish a message sent event
    // Convert EndPoint to NetworkAddress
    network::NetworkAddress networkAddress = endpoint.getAddress();
    auto event = std::make_shared<unified_event::MessageSentEvent>("DHT.MessageSender", message, networkAddress);
    m_eventBus->publish(event);

    // Message sent events are logged through the event system

    return true;
}

} // namespace dht_hunter::dht
