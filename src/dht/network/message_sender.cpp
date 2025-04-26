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
    : m_config(config), m_socketManager(socketManager), m_running(false),
      m_logger(event::Logger::forComponent("DHT.MessageSender")) {
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
        m_logger.warning("Message sender already running");
        return true;
    }

    if (!m_socketManager || !m_socketManager->isRunning()) {
        m_logger.error("Socket manager not running");
        return false;
    }

    m_running = true;
    return true;
}

void MessageSender::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    m_running = false;
    m_logger.info("Message sender stopped");
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
        m_logger.error("Cannot send message: Message sender not running");
        return false;
    }

    if (!m_socketManager) {
        m_logger.error("Cannot send message: Socket manager not available");
        return false;
    }

    // Encode the message
    std::vector<uint8_t> data = message->encode();

    // Send the message
    ssize_t result = m_socketManager->sendTo(data.data(), data.size(), endpoint);

    if (result < 0) {
        m_logger.error("Failed to send message to {}", endpoint.toString());
        return false;
    }

    m_logger.debug("Sent message to {}", endpoint.toString());
    return true;
}

} // namespace dht_hunter::dht
