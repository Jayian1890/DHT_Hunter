#include "dht_hunter/network/nat/natpmp_service.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/utility/network/network_utils.hpp"
#include <sstream>
#include <algorithm>
#include <random>
#include <cstring>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#endif

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
    unified_event::logDebug("Network.NATPMP", "NAT-PMP service created");
}

NATPMPService::~NATPMPService() {
    shutdown();
}

bool NATPMPService::initialize() {
    if (m_initialized) {
        return true;
    }

    unified_event::logDebug("Network.NATPMP", "Initializing NAT-PMP service");

    // Log network interfaces for diagnostic purposes
    logNetworkInterfaces();

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
        unified_event::logWarning("Network.NATPMP", "Failed to set socket timeout: " + std::string(strerror(errno)) +
                                 ". This may cause requests to hang.");
        // Continue despite this error - we'll just have longer timeouts
    }

    // Discover the gateway
    if (!discoverGateway()) {
        unified_event::logWarning("Network.NATPMP", "Failed to discover gateway. NAT-PMP service will be limited.");
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Try to get external IP address
    m_externalIP = tryGetExternalIPAddress();
    if (m_externalIP.empty()) {
        unified_event::logWarning("Network.NATPMP", "Failed to get external IP address after multiple attempts. "
                                "NAT-PMP service will continue with limited functionality.");
        m_externalIPFailed = true;
        // Continue initialization despite this failure
    } else {
        unified_event::logDebug("Network.NATPMP", "External IP address: " + m_externalIP);
    }

    unified_event::logDebug("Network.NATPMP", "Gateway IP address: " + m_gatewayIP);

    // Start the refresh thread
    m_running = true;
    m_refreshThread = std::thread(&NATPMPService::refreshPortMappingsPeriodically, this);

    m_initialized = true;

    if (m_externalIPFailed) {
        unified_event::logInfo("Network.NATPMP", "NAT-PMP service initialized with limited functionality (no external IP)");
    } else {
        unified_event::logInfo("Network.NATPMP", "NAT-PMP service fully initialized");
    }

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

    // Extract the internal port (unused but kept for completeness)
    [[maybe_unused]] uint16_t mappedInternalPort = static_cast<uint16_t>(
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
        unified_event::logDebug("Network.NATPMP", "NAT-PMP mapped to a different external port: " + std::to_string(mappedExternalPort));
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

    // If we already have the external IP, return it
    if (!m_externalIP.empty()) {
        return m_externalIP;
    }

    // If we previously failed to get the external IP and we're initialized,
    // try again with the retry mechanism
    if (m_externalIPFailed && m_initialized) {
        unified_event::logDebug("Network.NATPMP", "Attempting to get external IP address again after previous failure");
        m_externalIP = tryGetExternalIPAddress();
        if (!m_externalIP.empty()) {
            m_externalIPFailed = false;
            unified_event::logDebug("Network.NATPMP", "Successfully retrieved external IP address after previous failure: " + m_externalIP);
        }
        return m_externalIP; // May still be empty if retry failed
    }

    // Otherwise, just try once
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
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP external IP response size: " + std::to_string(responseSize) + " bytes");
        return "";
    }

    // Check the version
    if (response[0] != NATPMP_VERSION) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP version in external IP response: " + std::to_string(response[0]));
        return "";
    }

    // Check the operation code
    if (response[1] != (NATPMP_OP_EXTERNAL_IP + 128)) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP operation code in external IP response: " + std::to_string(response[1]));
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

    unified_event::logDebug("Network.NATPMP", "Got external IP address: " + m_externalIP);
    return m_externalIP;
}

std::string NATPMPService::getStatus() const {
    if (!m_initialized) {
        return "Not initialized";
    }

    std::stringstream ss;
    ss << "Initialized, " << m_portMappings.size() << " port mappings";

    if (m_externalIPFailed) {
        ss << ", External IP: Unknown (retrieval failed)";
    } else if (m_externalIP.empty()) {
        ss << ", External IP: Not available";
    } else {
        ss << ", External IP: " << m_externalIP;
    }

    ss << ", Gateway IP: " << m_gatewayIP;
    return ss.str();
}

