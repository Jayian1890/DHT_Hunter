#include "dht_hunter/network/nat/natpmp_service.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include <sstream>
#include <algorithm>
#include <random>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#endif

namespace dht_hunter::network::nat {

// Initialize static members
std::shared_ptr<NATPMPService> NATPMPService::s_instance = nullptr;
std::mutex NATPMPService::s_instanceMutex;

std::shared_ptr<NATPMPService> NATPMPService::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<NATPMPService>(new NATPMPService());
    }
    return s_instance;
}

NATPMPService::NATPMPService() : m_initialized(false), m_running(false) {
    unified_event::logInfo("Network.NATPMP", "NAT-PMP service created");
}

NATPMPService::~NATPMPService() {
    shutdown();
}

bool NATPMPService::initialize() {
    if (m_initialized) {
        return true;
    }

    unified_event::logInfo("Network.NATPMP", "Initializing NAT-PMP service");

    // Create a socket for NAT-PMP communication
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        unified_event::logError("Network.NATPMP", "Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = NATPMP_TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, static_cast<void*>(&tv), sizeof(tv)) < 0) {
        unified_event::logWarning("Network.NATPMP", "Failed to set socket timeout: " + std::string(strerror(errno)));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Discover the gateway
    if (!discoverGateway()) {
        unified_event::logWarning("Network.NATPMP", "Failed to discover gateway");
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Get external IP address
    m_externalIP = getExternalIPAddress();
    if (m_externalIP.empty()) {
        unified_event::logWarning("Network.NATPMP", "Failed to get external IP address");
        return false;
    }

    unified_event::logInfo("Network.NATPMP", "External IP address: " + m_externalIP);
    unified_event::logInfo("Network.NATPMP", "Gateway IP address: " + m_gatewayIP);

    // Start the refresh thread
    m_running = true;
    m_refreshThread = std::thread(&NATPMPService::refreshPortMappingsPeriodically, this);

    m_initialized = true;
    unified_event::logInfo("Network.NATPMP", "NAT-PMP service initialized");
    return true;
}

void NATPMPService::shutdown() {
    if (!m_initialized) {
        return;
    }

    unified_event::logInfo("Network.NATPMP", "Shutting down NAT-PMP service");

    // Stop the refresh thread
    m_running = false;
    m_refreshCondition.notify_all();
    if (m_refreshThread.joinable()) {
        m_refreshThread.join();
    }

    // Remove all port mappings
    std::vector<std::string> mappingsToRemove;
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        for (const auto& [key, mapping] : m_portMappings) {
            mappingsToRemove.push_back(key);
        }
    }

    for (const auto& key : mappingsToRemove) {
        size_t pos = key.find(':');
        if (pos != std::string::npos) {
            uint16_t externalPort = static_cast<uint16_t>(std::stoi(key.substr(0, pos)));
            std::string protocol = key.substr(pos + 1);
            removePortMapping(externalPort, protocol);
        }
    }

    // Close the socket
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }

    m_initialized = false;
    unified_event::logInfo("Network.NATPMP", "NAT-PMP service shut down");
}

bool NATPMPService::isInitialized() const {
    return m_initialized;
}

