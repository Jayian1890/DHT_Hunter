#include "dht_hunter/network/nat/upnp_service.hpp"
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
#include <netdb.h>
#include <unistd.h>
#endif

namespace dht_hunter::network::nat {

// Initialize static members
std::shared_ptr<UPnPService> UPnPService::s_instance = nullptr;
std::mutex UPnPService::s_instanceMutex;

std::shared_ptr<UPnPService> UPnPService::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<UPnPService>(new UPnPService());
    }
    return s_instance;
}

UPnPService::UPnPService() : m_initialized(false), m_running(false) {
    unified_event::logInfo("Network.UPnP", "UPnP service created");
}

UPnPService::~UPnPService() {
    shutdown();
}

bool UPnPService::initialize() {
    if (m_initialized) {
        return true;
    }

    unified_event::logInfo("Network.UPnP", "Initializing UPnP service");

    // Discover UPnP devices
    if (!discoverDevices()) {
        unified_event::logWarning("Network.UPnP", "Failed to discover UPnP devices");
        return false;
    }

    // Select a device to use
    if (!selectDevice()) {
        unified_event::logWarning("Network.UPnP", "Failed to select UPnP device");
        return false;
    }

    // Get external IP address
    m_externalIP = getExternalIPAddress();
    if (m_externalIP.empty()) {
        unified_event::logWarning("Network.UPnP", "Failed to get external IP address");
        return false;
    }

    unified_event::logInfo("Network.UPnP", "External IP address: " + m_externalIP);
    unified_event::logInfo("Network.UPnP", "Local IP address: " + m_localIP);
    unified_event::logInfo("Network.UPnP", "Device name: " + m_deviceName);
    unified_event::logInfo("Network.UPnP", "Service type: " + m_serviceType);
    unified_event::logInfo("Network.UPnP", "Control URL: " + m_controlURL);

    // Start the refresh thread
    m_running = true;
    m_refreshThread = std::thread(&UPnPService::refreshPortMappingsPeriodically, this);

    m_initialized = true;
    unified_event::logInfo("Network.UPnP", "UPnP service initialized");
    return true;
}

void UPnPService::shutdown() {
    if (!m_initialized) {
        return;
    }

    unified_event::logInfo("Network.UPnP", "Shutting down UPnP service");

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

    m_initialized = false;
    unified_event::logInfo("Network.UPnP", "UPnP service shut down");
}

bool UPnPService::isInitialized() const {
    return m_initialized;
}

uint16_t UPnPService::addPortMapping(uint16_t internalPort, uint16_t externalPort, const std::string& protocol, const std::string& description) {
    if (!m_initialized) {
        unified_event::logWarning("Network.UPnP", "Cannot add port mapping - UPnP service not initialized");
        return 0;
    }

    // If external port is 0, choose a random port between 49152 and 65535
    if (externalPort == 0) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint16_t> dis(49152, 65535);
        externalPort = dis(gen);
    }

    // Check if we already have this mapping
    std::string key = std::to_string(externalPort) + ":" + protocol;
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        if (m_portMappings.find(key) != m_portMappings.end()) {
            return externalPort;
        }
    }

    // Prepare the SOAP request
    std::string soapAction = m_serviceType + "#AddPortMapping";
    std::string soapBody =
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:AddPortMapping xmlns:u=\"" + m_serviceType + "\">"
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>" + std::to_string(externalPort) + "</NewExternalPort>"
        "<NewProtocol>" + protocol + "</NewProtocol>"
        "<NewInternalPort>" + std::to_string(internalPort) + "</NewInternalPort>"
        "<NewInternalClient>" + m_localIP + "</NewInternalClient>"
        "<NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>" + description + "</NewPortMappingDescription>"
        "<NewLeaseDuration>" + std::to_string(MAX_PORT_MAPPING_LIFETIME_SECONDS) + "</NewLeaseDuration>"
        "</u:AddPortMapping>"
        "</s:Body>"
        "</s:Envelope>";

    // Send the SOAP request
    std::string response;
    bool success = sendSOAPRequest(soapAction, soapBody, response);

    if (success) {
        // Check for errors in the response
        if (response.find("<errorCode>") != std::string::npos) {
            std::string errorCode = parseSOAPResponse(response, "errorCode");
            std::string errorDescription = parseSOAPResponse(response, "errorDescription");
            unified_event::logWarning("Network.UPnP", "Failed to add port mapping: " + errorCode + " - " + errorDescription);
            return 0;
        }

        // Add the mapping to our list
        PortMapping mapping;
        mapping.internalPort = internalPort;
        mapping.externalPort = externalPort;
        mapping.protocol = protocol;
        mapping.description = description;
        mapping.lastRefresh = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(m_portMappingsMutex);
            m_portMappings[key] = mapping;
        }

        unified_event::logInfo("Network.UPnP", "Added port mapping: " + std::to_string(externalPort) + " (" + protocol + ") -> " + m_localIP + ":" + std::to_string(internalPort) + " (" + description + ")");
        return externalPort;
    } else {
        unified_event::logWarning("Network.UPnP", "Failed to add port mapping: " + std::to_string(externalPort) + " (" + protocol + ") -> " + m_localIP + ":" + std::to_string(internalPort));
        return 0;
    }
}

