#include "network/udp_socket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <iostream>

namespace network {

UDPSocket::UDPSocket(const UDPSocketConfig& config)
    : m_config(config),
      m_socket(-1),
      m_running(false),
      m_localAddress("0.0.0.0", config.port) {
}

UDPSocket::~UDPSocket() {
    close().wait();
}

std::future<bool> UDPSocket::open() {
    return utils::executeAsync([this]() {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        
        if (m_socket >= 0) {
            // Socket is already open
            return true;
        }
        
        // Create socket
        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Set socket options
        int optval = 1;
        if (m_config.reuseAddress) {
            if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
                std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
                close();
                return false;
            }
        }
        
        if (m_config.broadcast) {
            if (setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
                std::cerr << "Failed to set SO_BROADCAST: " << strerror(errno) << std::endl;
                close();
                return false;
            }
        }
        
        // Bind socket
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(m_config.port);
        
        if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
            close();
            return false;
        }
        
        // Get the actual port (in case we used port 0)
        socklen_t addrLen = sizeof(addr);
        if (getsockname(m_socket, (struct sockaddr*)&addr, &addrLen) < 0) {
            std::cerr << "Failed to get socket name: " << strerror(errno) << std::endl;
            close();
            return false;
        }
        
        // Update local address
        m_localAddress = NetworkAddress("0.0.0.0", ntohs(addr.sin_port));
        
        // Start receive thread
        m_running = true;
        m_receiveThread = utils::executeAsync(&UDPSocket::receiveLoop, this);
        
        // Publish event
        auto eventBus = event_system::getEventBus();
        auto event = std::make_shared<event_system::NetworkEvent>(
            event_system::NetworkEvent::NetworkEventType::CONNECTED,
            "UDPSocket",
            event_system::EventSeverity::INFO
        );
        event->setProperty("address", m_localAddress.toString());
        eventBus->publish(event);
        
        return true;
    });
}

std::future<void> UDPSocket::close() {
    return utils::executeAsync([this]() {
        // Stop receive thread
        m_running = false;
        
        if (m_receiveThread.valid()) {
            m_receiveThread.wait();
        }
        
        // Close socket
        std::lock_guard<std::mutex> lock(m_socketMutex);
        if (m_socket >= 0) {
            ::close(m_socket);
            m_socket = -1;
            
            // Publish event
            auto eventBus = event_system::getEventBus();
            auto event = std::make_shared<event_system::NetworkEvent>(
                event_system::NetworkEvent::NetworkEventType::DISCONNECTED,
                "UDPSocket",
                event_system::EventSeverity::INFO
            );
            event->setProperty("address", m_localAddress.toString());
            eventBus->publish(event);
        }
    });
}

std::future<bool> UDPSocket::send(const NetworkAddress& address, const NetworkMessage& message) {
    return utils::executeAsync([this, address, message]() {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        
        if (m_socket < 0) {
            std::cerr << "Socket is not open" << std::endl;
            return false;
        }
        
        // Set up destination address
        struct sockaddr_in destAddr;
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(address.getPort());
        
        if (inet_pton(AF_INET, address.getHost().c_str(), &destAddr.sin_addr) <= 0) {
            std::cerr << "Invalid address: " << address.getHost() << std::endl;
            return false;
        }
        
        // Send data
        ssize_t bytesSent = sendto(
            m_socket,
            message.getData().data(),
            message.getSize(),
            0,
            (struct sockaddr*)&destAddr,
            sizeof(destAddr)
        );
        
        if (bytesSent < 0) {
            std::cerr << "Failed to send data: " << strerror(errno) << std::endl;
            return false;
        }
        
        if (static_cast<size_t>(bytesSent) != message.getSize()) {
            std::cerr << "Incomplete send: " << bytesSent << " of " << message.getSize() << " bytes" << std::endl;
            return false;
        }
        
        // Publish event
        auto eventBus = event_system::getEventBus();
        auto event = std::make_shared<event_system::NetworkEvent>(
            event_system::NetworkEvent::NetworkEventType::DATA_SENT,
            "UDPSocket",
            event_system::EventSeverity::DEBUG
        );
        event->setProperty("address", address.toString());
        event->setProperty("size", message.getSize());
        eventBus->publish(event);
        
        return true;
    });
}

bool UDPSocket::isOpen() const {
    std::lock_guard<std::mutex> lock(m_socketMutex);
    return m_socket >= 0;
}

NetworkAddress UDPSocket::getLocalAddress() const {
    return m_localAddress;
}

void UDPSocket::receiveLoop() {
    std::vector<uint8_t> buffer(m_config.receiveBufferSize);
    
    while (m_running) {
        struct sockaddr_in srcAddr;
        socklen_t srcAddrLen = sizeof(srcAddr);
        
        // Receive data
        ssize_t bytesReceived;
        {
            std::lock_guard<std::mutex> lock(m_socketMutex);
            if (m_socket < 0) {
                break;
            }
            
            bytesReceived = recvfrom(
                m_socket,
                buffer.data(),
                buffer.size(),
                0,
                (struct sockaddr*)&srcAddr,
                &srcAddrLen
            );
        }
        
        if (bytesReceived < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, try again
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
            break;
        }
        
        if (bytesReceived == 0) {
            // Connection closed
            break;
        }
        
        // Create message from received data
        std::vector<uint8_t> messageData(buffer.begin(), buffer.begin() + bytesReceived);
        NetworkMessage message(std::move(messageData));
        
        // Get source address
        char srcHost[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &srcAddr.sin_addr, srcHost, INET_ADDRSTRLEN);
        NetworkAddress srcAddress(srcHost, ntohs(srcAddr.sin_port));
        
        // Publish event
        auto eventBus = event_system::getEventBus();
        auto event = std::make_shared<event_system::NetworkEvent>(
            event_system::NetworkEvent::NetworkEventType::DATA_RECEIVED,
            "UDPSocket",
            event_system::EventSeverity::DEBUG
        );
        event->setProperty("address", srcAddress.toString());
        event->setProperty("size", message.getSize());
        event->setProperty("message", message);
        eventBus->publish(event);
    }
}

} // namespace network