uint16_t NATPMPService::addPortMapping(uint16_t internalPort, uint16_t externalPort, const std::string& protocol, const std::string& description) {
    if (!m_initialized) {
        unified_event::logWarning("Network.NATPMP", "Cannot add port mapping - NAT-PMP service not initialized");
        return 0;
    }

    // If external port is 0, use the same as internal port
    if (externalPort == 0) {
        externalPort = internalPort;
    }

    // Check if we already have this mapping
    std::string key = std::to_string(externalPort) + ":" + protocol;
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        if (m_portMappings.find(key) != m_portMappings.end()) {
            return externalPort;
        }
    }

    // Prepare the NAT-PMP request
    uint8_t request[12];
    memset(request, 0, sizeof(request));
    request[0] = NATPMP_VERSION;
    request[1] = protocol == "UDP" ? NATPMP_OP_MAP_UDP : NATPMP_OP_MAP_TCP;
    // Bytes 2-3 are reserved and must be 0

    // Internal port (bytes 4-5)
    request[4] = (internalPort >> 8) & 0xFF;
    request[5] = internalPort & 0xFF;

    // External port (bytes 6-7)
    request[6] = (externalPort >> 8) & 0xFF;
    request[7] = externalPort & 0xFF;

    // Lifetime (bytes 8-11)
    uint32_t lifetime = DEFAULT_PORT_MAPPING_LIFETIME_SECONDS;
    request[8] = (lifetime >> 24) & 0xFF;
    request[9] = (lifetime >> 16) & 0xFF;
    request[10] = (lifetime >> 8) & 0xFF;
    request[11] = lifetime & 0xFF;

    // Send the request
    uint8_t response[16];
    size_t responseSize = sizeof(response);

    if (!sendNATPMPRequest(request, sizeof(request), response, responseSize)) {
        unified_event::logWarning("Network.NATPMP", "Failed to send NAT-PMP port mapping request");
        return 0;
    }

    // Check if the response is valid
    if (responseSize < 16) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP port mapping response size");
        return 0;
    }

    // Check the version
    if (response[0] != NATPMP_VERSION) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP version in port mapping response");
        return 0;
    }

    // Check the operation code
    uint8_t expectedOp = (protocol == "UDP" ? NATPMP_OP_MAP_UDP : NATPMP_OP_MAP_TCP) + 128;
    if (response[1] != expectedOp) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP operation code in port mapping response");
        return 0;
    }

    // Check the result code
    uint16_t resultCode = static_cast<uint16_t>((static_cast<uint16_t>(response[2]) << 8) | static_cast<uint16_t>(response[3]));
    if (resultCode != NATPMP_RESULT_SUCCESS) {
        unified_event::logWarning("Network.NATPMP", "NAT-PMP port mapping request failed with result code: " + std::to_string(resultCode));
        return 0;
    }

    // Extract the epoch time
    m_epoch = static_cast<uint32_t>(
        (static_cast<uint32_t>(response[4]) << 24) |
        (static_cast<uint32_t>(response[5]) << 16) |
        (static_cast<uint32_t>(response[6]) << 8) |
        static_cast<uint32_t>(response[7]));

    // Extract the internal port
    uint16_t mappedInternalPort = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[8]) << 8) |
        static_cast<uint16_t>(response[9]));

    // Extract the external port
    uint16_t mappedExternalPort = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[10]) << 8) |
        static_cast<uint16_t>(response[11]));

    // Extract the lifetime
    uint32_t mappedLifetime = static_cast<uint32_t>(
        (static_cast<uint32_t>(response[12]) << 24) |
        (static_cast<uint32_t>(response[13]) << 16) |
        (static_cast<uint32_t>(response[14]) << 8) |
        static_cast<uint32_t>(response[15]));

    // Check if the mapping was successful
    if (mappedExternalPort != externalPort) {
        unified_event::logInfo("Network.NATPMP", "NAT-PMP mapped to a different external port: " + std::to_string(mappedExternalPort));
        externalPort = mappedExternalPort;
        key = std::to_string(externalPort) + ":" + protocol;
    }

    // Add the mapping to our list
    PortMapping mapping;
    mapping.internalPort = internalPort;
    mapping.externalPort = externalPort;
    mapping.protocol = protocol;
    mapping.description = description;
    mapping.lastRefresh = std::chrono::steady_clock::now();
    mapping.lifetime = mappedLifetime;

    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        m_portMappings[key] = mapping;
    }

    unified_event::logInfo("Network.NATPMP", "Added port mapping: " + std::to_string(externalPort) + " (" + protocol + ") -> " + std::to_string(internalPort) + " (" + description + ")");
    return externalPort;
}