bool UPnPService::removePortMapping(uint16_t externalPort, const std::string& protocol) {
    if (!m_initialized) {
        unified_event::logWarning("Network.UPnP", "Cannot remove port mapping - UPnP service not initialized");
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

    // Prepare the SOAP request
    std::string soapAction = m_serviceType + "#DeletePortMapping";
    std::string soapBody =
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:DeletePortMapping xmlns:u=\"" + m_serviceType + "\">"
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>" + std::to_string(externalPort) + "</NewExternalPort>"
        "<NewProtocol>" + protocol + "</NewProtocol>"
        "</u:DeletePortMapping>"
        "</s:Body>"
        "</s:Envelope>";

    // Send the SOAP request
    std::string response;
    bool success = sendSOAPRequest(soapAction, soapBody, response);

    if (success) {
        // Check for errors in the response
        if (response.find("<errorCode>") != std::string::npos) {
            std::string errorCode = parseSOAPResponse(response, "errorCode");
            std::string errorDescription = parseSOAPResponse(response, "errorDescription");
            unified_event::logWarning("Network.UPnP", "Failed to remove port mapping: " + errorCode + " - " + errorDescription);
            return false;
        }

        // Remove the mapping from our list
        {
            std::lock_guard<std::mutex> lock(m_portMappingsMutex);
            m_portMappings.erase(key);
        }

        unified_event::logInfo("Network.UPnP", "Removed port mapping: " + std::to_string(externalPort) + " (" + protocol + ")");
        return true;
    } else {
        unified_event::logWarning("Network.UPnP", "Failed to remove port mapping: " + std::to_string(externalPort) + " (" + protocol + ")");
        return false;
    }
}

std::string UPnPService::getExternalIPAddress() {
    if (!m_initialized && m_externalIP.empty()) {
        unified_event::logWarning("Network.UPnP", "Cannot get external IP address - UPnP service not initialized");
        return "";
    }

    if (!m_externalIP.empty()) {
        return m_externalIP;
    }

    // Prepare the SOAP request
    std::string soapAction = m_serviceType + "#GetExternalIPAddress";
    std::string soapBody =
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:GetExternalIPAddress xmlns:u=\"" + m_serviceType + "\">"
        "</u:GetExternalIPAddress>"
        "</s:Body>"
        "</s:Envelope>";

    // Send the SOAP request
    std::string response;
    bool success = sendSOAPRequest(soapAction, soapBody, response);

    if (success) {
        // Check for errors in the response
        if (response.find("<errorCode>") != std::string::npos) {
            std::string errorCode = parseSOAPResponse(response, "errorCode");
            std::string errorDescription = parseSOAPResponse(response, "errorDescription");
            unified_event::logWarning("Network.UPnP", "Failed to get external IP address: " + errorCode + " - " + errorDescription);
            return "";
        }

        // Extract the external IP address from the response
        m_externalIP = parseSOAPResponse(response, "NewExternalIPAddress");
        if (m_externalIP.empty()) {
            unified_event::logWarning("Network.UPnP", "Failed to parse external IP address from response");
            return "";
        }

        unified_event::logInfo("Network.UPnP", "Got external IP address: " + m_externalIP);
        return m_externalIP;
    } else {
        unified_event::logWarning("Network.UPnP", "Failed to get external IP address");
        return "";
    }
}

std::string UPnPService::getStatus() const {
    if (!m_initialized) {
        return "Not initialized";
    }

    std::stringstream ss;
    ss << "Initialized, " << m_portMappings.size() << " port mappings";
    ss << ", External IP: " << m_externalIP;
    ss << ", Local IP: " << m_localIP;
    ss << ", Device: " << m_deviceName;
    return ss.str();
}

bool UPnPService::discoverDevices() {
    unified_event::logInfo("Network.UPnP", "Discovering UPnP devices");

    // Get the local IP address
    if (!getLocalIPAddress()) {
        unified_event::logWarning("Network.UPnP", "Failed to get local IP address");
        return false;
    }

    // Create a UDP socket for SSDP discovery
    int ssdpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ssdpSocket < 0) {
        unified_event::logError("Network.UPnP", "Failed to create SSDP socket: " + std::string(strerror(errno)));
        return false;
    }

    // Set socket options
    int optval = 1;
    if (setsockopt(ssdpSocket, SOL_SOCKET, SO_REUSEADDR, static_cast<void*>(&optval), sizeof(optval)) < 0) {
        unified_event::logWarning("Network.UPnP", "Failed to set socket options: " + std::string(strerror(errno)));
        close(ssdpSocket);
        return false;
    }

    // Set timeout for receiving responses
    struct timeval tv;
    tv.tv_sec = DISCOVERY_TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    if (setsockopt(ssdpSocket, SOL_SOCKET, SO_RCVTIMEO, static_cast<void*>(&tv), sizeof(tv)) < 0) {
        unified_event::logWarning("Network.UPnP", "Failed to set socket timeout: " + std::string(strerror(errno)));
        close(ssdpSocket);
        return false;
    }

    // Prepare the multicast address
    struct sockaddr_in multicastAddr;
    memset(&multicastAddr, 0, sizeof(multicastAddr));
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(1900); // SSDP port
    multicastAddr.sin_addr.s_addr = inet_addr("239.255.255.250"); // SSDP multicast address

    // Prepare the SSDP discovery message
    std::string ssdpRequest =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: " + std::to_string(DISCOVERY_TIMEOUT_SECONDS) + "\r\n"
        "ST: urn:schemas-upnp-org:service:WANIPConnection:1\r\n"
        "\r\n";

    // Send the discovery message
    if (sendto(ssdpSocket, ssdpRequest.c_str(), ssdpRequest.length(), 0, reinterpret_cast<struct sockaddr*>(&multicastAddr), sizeof(multicastAddr)) < 0) {
        unified_event::logError("Network.UPnP", "Failed to send SSDP discovery message: " + std::string(strerror(errno)));
        close(ssdpSocket);
        return false;
    }

    unified_event::logInfo("Network.UPnP", "Sent SSDP discovery message, waiting for responses...");

    // Receive responses
    char buffer[4096];
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    std::vector<std::string> deviceLocations;

    // Keep receiving responses until timeout
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recvfrom(ssdpSocket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<struct sockaddr*>(&senderAddr), &senderAddrLen);

        if (bytesReceived < 0) {
            // Check if it's a timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // Timeout, no more responses
            }

            unified_event::logWarning("Network.UPnP", "Error receiving SSDP response: " + std::string(strerror(errno)));
            continue;
        }

        buffer[bytesReceived] = '\0';
        std::string response(buffer);

        // Parse the response to get the location of the device description
        if (response.find("HTTP/1.1 200 OK") != std::string::npos) {
            size_t locPos = response.find("LOCATION:");
            if (locPos != std::string::npos) {
                size_t locEndPos = response.find("\r\n", locPos);
                if (locEndPos != std::string::npos) {
                    std::string location = response.substr(locPos + 9, locEndPos - locPos - 9);
                    location.erase(0, location.find_first_not_of(" \t")); // Trim leading whitespace
                    deviceLocations.push_back(location);
                    unified_event::logInfo("Network.UPnP", "Found UPnP device at: " + location);
                }
            }
        }
    }

    close(ssdpSocket);

    if (deviceLocations.empty()) {
        unified_event::logWarning("Network.UPnP", "No UPnP devices found");
        return false;
    }

    // Store the device locations for later use in selectDevice()
    m_deviceLocations = deviceLocations;
    unified_event::logInfo("Network.UPnP", "Discovered " + std::to_string(deviceLocations.size()) + " UPnP devices");
    return true;
}