bool NATPMPService::discoverGateway() {
    unified_event::logDebug("Network.NATPMP", "Discovering gateway");

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
#elif defined(__APPLE__)
    // macOS implementation
    unified_event::logDebug("Network.NATPMP", "Using macOS-specific gateway discovery");

    // Store potential gateways for evaluation
    std::vector<std::string> potentialGateways;

    // First try using SystemConfiguration framework
    #ifdef __APPLE__
    unified_event::logDebug("Network.NATPMP", "Trying SystemConfiguration framework");

    SCDynamicStoreRef store = SCDynamicStoreCreate(nullptr, CFSTR("dht_hunter"), nullptr, nullptr);
    if (store) {
        // Get all network services
        CFArrayRef services = SCDynamicStoreCopyKeyList(store, CFSTR("State:/Network/Service/[^/]+/IPv4"));
        if (services) {
            CFIndex count = CFArrayGetCount(services);
            unified_event::logDebug("Network.NATPMP", "Found " + std::to_string(count) + " network services");

            for (CFIndex i = 0; i < count; i++) {
                CFStringRef serviceKey = static_cast<CFStringRef>(CFArrayGetValueAtIndex(services, i));
                CFDictionaryRef serviceDict = static_cast<CFDictionaryRef>(SCDynamicStoreCopyValue(store, serviceKey));

                if (serviceDict) {
                    // Get router address
                    CFStringRef routerAddress = static_cast<CFStringRef>(CFDictionaryGetValue(serviceDict, kSCPropNetIPv4Router));
                    if (routerAddress) {
                        char gatewayBuffer[16]; // IPv4 address max length
                        if (CFStringGetCString(routerAddress, gatewayBuffer, sizeof(gatewayBuffer), kCFStringEncodingUTF8)) {
                            std::string gateway = gatewayBuffer;
                            unified_event::logDebug("Network.NATPMP", "Found potential gateway IP: " + gateway);
                            potentialGateways.push_back(gateway);
                        }
                    }
                    CFRelease(serviceDict);
                }
            }
            CFRelease(services);
        }

        // Also try the global route
        CFStringRef routeKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(nullptr, kSCDynamicStoreDomainState, kSCEntNetIPv4);
        if (routeKey) {
            CFDictionaryRef routeDict = static_cast<CFDictionaryRef>(SCDynamicStoreCopyValue(store, routeKey));
            if (routeDict) {
                CFStringRef routerAddress = static_cast<CFStringRef>(CFDictionaryGetValue(routeDict, kSCPropNetIPv4Router));
                if (routerAddress) {
                    char gatewayBuffer[16]; // IPv4 address max length
                    if (CFStringGetCString(routerAddress, gatewayBuffer, sizeof(gatewayBuffer), kCFStringEncodingUTF8)) {
                        std::string gateway = gatewayBuffer;
                        unified_event::logDebug("Network.NATPMP", "Found global gateway IP: " + gateway);
                        potentialGateways.push_back(gateway);
                    }
                }
                CFRelease(routeDict);
            }
            CFRelease(routeKey);
        }
        CFRelease(store);
    }
    #endif

    // Try using command line tools to find more potential gateways
    unified_event::logDebug("Network.NATPMP", "Trying command line tools for gateway discovery");

    // Try using route command
    FILE* fp = popen("route -n get default | grep gateway | awk '{print $2}'", "r");
    if (fp != nullptr) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;
            std::string gateway = buffer;
            unified_event::logDebug("Network.NATPMP", "Found gateway IP using route command: " + gateway);
            potentialGateways.push_back(gateway);
        }
        pclose(fp);
    }

    // Try alternative method using netstat
    fp = popen("netstat -nr | grep default | awk '{print $2}' | head -n 1", "r");
    if (fp != nullptr) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;
            std::string gateway = buffer;
            unified_event::logDebug("Network.NATPMP", "Found gateway IP using netstat command: " + gateway);
            potentialGateways.push_back(gateway);
        }
        pclose(fp);
    }

    // Try to find all gateways using netstat
    fp = popen("netstat -nr | grep -v 'default' | grep -E '^[0-9]' | awk '{print $2}' | grep -v '0.0.0.0'", "r");
    if (fp != nullptr) {
        char buffer[16];
        while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;
            std::string gateway = buffer;
            if (!gateway.empty() && gateway != "0.0.0.0") {
                unified_event::logDebug("Network.NATPMP", "Found additional gateway IP: " + gateway);
                potentialGateways.push_back(gateway);
            }
        }
        pclose(fp);
    }

    // If we have potential gateways, prioritize them based on common home network patterns
    if (!potentialGateways.empty()) {
        // Remove duplicates
        std::sort(potentialGateways.begin(), potentialGateways.end());
        potentialGateways.erase(std::unique(potentialGateways.begin(), potentialGateways.end()), potentialGateways.end());

        unified_event::logDebug("Network.NATPMP", "Found " + std::to_string(potentialGateways.size()) + " unique potential gateways");

        // Helper function to check if an IP is in a specific range
        auto isInRange = [](const std::string& ip, const std::string& prefix) {
            return ip.substr(0, prefix.length()) == prefix;
        };

        // First priority: 192.168.x.x (most common home network)
        for (const auto& gateway : potentialGateways) {
            if (isInRange(gateway, "192.168.")) {
                m_gatewayIP = gateway;
                unified_event::logDebug("Network.NATPMP", "Selected 192.168.x.x gateway: " + m_gatewayIP);
                return true;
            }
        }

        // Second priority: 10.x.x.x (common for larger networks)
        for (const auto& gateway : potentialGateways) {
            if (isInRange(gateway, "10.")) {
                m_gatewayIP = gateway;
                unified_event::logDebug("Network.NATPMP", "Selected 10.x.x.x gateway: " + m_gatewayIP);
                return true;
            }
        }

        // Third priority: 172.16-31.x.x (less common private range)
        for (const auto& gateway : potentialGateways) {
            if (isInRange(gateway, "172.")) {
                // Check if it's in the private range (172.16.x.x to 172.31.x.x)
                int secondOctet = std::stoi(gateway.substr(4, gateway.find('.', 4) - 4));
                if (secondOctet >= 16 && secondOctet <= 31) {
                    m_gatewayIP = gateway;
                    unified_event::logDebug("Network.NATPMP", "Selected 172.16-31.x.x gateway: " + m_gatewayIP);
                    return true;
                }
            }
        }

        // Last resort: Use any gateway that's not a VPN (avoid 100.x.x.x Tailscale addresses)
        for (const auto& gateway : potentialGateways) {
            if (!isInRange(gateway, "100.")) {
                m_gatewayIP = gateway;
                unified_event::logDebug("Network.NATPMP", "Selected non-VPN gateway: " + m_gatewayIP);
                return true;
            }
        }

        // If we still don't have a gateway, use the first one as a last resort
        m_gatewayIP = potentialGateways[0];
        unified_event::logWarning("Network.NATPMP", "Using first available gateway as last resort: " + m_gatewayIP);
        return true;
    }

    // If we get here, we couldn't find any gateway
    unified_event::logError("Network.NATPMP", "Failed to find any gateway IP addresses");
    return false;