bool NATPMPService::removePortMapping(uint16_t externalPort, const std::string& protocol) {
    if (!m_initialized) {
        unified_event::logWarning("Network.NATPMP", "Cannot remove port mapping - NAT-PMP service not initialized");
        return false;
    }

    // Check if we have this mapping
    std::string key = std::to_string(externalPort) + ":" + protocol;
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        if (m_portMappings.find(key) == m_portMappings.end()) {
            return false;
        }
    }

    // Get the internal port from the mapping
    uint16_t internalPort = 0;
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        auto it = m_portMappings.find(key);
        if (it != m_portMappings.end()) {
            internalPort = it->second.internalPort;
        }
    }

    if (internalPort == 0) {
        unified_event::logWarning("Network.NATPMP", "Cannot remove port mapping - mapping not found");
        return false;
    }

    // Prepare the NAT-PMP request
    uint8_t request[12];
    memset(request, 0, sizeof(request));
    request[0] = NATPMP_VERSION;
    request[1] = protocol == "UDP" ? NATPMP_OP_MAP_UDP : NATPMP_OP_MAP_TCP;
    // Bytes 2-3 are reserved and must be 0

    // Internal port (bytes 4-5)
    request[4] = (internalPort >> 8) & 0xFF;
    request[5] = internalPort & 0xFF;

    // External port (bytes 6-7)
    request[6] = (externalPort >> 8) & 0xFF;
    request[7] = externalPort & 0xFF;

    // Lifetime (bytes 8-11) - 0 to remove the mapping
    request[8] = 0;
    request[9] = 0;
    request[10] = 0;
    request[11] = 0;

    // Send the request
    uint8_t response[16];
    size_t responseSize = sizeof(response);

    if (!sendNATPMPRequest(request, sizeof(request), response, responseSize)) {
        unified_event::logWarning("Network.NATPMP", "Failed to send NAT-PMP port mapping removal request");
        return false;
    }

    // Check if the response is valid
    if (responseSize < 16) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP port mapping removal response size");
        return false;
    }

    // Check the version
    if (response[0] != NATPMP_VERSION) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP version in port mapping removal response");
        return false;
    }

    // Check the operation code
    uint8_t expectedOp = (protocol == "UDP" ? NATPMP_OP_MAP_UDP : NATPMP_OP_MAP_TCP) + 128;
    if (response[1] != expectedOp) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP operation code in port mapping removal response");
        return false;
    }

    // Check the result code
    uint16_t resultCode = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[2]) << 8) |
        static_cast<uint16_t>(response[3]));
    if (resultCode != NATPMP_RESULT_SUCCESS) {
        unified_event::logWarning("Network.NATPMP", "NAT-PMP port mapping removal request failed with result code: " + std::to_string(resultCode));
        return false;
    }

    // Remove the mapping from our list
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        m_portMappings.erase(key);
    }

    unified_event::logInfo("Network.NATPMP", "Removed port mapping: " + std::to_string(externalPort) + " (" + protocol + ")");
    return true;
}

std::string NATPMPService::getExternalIPAddress() {
    if (!m_initialized && m_externalIP.empty()) {
        unified_event::logWarning("Network.NATPMP", "Cannot get external IP address - NAT-PMP service not initialized");
        return "";
    }

    if (!m_externalIP.empty()) {
        return m_externalIP;
    }

    // Prepare the NAT-PMP request
    uint8_t request[2];
    request[0] = NATPMP_VERSION;
    request[1] = NATPMP_OP_EXTERNAL_IP;

    uint8_t response[16];
    size_t responseSize = sizeof(response);

    if (!sendNATPMPRequest(request, sizeof(request), response, responseSize)) {
        unified_event::logWarning("Network.NATPMP", "Failed to send NAT-PMP external IP request");
        return "";
    }

    // Check if the response is valid
    if (responseSize < 12) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP external IP response size");
        return "";
    }

    // Check the version
    if (response[0] != NATPMP_VERSION) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP version in external IP response");
        return "";
    }

    // Check the operation code
    if (response[1] != (NATPMP_OP_EXTERNAL_IP + 128)) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP operation code in external IP response");
        return "";
    }

    // Check the result code
    uint16_t resultCode = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[2]) << 8) |
        static_cast<uint16_t>(response[3]));
    if (resultCode != NATPMP_RESULT_SUCCESS) {
        unified_event::logWarning("Network.NATPMP", "NAT-PMP external IP request failed with result code: " + std::to_string(resultCode));
        return "";
    }

    // Extract the epoch time
    m_epoch = static_cast<uint32_t>(
        (static_cast<uint32_t>(response[4]) << 24) |
        (static_cast<uint32_t>(response[5]) << 16) |
        (static_cast<uint32_t>(response[6]) << 8) |
        static_cast<uint32_t>(response[7]));

    // Extract the external IP address
    std::stringstream ss;
    ss << static_cast<int>(response[8]) << "." << static_cast<int>(response[9]) << "." << static_cast<int>(response[10]) << "." << static_cast<int>(response[11]);
    m_externalIP = ss.str();

    unified_event::logInfo("Network.NATPMP", "Got external IP address: " + m_externalIP);
    return m_externalIP;
}

std::string NATPMPService::getStatus() const {
    if (!m_initialized) {
        return "Not initialized";
    }

    std::stringstream ss;
    ss << "Initialized, " << m_portMappings.size() << " port mappings";
    ss << ", External IP: " << m_externalIP;
    ss << ", Gateway IP: " << m_gatewayIP;
    return ss.str();
}