bool UPnPService::selectDevice() {
    unified_event::logInfo("Network.UPnP", "Selecting UPnP device");

    if (m_deviceLocations.empty()) {
        unified_event::logWarning("Network.UPnP", "No device locations available");
        return false;
    }

    // Try each device location until we find a suitable one
    for (const auto& location : m_deviceLocations) {
        if (parseDeviceDescription(location)) {
            unified_event::logInfo("Network.UPnP", "Selected UPnP device: " + m_deviceName);
            return true;
        }
    }

    unified_event::logWarning("Network.UPnP", "Failed to select a suitable UPnP device");
    return false;
}

bool UPnPService::getLocalIPAddress() {
    unified_event::logInfo("Network.UPnP", "Getting local IP address");

#ifdef _WIN32
    // Windows implementation
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        unified_event::logError("Network.UPnP", "WSAStartup failed");
        return false;
    }

    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) != 0) {
        unified_event::logError("Network.UPnP", "Failed to get hostname");
        WSACleanup();
        return false;
    }

    struct addrinfo hints, *result = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 only
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostName, nullptr, &hints, &result) != 0) {
        unified_event::logError("Network.UPnP", "Failed to get address info");
        WSACleanup();
        return false;
    }

    char ipBuffer[INET_ADDRSTRLEN];
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        struct sockaddr_in* addr = (struct sockaddr_in*)ptr->ai_addr;
        inet_ntop(AF_INET, &(addr->sin_addr), ipBuffer, INET_ADDRSTRLEN);
        std::string ip(ipBuffer);

        // Skip loopback addresses
        if (ip != "127.0.0.1") {
            m_localIP = ip;
            break;
        }
    }

    freeaddrinfo(result);
    WSACleanup();