#else
    // Unix/Linux implementation
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
        unified_event::logDebug("Network.NATPMP", "Found gateway IP using ip route command: " + m_gatewayIP);
    } else {
        pclose(fp);
        unified_event::logError("Network.NATPMP", "Failed to get default gateway using ip route command");
        return false;
    }

    pclose(fp);
#endif

    if (m_gatewayIP.empty()) {
        unified_event::logError("Network.NATPMP", "Failed to discover gateway");
        return false;
    }

    unified_event::logDebug("Network.NATPMP", "Discovered gateway: " + m_gatewayIP);

    // Test if the gateway supports NAT-PMP by sending a request for external IP
    if (!testGateway()) {
        unified_event::logWarning("Network.NATPMP", "Gateway does not support NAT-PMP");
        return false;
    }

    return true;
}

bool NATPMPService::testGateway() {
    unified_event::logDebug("Network.NATPMP", "Testing gateway " + m_gatewayIP + " for NAT-PMP support");

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
            unified_event::logWarning("Network.NATPMP", "Failed to set socket timeout: " + std::string(strerror(errno)) +
                                     ". Testing will continue but may hang.");
            // Continue despite this error
        }
    }

    // Send a request for external IP address
    uint8_t request[2];
    request[0] = NATPMP_VERSION;
    request[1] = NATPMP_OP_EXTERNAL_IP;

    uint8_t response[16];
    size_t responseSize = sizeof(response);

    unified_event::logDebug("Network.NATPMP", "Sending NAT-PMP test request to gateway " + m_gatewayIP);
    if (!sendNATPMPRequest(request, sizeof(request), response, responseSize)) {
        unified_event::logWarning("Network.NATPMP", "Failed to send NAT-PMP test request to gateway " + m_gatewayIP);
        return false;
    }

    // Check if the response is valid
    if (responseSize < 12) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP response size: " + std::to_string(responseSize) + " bytes");
        return false;
    }

    // Check the version
    if (response[0] != NATPMP_VERSION) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP version in response: " + std::to_string(response[0]));
        return false;
    }

    // Check the operation code
    if (response[1] != (NATPMP_OP_EXTERNAL_IP + 128)) {
        unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP operation code in response: " + std::to_string(response[1]));
        return false;
    }

    // Check the result code
    uint16_t resultCode = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[2]) << 8) |
        static_cast<uint16_t>(response[3]));
    if (resultCode != NATPMP_RESULT_SUCCESS) {
        unified_event::logWarning("Network.NATPMP", "NAT-PMP test request failed with result code: " + std::to_string(resultCode));
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

    unified_event::logDebug("Network.NATPMP", "Gateway " + m_gatewayIP + " supports NAT-PMP, external IP: " + m_externalIP);
    return true;
}