bool NATPMPService::discoverGateway() {
    unified_event::logInfo("Network.NATPMP", "Discovering gateway");

#ifdef _WIN32
    // Windows implementation
    PMIB_IPFORWARDTABLE pIpForwardTable = nullptr;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // Get the size needed for the table
    dwRetVal = GetIpForwardTable(nullptr, &dwSize, FALSE);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pIpForwardTable = (PMIB_IPFORWARDTABLE)malloc(dwSize);
        if (pIpForwardTable == nullptr) {
            unified_event::logError("Network.NATPMP", "Memory allocation failed for IP forward table");
            return false;
        }

        // Get the actual table
        dwRetVal = GetIpForwardTable(pIpForwardTable, &dwSize, FALSE);
        if (dwRetVal == NO_ERROR) {
            // Find the default route (0.0.0.0)
            for (DWORD i = 0; i < pIpForwardTable->dwNumEntries; i++) {
                if (pIpForwardTable->table[i].dwForwardDest == 0) {
                    // Convert the next hop address to a string
                    IN_ADDR nextHopAddr;
                    nextHopAddr.S_un.S_addr = pIpForwardTable->table[i].dwForwardNextHop;
                    char szNextHop[16];
                    strcpy_s(szNextHop, 16, inet_ntoa(nextHopAddr));
                    m_gatewayIP = szNextHop;
                    break;
                }
            }
        } else {
            unified_event::logError("Network.NATPMP", "GetIpForwardTable failed: " + std::to_string(dwRetVal));
            free(pIpForwardTable);
            return false;
        }

        free(pIpForwardTable);
    } else {
        unified_event::logError("Network.NATPMP", "GetIpForwardTable failed: " + std::to_string(dwRetVal));
        return false;
    }
#else
    // Unix/Linux/macOS implementation
    FILE* fp = popen("ip route | grep default | awk '{print $3}'", "r");
    if (fp == nullptr) {
        unified_event::logError("Network.NATPMP", "Failed to execute command to get default gateway");
        return false;
    }

    char buffer[16];
    if (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;
        m_gatewayIP = buffer;
    } else {
        // Try alternative command for macOS
        pclose(fp);
        fp = popen("route -n get default | grep gateway | awk '{print $2}'", "r");
        if (fp == nullptr) {
            unified_event::logError("Network.NATPMP", "Failed to execute command to get default gateway");
            return false;
        }

        if (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;
            m_gatewayIP = buffer;
        } else {
            pclose(fp);
            unified_event::logError("Network.NATPMP", "Failed to get default gateway");
            return false;
        }
    }

    pclose(fp);
#endif

    if (m_gatewayIP.empty()) {
        unified_event::logError("Network.NATPMP", "Failed to discover gateway");
        return false;
    }

    unified_event::logInfo("Network.NATPMP", "Discovered gateway: " + m_gatewayIP);

    // Test if the gateway supports NAT-PMP by sending a request for external IP
    if (!testGateway()) {
        unified_event::logWarning("Network.NATPMP", "Gateway does not support NAT-PMP");
        return false;
    }

    return true;
}

bool NATPMPService::testGateway() {
    unified_event::logInfo("Network.NATPMP", "Testing gateway for NAT-PMP support");

    // Create a socket for NAT-PMP communication if it doesn't exist
    if (m_socket < 0) {
        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0) {
            unified_event::logError("Network.NATPMP", "Failed to create socket: " + std::string(strerror(errno)));
            return false;
        }

        // Set socket timeout
        struct timeval tv;
        tv.tv_sec = NATPMP_TIMEOUT_SECONDS;
        tv.tv_usec = 0;
        if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, static_cast<void*>(&tv), sizeof(tv)) < 0) {
            unified_event::logWarning("Network.NATPMP", "Failed to set socket timeout: " + std::string(strerror(errno)));
            close(m_socket);
            m_socket = -1;
            return false;
        }
    }

    // Send a request for external IP address
    uint8_t request[2];
    request[0] = NATPMP_VERSION;
    request[1] = NATPMP_OP_EXTERNAL_IP;

    uint8_t response[16];
    size_t responseSize = sizeof(response);

    if (!sendNATPMPRequest(request, sizeof(request), response, responseSize)) {
        unified_event::logWarning("Network.NATPMP", "Failed to send NAT-PMP request");
        return false;
    }

    // Check if the response is valid
    if (responseSize < 12) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP response size");
        return false;
    }

    // Check the version
    if (response[0] != NATPMP_VERSION) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP version in response");
        return false;
    }

    // Check the operation code
    if (response[1] != (NATPMP_OP_EXTERNAL_IP + 128)) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP operation code in response");
        return false;
    }

    // Check the result code
    uint16_t resultCode = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[2]) << 8) |
        static_cast<uint16_t>(response[3]));
    if (resultCode != NATPMP_RESULT_SUCCESS) {
        unified_event::logWarning("Network.NATPMP", "NAT-PMP request failed with result code: " + std::to_string(resultCode));
        return false;
    }

    // Extract the epoch time
    m_epoch = static_cast<uint32_t>(
        (static_cast<uint32_t>(response[4]) << 24) |
        (static_cast<uint32_t>(response[5]) << 16) |
        (static_cast<uint32_t>(response[6]) << 8) |
        static_cast<uint32_t>(response[7]));

    // Extract the external IP address
    std::stringstream ss;
    ss << static_cast<int>(response[8]) << "." << static_cast<int>(response[9]) << "." << static_cast<int>(response[10]) << "." << static_cast<int>(response[11]);
    m_externalIP = ss.str();

    unified_event::logInfo("Network.NATPMP", "Gateway supports NAT-PMP, external IP: " + m_externalIP);
    return true;
}