#else
    // Unix/Linux/macOS implementation
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;

    if (getifaddrs(&ifAddrStruct) == -1) {
        unified_event::logError("Network.UPnP", "Failed to get network interfaces");
        return false;
    }

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

        // Check if it's an IPv4 address
        if (ifa->ifa_addr->sa_family == AF_INET) {
            // Skip loopback interfaces
            if (!(ifa->ifa_flags & IFF_LOOPBACK)) {
                void* tmpAddrPtr = &(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr))->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                m_localIP = addressBuffer;
                break;
            }
        }
    }

    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }
#endif

    if (m_localIP.empty()) {
        unified_event::logWarning("Network.UPnP", "Failed to get local IP address");
        return false;
    }

    unified_event::logInfo("Network.UPnP", "Local IP address: " + m_localIP);
    return true;
}

bool UPnPService::parseDeviceDescription(const std::string& url) {
    unified_event::logInfo("Network.UPnP", "Parsing device description: " + url);

    // Download the device description
    std::string response;
    if (!httpGet(url, response)) {
        unified_event::logWarning("Network.UPnP", "Failed to download device description");
        return false;
    }

    // Parse the device description to find the control URL and service type
    // This is a simple parser that looks for specific XML tags
    // In a real implementation, you would use a proper XML parser

    // Find the device name
    size_t friendlyNameStart = response.find("<friendlyName>");
    size_t friendlyNameEnd = response.find("</friendlyName>");
    if (friendlyNameStart != std::string::npos && friendlyNameEnd != std::string::npos) {
        m_deviceName = response.substr(friendlyNameStart + 14, friendlyNameEnd - friendlyNameStart - 14);
    } else {
        m_deviceName = "Unknown Device";
    }

    // Find the WANIPConnection service
    size_t serviceTypeStart = response.find("<serviceType>urn:schemas-upnp-org:service:WANIPConnection:");
    if (serviceTypeStart == std::string::npos) {
        // Try WANPPPConnection as an alternative
        serviceTypeStart = response.find("<serviceType>urn:schemas-upnp-org:service:WANPPPConnection:");
        if (serviceTypeStart == std::string::npos) {
            unified_event::logWarning("Network.UPnP", "No WANIPConnection or WANPPPConnection service found");
            return false;
        }
    }

    // Extract the service type
    size_t serviceTypeEnd = response.find("</serviceType>", serviceTypeStart);
    if (serviceTypeEnd == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "Failed to find service type end tag");
        return false;
    }
    m_serviceType = response.substr(serviceTypeStart + 13, serviceTypeEnd - serviceTypeStart - 13);

    // Find the control URL for this service
    // We need to find the <controlURL> that belongs to the service we found
    size_t serviceEnd = response.find("</service>", serviceTypeStart);
    if (serviceEnd == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "Failed to find service end tag");
        return false;
    }

    size_t controlURLStart = response.find("<controlURL>", serviceTypeStart);
    if (controlURLStart == std::string::npos || controlURLStart > serviceEnd) {
        unified_event::logWarning("Network.UPnP", "Failed to find control URL");
        return false;
    }

    size_t controlURLEnd = response.find("</controlURL>", controlURLStart);
    if (controlURLEnd == std::string::npos || controlURLEnd > serviceEnd) {
        unified_event::logWarning("Network.UPnP", "Failed to find control URL end tag");
        return false;
    }

    std::string controlURL = response.substr(controlURLStart + 12, controlURLEnd - controlURLStart - 12);

    // Construct the full control URL
    // If the control URL is absolute, use it as is
    // If it's relative, combine it with the base URL
    if (controlURL.find("http://") == 0 || controlURL.find("https://") == 0) {
        m_controlURL = controlURL;
    } else {
        // Extract the base URL from the device description URL
        size_t urlEnd = url.find_last_of("/");
        if (urlEnd == std::string::npos) {
            unified_event::logWarning("Network.UPnP", "Failed to extract base URL");
            return false;
        }

        // If the control URL starts with a slash, use the host part of the URL
        if (controlURL[0] == '/') {
            size_t hostStart = url.find("://");
            if (hostStart == std::string::npos) {
                unified_event::logWarning("Network.UPnP", "Failed to extract host from URL");
                return false;
            }
            hostStart += 3;

            size_t hostEnd = url.find("/", hostStart);
            if (hostEnd == std::string::npos) {
                unified_event::logWarning("Network.UPnP", "Failed to extract host from URL");
                return false;
            }

            std::string host = url.substr(0, hostEnd);
            m_controlURL = host + controlURL;
        } else {
            // Otherwise, use the directory part of the URL
            m_controlURL = url.substr(0, urlEnd + 1) + controlURL;
        }
    }

    unified_event::logInfo("Network.UPnP", "Found service: " + m_serviceType);
    unified_event::logInfo("Network.UPnP", "Control URL: " + m_controlURL);
    return true;
}