bool NATPMPService::sendNATPMPRequest(const uint8_t* request, size_t requestSize, uint8_t* response, size_t& responseSize) {
    if (m_socket < 0) {
        unified_event::logWarning("Network.NATPMP", "Cannot send NAT-PMP request - socket not initialized");
        return false;
    }

    if (m_gatewayIP.empty()) {
        unified_event::logWarning("Network.NATPMP", "Cannot send NAT-PMP request - gateway not discovered");
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
    unified_event::logDebug("Network.NATPMP", "Starting port mapping refresh thread");

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
                [[maybe_unused]] uint16_t externalPort = static_cast<uint16_t>(std::stoi(key.substr(0, pos)));
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

    unified_event::logDebug("Network.NATPMP", "Port mapping refresh thread stopped");
}

std::string NATPMPService::tryGetExternalIPAddress() {
    unified_event::logDebug("Network.NATPMP", "Attempting to get external IP address with retry logic");

    // Reset retry count
    m_externalIPRetryCount = 0;

    // Try multiple times with increasing timeouts
    for (int attempt = 0; attempt < MAX_EXTERNAL_IP_RETRY_ATTEMPTS; attempt++) {
        m_externalIPRetryCount++;

        // If this isn't the first attempt, wait with exponential backoff
        if (attempt > 0) {
            int waitTime = 1000 * (1 << (attempt - 1)); // 1s, 2s, 4s...
            unified_event::logDebug("Network.NATPMP", "Waiting " + std::to_string(waitTime/1000) +
                                   " seconds before retry attempt " + std::to_string(attempt + 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
        }

        // Prepare the NAT-PMP request
        uint8_t request[2];
        request[0] = NATPMP_VERSION;
        request[1] = NATPMP_OP_EXTERNAL_IP;

        uint8_t response[16];
        size_t responseSize = sizeof(response);

        unified_event::logDebug("Network.NATPMP", "External IP retrieval attempt " + std::to_string(attempt + 1) +
                               " of " + std::to_string(MAX_EXTERNAL_IP_RETRY_ATTEMPTS));

        if (!sendNATPMPRequest(request, sizeof(request), response, responseSize)) {
            unified_event::logWarning("Network.NATPMP", "Failed to send NAT-PMP external IP request (attempt " +
                                    std::to_string(attempt + 1) + ")");
            continue; // Try again
        }

        // Check if the response is valid
        if (responseSize < 12) {
            unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP external IP response size: " +
                                    std::to_string(responseSize) + " bytes (attempt " + std::to_string(attempt + 1) + ")");
            continue; // Try again
        }

        // Check the version
        if (response[0] != NATPMP_VERSION) {
            unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP version in external IP response: " +
                                    std::to_string(response[0]) + " (attempt " + std::to_string(attempt + 1) + ")");
            continue; // Try again
        }

        // Check the operation code
        if (response[1] != (NATPMP_OP_EXTERNAL_IP + 128)) {
            unified_event::logWarning("Network.NATPMP", "Invalid NAT-PMP operation code in external IP response: " +
                                    std::to_string(response[1]) + " (attempt " + std::to_string(attempt + 1) + ")");
            continue; // Try again
        }

        // Check the result code
        uint16_t resultCode = static_cast<uint16_t>(
            (static_cast<uint16_t>(response[2]) << 8) |
            static_cast<uint16_t>(response[3]));
        if (resultCode != NATPMP_RESULT_SUCCESS) {
            unified_event::logWarning("Network.NATPMP", "NAT-PMP external IP request failed with result code: " +
                                    std::to_string(resultCode) + " (attempt " + std::to_string(attempt + 1) + ")");
            continue; // Try again
        }

        // Extract the epoch time
        m_epoch = static_cast<uint32_t>(
            (static_cast<uint32_t>(response[4]) << 24) |
            (static_cast<uint32_t>(response[5]) << 16) |
            (static_cast<uint32_t>(response[6]) << 8) |
            static_cast<uint32_t>(response[7]));

        // Extract the external IP address
        std::stringstream ss;
        ss << static_cast<int>(response[8]) << "." << static_cast<int>(response[9]) << "." <<
           static_cast<int>(response[10]) << "." << static_cast<int>(response[11]);
        std::string externalIP = ss.str();

        // Validate the IP address (basic check)
        if (externalIP == "0.0.0.0") {
            unified_event::logWarning("Network.NATPMP", "Received invalid external IP address: 0.0.0.0 (attempt " +
                                    std::to_string(attempt + 1) + ")");
            continue; // Try again
        }

        unified_event::logDebug("Network.NATPMP", "Successfully retrieved external IP address: " + externalIP +
                             " (attempt " + std::to_string(attempt + 1) + ")");
        return externalIP;
    }

    unified_event::logError("Network.NATPMP", "Failed to get external IP address after " +
                          std::to_string(MAX_EXTERNAL_IP_RETRY_ATTEMPTS) + " attempts");
    return "";
}

void NATPMPService::logNetworkInterfaces() {
    unified_event::logDebug("Network.NATPMP", "Logging network interface information for diagnostics");

#ifdef _WIN32
    // Windows implementation
    ULONG bufferSize = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    ULONG result = 0;

    do {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        if (pAddresses == nullptr) {
            unified_event::logError("Network.NATPMP", "Memory allocation failed for adapter addresses");
            return;
        }

        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);

        if (result == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = nullptr;
            bufferSize *= 2;
        }
    } while (result == ERROR_BUFFER_OVERFLOW);

    if (result == NO_ERROR) {
        int adapterCount = 0;
        for (PIP_ADAPTER_ADDRESSES pCurrent = pAddresses; pCurrent; pCurrent = pCurrent->Next) {
            adapterCount++;

            std::string adapterName = "<unknown>";
            if (pCurrent->FriendlyName) {
                char friendlyName[256];
                WideCharToMultiByte(CP_ACP, 0, pCurrent->FriendlyName, -1, friendlyName, sizeof(friendlyName), nullptr, nullptr);
                adapterName = friendlyName;
            }

            std::string adapterDesc = "<unknown>";
            if (pCurrent->Description) {
                char description[256];
                WideCharToMultiByte(CP_ACP, 0, pCurrent->Description, -1, description, sizeof(description), nullptr, nullptr);
                adapterDesc = description;
            }

            unified_event::logDebug("Network.NATPMP", "Adapter " + std::to_string(adapterCount) + ": " + adapterName +
                                   " (" + adapterDesc + ")");

            // Log IP addresses
            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrent->FirstUnicastAddress;
            int addressCount = 0;
            while (pUnicast) {
                addressCount++;
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* pSockAddr = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);
                    char ipAddress[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(pSockAddr->sin_addr), ipAddress, INET_ADDRSTRLEN);
                    unified_event::logDebug("Network.NATPMP", "  IP Address " + std::to_string(addressCount) + ": " + ipAddress);
                }
                pUnicast = pUnicast->Next;
            }

            // Log gateway addresses
            PIP_ADAPTER_GATEWAY_ADDRESS pGateway = pCurrent->FirstGatewayAddress;
            int gatewayCount = 0;
            while (pGateway) {
                gatewayCount++;
                if (pGateway->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* pSockAddr = reinterpret_cast<sockaddr_in*>(pGateway->Address.lpSockaddr);
                    char gatewayAddress[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(pSockAddr->sin_addr), gatewayAddress, INET_ADDRSTRLEN);
                    unified_event::logDebug("Network.NATPMP", "  Gateway " + std::to_string(gatewayCount) + ": " + gatewayAddress);
                }
                pGateway = pGateway->Next;
            }
        }
    } else {
        unified_event::logWarning("Network.NATPMP", "GetAdaptersAddresses failed with error code: " + std::to_string(result));
    }

    if (pAddresses) {
        free(pAddresses);
    }