bool NATPMPService::sendNATPMPRequest(const uint8_t* request, size_t requestSize, uint8_t* response, size_t& responseSize) {
    if (m_socket < 0 || m_gatewayIP.empty()) {
        unified_event::logWarning("Network.NATPMP", "Cannot send NAT-PMP request - socket not initialized or gateway not discovered");
        return false;
    }

    // Set up the gateway address
    struct sockaddr_in gatewayAddr;
    memset(&gatewayAddr, 0, sizeof(gatewayAddr));
    gatewayAddr.sin_family = AF_INET;
    gatewayAddr.sin_port = htons(NATPMP_PORT);
    gatewayAddr.sin_addr.s_addr = inet_addr(m_gatewayIP.c_str());

    // Send the request
    if (sendto(m_socket, reinterpret_cast<const char*>(request), requestSize, 0, reinterpret_cast<struct sockaddr*>(&gatewayAddr), sizeof(gatewayAddr)) < 0) {
        unified_event::logError("Network.NATPMP", "Failed to send NAT-PMP request: " + std::string(strerror(errno)));
        return false;
    }

    // Receive the response
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    ssize_t bytesReceived = recvfrom(m_socket, reinterpret_cast<char*>(response), responseSize, 0, reinterpret_cast<struct sockaddr*>(&senderAddr), &senderAddrLen);

    if (bytesReceived < 0) {
        // Check if it's a timeout
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            unified_event::logWarning("Network.NATPMP", "NAT-PMP request timed out");
        } else {
            unified_event::logError("Network.NATPMP", "Failed to receive NAT-PMP response: " + std::string(strerror(errno)));
        }
        return false;
    }

    // Check if the response is from the gateway
    if (senderAddr.sin_addr.s_addr != gatewayAddr.sin_addr.s_addr) {
        unified_event::logWarning("Network.NATPMP", "Received NAT-PMP response from unknown source");
        return false;
    }

    // Update the response size
    responseSize = static_cast<size_t>(bytesReceived);
    return true;
}

void NATPMPService::refreshPortMappingsPeriodically() {
    unified_event::logInfo("Network.NATPMP", "Starting port mapping refresh thread");

    while (m_running) {
        // Wait for the refresh interval or until shutdown
        {
            std::unique_lock<std::mutex> lock(m_refreshMutex);
            m_refreshCondition.wait_for(lock, std::chrono::seconds(REFRESH_INTERVAL_SECONDS), [this]() { return !m_running; });
        }

        if (!m_running) {
            break;
        }

        // Refresh port mappings
        std::vector<std::string> mappingsToRefresh;
        {
            std::lock_guard<std::mutex> lock(m_portMappingsMutex);
            for (const auto& [key, mapping] : m_portMappings) {
                mappingsToRefresh.push_back(key);
            }
        }

        for (const auto& key : mappingsToRefresh) {
            size_t pos = key.find(':');
            if (pos != std::string::npos) {
                uint16_t externalPort = static_cast<uint16_t>(std::stoi(key.substr(0, pos)));
                std::string protocol = key.substr(pos + 1);

                // Get the mapping
                PortMapping mapping;
                {
                    std::lock_guard<std::mutex> lock(m_portMappingsMutex);
                    auto it = m_portMappings.find(key);
                    if (it != m_portMappings.end()) {
                        mapping = it->second;
                    } else {
                        continue;
                    }
                }

                // Refresh the mapping
                addPortMapping(mapping.internalPort, mapping.externalPort, mapping.protocol, mapping.description);
            }
        }
    }

    unified_event::logInfo("Network.NATPMP", "Port mapping refresh thread stopped");
}

} // namespace dht_hunter::network::nat