bool UPnPService::httpGet(const std::string& url, std::string& response) {
    unified_event::logInfo("Network.UPnP", "HTTP GET: " + url);

    // Parse the URL to get the host, port, and path
    size_t hostStart = url.find("://");
    if (hostStart == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "Invalid URL: " + url);
        return false;
    }
    hostStart += 3;

    size_t hostEnd = url.find("/", hostStart);
    if (hostEnd == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "Invalid URL: " + url);
        return false;
    }

    std::string host = url.substr(hostStart, hostEnd - hostStart);
    std::string path = url.substr(hostEnd);

    // Check if the host includes a port
    size_t portPos = host.find(":");
    int port = 80; // Default HTTP port
    if (portPos != std::string::npos) {
        port = std::stoi(host.substr(portPos + 1));
        host = host.substr(0, portPos);
    }

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        unified_event::logError("Network.UPnP", "Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }

    // Set a timeout for the socket
    struct timeval tv;
    tv.tv_sec = 5; // 5 seconds timeout
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, static_cast<void*>(&tv), sizeof(tv)) < 0) {
        unified_event::logWarning("Network.UPnP", "Failed to set socket timeout: " + std::string(strerror(errno)));
        close(sockfd);
        return false;
    }

    // Resolve the host
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        unified_event::logError("Network.UPnP", "Failed to resolve host: " + host);
        close(sockfd);
        return false;
    }

    // Set up the server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, static_cast<size_t>(server->h_length));
    serverAddr.sin_port = htons(port);

    // Connect to the server
    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        unified_event::logError("Network.UPnP", "Failed to connect to server: " + std::string(strerror(errno)));
        close(sockfd);
        return false;
    }

    // Prepare the HTTP request
    std::string request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + ":" + std::to_string(port) + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";

    // Send the request
    if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
        unified_event::logError("Network.UPnP", "Failed to send HTTP request: " + std::string(strerror(errno)));
        close(sockfd);
        return false;
    }

    // Receive the response
    char buffer[4096];
    response.clear();
    int bytesRead;

    while ((bytesRead = static_cast<int>(recv(sockfd, buffer, sizeof(buffer) - 1, 0))) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    close(sockfd);

    if (response.empty()) {
        unified_event::logWarning("Network.UPnP", "Empty HTTP response");
        return false;
    }

    // Check if the response is valid
    if (response.find("HTTP/1.1 200") == std::string::npos && response.find("HTTP/1.0 200") == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "HTTP request failed: " + response.substr(0, response.find("\r\n")));
        return false;
    }

    // Extract the body from the response
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        response = response.substr(bodyStart + 4);
    }

    return true;
}