#elif defined(__APPLE__) || defined(__linux__)
    // macOS and Linux implementation
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        unified_event::logError("Network.NATPMP", "getifaddrs failed: " + std::string(strerror(errno)));
        return;
    }

    int adapterCount = 0;
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        // Only handle IPv4 addresses
        if (ifa->ifa_addr->sa_family == AF_INET) {
            adapterCount++;

            char ipAddress[INET_ADDRSTRLEN];
            struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            inet_ntop(AF_INET, &(addr->sin_addr), ipAddress, INET_ADDRSTRLEN);

            unified_event::logDebug("Network.NATPMP", "Interface " + std::to_string(adapterCount) + ": " + ifa->ifa_name +
                                   " - IP: " + ipAddress);

            // Get netmask
            if (ifa->ifa_netmask != nullptr) {
                char netmask[INET_ADDRSTRLEN];
                struct sockaddr_in* mask = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_netmask);
                inet_ntop(AF_INET, &(mask->sin_addr), netmask, INET_ADDRSTRLEN);
                unified_event::logDebug("Network.NATPMP", "  Netmask: " + std::string(netmask));
            }

            // Get broadcast address if available
            if ((ifa->ifa_flags & IFF_BROADCAST) && ifa->ifa_broadaddr != nullptr) {
                char broadcast[INET_ADDRSTRLEN];
                struct sockaddr_in* bcast = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_broadaddr);
                inet_ntop(AF_INET, &(bcast->sin_addr), broadcast, INET_ADDRSTRLEN);
                unified_event::logDebug("Network.NATPMP", "  Broadcast: " + std::string(broadcast));
            }
        }
    }

    freeifaddrs(ifaddr);

    // Try to get gateway information using command line tools
    unified_event::logDebug("Network.NATPMP", "Attempting to get gateway information using command line tools");

#ifdef __APPLE__
    FILE* fp = popen("route -n get default | grep gateway | awk '{print $2}'", "r");
#else
    FILE* fp = popen("ip route | grep default | awk '{print $3}'", "r");
#endif

    if (fp != nullptr) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;
            unified_event::logDebug("Network.NATPMP", "Default gateway: " + std::string(buffer));
        } else {
            unified_event::logWarning("Network.NATPMP", "Failed to get default gateway from command line");
        }
        pclose(fp);
    } else {
        unified_event::logWarning("Network.NATPMP", "Failed to execute command to get default gateway");
    }
#endif

    unified_event::logDebug("Network.NATPMP", "Network interface logging complete");
}

} // namespace dht_hunter::network::nat