bool UPnPService::httpPost(const std::string& url, const std::string& contentType, const std::string& data, std::string& response) {
    unified_event::logInfo("Network.UPnP", "HTTP POST: " + url);

    // Parse the URL to get the host, port, and path
    size_t hostStart = url.find("://");
    if (hostStart == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "Invalid URL: " + url);
        return false;
    }
    hostStart += 3;

    size_t hostEnd = url.find("/", hostStart);
    if (hostEnd == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "Invalid URL: " + url);
        return false;
    }

    std::string host = url.substr(hostStart, hostEnd - hostStart);
    std::string path = url.substr(hostEnd);

    // Check if the host includes a port
    size_t portPos = host.find(":");
    int port = 80; // Default HTTP port
    if (portPos != std::string::npos) {
        port = std::stoi(host.substr(portPos + 1));
        host = host.substr(0, portPos);
    }

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        unified_event::logError("Network.UPnP", "Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }

    // Set a timeout for the socket
    struct timeval tv;
    tv.tv_sec = 5; // 5 seconds timeout
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, static_cast<void*>(&tv), sizeof(tv)) < 0) {
        unified_event::logWarning("Network.UPnP", "Failed to set socket timeout: " + std::string(strerror(errno)));
        close(sockfd);
        return false;
    }

    // Resolve the host
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        unified_event::logError("Network.UPnP", "Failed to resolve host: " + host);
        close(sockfd);
        return false;
    }

    // Set up the server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, static_cast<size_t>(server->h_length));
    serverAddr.sin_port = htons(port);

    // Connect to the server
    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        unified_event::logError("Network.UPnP", "Failed to connect to server: " + std::string(strerror(errno)));
        close(sockfd);
        return false;
    }

    // Prepare the HTTP request
    std::string request = "POST " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + ":" + std::to_string(port) + "\r\n";
    request += "Content-Type: " + contentType + "\r\n";
    request += "Content-Length: " + std::to_string(data.length()) + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    request += data;

    // Send the request
    if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
        unified_event::logError("Network.UPnP", "Failed to send HTTP request: " + std::string(strerror(errno)));
        close(sockfd);
        return false;
    }

    // Receive the response
    char buffer[4096];
    response.clear();
    int bytesRead;

    while ((bytesRead = static_cast<int>(recv(sockfd, buffer, sizeof(buffer) - 1, 0))) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    close(sockfd);

    if (response.empty()) {
        unified_event::logWarning("Network.UPnP", "Empty HTTP response");
        return false;
    }

    // Check if the response is valid
    if (response.find("HTTP/1.1 200") == std::string::npos && response.find("HTTP/1.0 200") == std::string::npos) {
        unified_event::logWarning("Network.UPnP", "HTTP request failed: " + response.substr(0, response.find("\r\n")));
        return false;
    }

    // Extract the body from the response
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        response = response.substr(bodyStart + 4);
    }

    return true;
}

bool UPnPService::sendSOAPRequest(const std::string& soapAction, const std::string& soapBody, std::string& response) {
    unified_event::logInfo("Network.UPnP", "Sending SOAP request: " + soapAction);

    // Prepare the HTTP headers
    std::string contentType = "text/xml; charset=\"utf-8\"";
    std::string headers = "SOAPAction: \"" + soapAction + "\"\r\n";

    // Send the HTTP POST request
    return httpPost(m_controlURL, contentType, soapBody, response);
}

std::string UPnPService::parseSOAPResponse(const std::string& response, const std::string& elementName) {
    // Find the element in the response
    std::string startTag = "<" + elementName + ">";
    std::string endTag = "</" + elementName + ">";

    size_t startPos = response.find(startTag);
    if (startPos == std::string::npos) {
        return "";
    }

    size_t endPos = response.find(endTag, startPos);
    if (endPos == std::string::npos) {
        return "";
    }

    return response.substr(startPos + startTag.length(), endPos - startPos - startTag.length());
}

void UPnPService::refreshPortMappingsPeriodically() {
    unified_event::logInfo("Network.UPnP", "Starting port mapping refresh thread");

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

    unified_event::logInfo("Network.UPnP", "Port mapping refresh thread stopped");
}

} // namespace dht_hunter::network::nat
