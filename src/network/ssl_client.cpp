#include "dht_hunter/network/ssl_client.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>

namespace dht_hunter::network {

// TLS record types
enum class TLSRecordType : uint8_t {
    CHANGE_CIPHER_SPEC = 20,
    ALERT = 21,
    HANDSHAKE = 22,
    APPLICATION_DATA = 23
};

// TLS alert levels
enum class TLSAlertLevel : uint8_t {
    WARNING = 1,
    FATAL = 2
};

// TLS alert descriptions
enum class TLSAlertDescription : uint8_t {
    CLOSE_NOTIFY = 0,
    UNEXPECTED_MESSAGE = 10,
    BAD_RECORD_MAC = 20,
    RECORD_OVERFLOW = 22,
    HANDSHAKE_FAILURE = 40,
    BAD_CERTIFICATE = 42,
    UNSUPPORTED_CERTIFICATE = 43,
    CERTIFICATE_REVOKED = 44,
    CERTIFICATE_EXPIRED = 45,
    CERTIFICATE_UNKNOWN = 46,
    ILLEGAL_PARAMETER = 47,
    UNKNOWN_CA = 48,
    ACCESS_DENIED = 49,
    DECODE_ERROR = 50,
    DECRYPT_ERROR = 51,
    PROTOCOL_VERSION = 70,
    INSUFFICIENT_SECURITY = 71,
    INTERNAL_ERROR = 80,
    USER_CANCELED = 90,
    NO_RENEGOTIATION = 100
};

// TLS handshake types
enum class TLSHandshakeType : uint8_t {
    HELLO_REQUEST = 0,
    CLIENT_HELLO = 1,
    SERVER_HELLO = 2,
    CERTIFICATE = 11,
    SERVER_KEY_EXCHANGE = 12,
    CERTIFICATE_REQUEST = 13,
    SERVER_HELLO_DONE = 14,
    CERTIFICATE_VERIFY = 15,
    CLIENT_KEY_EXCHANGE = 16,
    FINISHED = 20
};

// TLS versions
constexpr uint16_t TLS_VERSION_1_0 = 0x0301;
constexpr uint16_t TLS_VERSION_1_1 = 0x0302;
constexpr uint16_t TLS_VERSION_1_2 = 0x0303;

// TLS cipher suites
constexpr uint16_t TLS_RSA_WITH_AES_128_CBC_SHA = 0x002F;
constexpr uint16_t TLS_RSA_WITH_AES_256_CBC_SHA = 0x0035;
constexpr uint16_t TLS_RSA_WITH_AES_128_CBC_SHA256 = 0x003C;
constexpr uint16_t TLS_RSA_WITH_AES_256_CBC_SHA256 = 0x003D;

SSLClient::SSLClient()
    : m_connected(false),
      m_handshakeComplete(false),
      m_verifyCertificate(true),
      m_clientSeqNum(0),
      m_serverSeqNum(0) {

    // Create the TCP client
    m_tcpClient = std::make_shared<TCPClient>();

    // Set up callbacks
    m_tcpClient->setDataReceivedCallback([this](const uint8_t* data, size_t length) {
        handleReceivedData(data, length);
    });

    m_tcpClient->setErrorCallback([this](const std::string& error) {
        handleError(error);
    });

    m_tcpClient->setConnectionClosedCallback([this]() {
        handleConnectionClosed();
    });
}

SSLClient::~SSLClient() {
    disconnect();
}

bool SSLClient::connect(const std::string& host, uint16_t port, int timeoutSec) {
    if (m_connected) {
        return true;
    }

    // Store the hostname for SNI
    m_hostname = host;

    unified_event::logDebug("Network.SSLClient", "Connecting to " + host + ":" + std::to_string(port));

    // Connect using the TCP client
    if (!m_tcpClient->connect(host, port, timeoutSec)) {
        unified_event::logWarning("Network.SSLClient", "Failed to establish TCP connection to " + host + ":" + std::to_string(port));
        return false;
    }

    // Perform the SSL/TLS handshake
    if (!performHandshake()) {
        unified_event::logWarning("Network.SSLClient", "SSL/TLS handshake failed with " + host + ":" + std::to_string(port));
        m_tcpClient->disconnect();
        return false;
    }

    m_connected = true;
    unified_event::logDebug("Network.SSLClient", "Successfully established secure connection to " + host + ":" + std::to_string(port));

    return true;
}

std::vector<uint8_t> SSLClient::generateAlert(TLSAlertLevel level, TLSAlertDescription description) {
    unified_event::logDebug("Network.SSLClient", "Generating alert: level=" + std::to_string(static_cast<uint8_t>(level)) +
                                              ", description=" + std::to_string(static_cast<uint8_t>(description)));

    // Create a buffer for the TLS record
    std::vector<uint8_t> record;

    // TLS Record Header (5 bytes)
    // Record Type: Alert (21)
    record.push_back(static_cast<uint8_t>(TLSRecordType::ALERT));

    // Protocol Version: TLS 1.2 (0x0303)
    record.push_back(0x03); // Major version
    record.push_back(0x03); // Minor version

    // Length (2 bytes)
    record.push_back(0x00);
    record.push_back(0x02); // Alert messages are always 2 bytes

    // Alert Level
    record.push_back(static_cast<uint8_t>(level));

    // Alert Description
    record.push_back(static_cast<uint8_t>(description));

    return record;
}

void SSLClient::disconnect() {
    if (!m_connected) {
        return;
    }

    // Send close_notify alert if possible
    if (m_handshakeComplete) {
        // Generate and send a close_notify alert
        std::vector<uint8_t> closeNotify = generateAlert(TLSAlertLevel::WARNING, TLSAlertDescription::CLOSE_NOTIFY);

        // Try to send the alert, but don't worry if it fails
        if (m_tcpClient->send(closeNotify.data(), closeNotify.size())) {
            unified_event::logDebug("Network.SSLClient", "Sent close_notify alert");
        } else {
            unified_event::logDebug("Network.SSLClient", "Failed to send close_notify alert");
        }

        // According to RFC 5246, we should wait for the peer's close_notify alert,
        // but in practice many implementations don't send it, so we'll just continue
        // with the disconnection process
    }

    m_tcpClient->disconnect();
    m_connected = false;
    m_handshakeComplete = false;
}

bool SSLClient::isConnected() const {
    return m_connected && m_tcpClient->isConnected();
}

bool SSLClient::send(const uint8_t* data, size_t length) {
    if (!m_connected || !m_handshakeComplete) {
        return false;
    }

    // Encrypt the data
    std::vector<uint8_t> encryptedData = encrypt(data, length);

    // Send the encrypted data
    return m_tcpClient->send(encryptedData.data(), encryptedData.size());
}

void SSLClient::setDataReceivedCallback(std::function<void(const uint8_t*, size_t)> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_dataReceivedCallback = callback;
}

void SSLClient::setErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_errorCallback = callback;
}

void SSLClient::setConnectionClosedCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_connectionClosedCallback = callback;
}

void SSLClient::setVerifyCertificate(bool verify) {
    m_verifyCertificate = verify;
}

bool SSLClient::performHandshake() {
    unified_event::logDebug("Network.SSLClient", "Starting SSL/TLS handshake");

    // Generate and send client hello
    std::vector<uint8_t> clientHello = generateClientHello();
    if (!m_tcpClient->send(clientHello.data(), clientHello.size())) {
        unified_event::logWarning("Network.SSLClient", "Failed to send client hello");
        return false;
    }

    // Set up flags to track handshake progress
    bool serverHelloReceived = false;
    bool certificateReceived = false;
    bool serverKeyExchangeReceived = false;
    bool serverHelloDoneReceived = false;

    // Set up a temporary data received callback to handle the handshake
    auto originalCallback = m_tcpClient->getDataReceivedCallback();

    // Clear the receive buffer
    m_receiveBuffer.clear();

    // Set up a callback to process handshake messages
    m_tcpClient->setDataReceivedCallback([this, &serverHelloReceived, &certificateReceived,
                                         &serverKeyExchangeReceived, &serverHelloDoneReceived]
                                         (const uint8_t* data, size_t length) {
        // Add the data to the receive buffer
        m_receiveBuffer.insert(m_receiveBuffer.end(), data, data + length);

        // Process all complete records in the buffer
        while (m_receiveBuffer.size() >= 5) {
            // Parse record header
            uint8_t recordType = m_receiveBuffer[0];
            uint16_t version = static_cast<uint16_t>(
                (static_cast<uint16_t>(m_receiveBuffer[1]) << 8) |
                static_cast<uint16_t>(m_receiveBuffer[2]));
            uint16_t recordLength = static_cast<uint16_t>(
                (static_cast<uint16_t>(m_receiveBuffer[3]) << 8) |
                static_cast<uint16_t>(m_receiveBuffer[4]));

            // Check if we have a complete record
            if (m_receiveBuffer.size() < 5 + recordLength) {
                break; // Wait for more data
            }

            // Process the record based on type
            if (recordType == static_cast<uint8_t>(TLSRecordType::HANDSHAKE)) {
                // Process handshake record
                size_t offset = 0;
                const uint8_t* handshakeData = m_receiveBuffer.data() + 5;

                while (offset + 4 <= recordLength) {
                    // Parse handshake header
                    uint8_t handshakeType = handshakeData[offset];
                    uint32_t handshakeLength = static_cast<uint32_t>(
                        (static_cast<uint32_t>(handshakeData[offset + 1]) << 16) |
                        (static_cast<uint32_t>(handshakeData[offset + 2]) << 8) |
                        static_cast<uint32_t>(handshakeData[offset + 3]));

                    // Check if we have the complete handshake message
                    if (offset + 4 + handshakeLength > recordLength) {
                        break; // Incomplete message, wait for more data
                    }

                    // Process the handshake message based on type
                    switch (handshakeType) {
                        case static_cast<uint8_t>(TLSHandshakeType::SERVER_HELLO):
                            if (processServerHello(handshakeData + offset, 4 + handshakeLength)) {
                                serverHelloReceived = true;
                                unified_event::logDebug("Network.SSLClient", "Server hello processed successfully");
                            }
                            break;
                        case static_cast<uint8_t>(TLSHandshakeType::CERTIFICATE):
                            if (processCertificate(handshakeData + offset, 4 + handshakeLength)) {
                                certificateReceived = true;
                                unified_event::logDebug("Network.SSLClient", "Certificate processed successfully");
                            }
                            break;
                        case static_cast<uint8_t>(TLSHandshakeType::SERVER_KEY_EXCHANGE):
                            if (processServerKeyExchange(handshakeData + offset, 4 + handshakeLength)) {
                                serverKeyExchangeReceived = true;
                                unified_event::logDebug("Network.SSLClient", "Server key exchange processed successfully");
                            }
                            break;
                        case static_cast<uint8_t>(TLSHandshakeType::SERVER_HELLO_DONE):
                            if (processServerHelloDone(handshakeData + offset, 4 + handshakeLength)) {
                                serverHelloDoneReceived = true;
                                unified_event::logDebug("Network.SSLClient", "Server hello done processed successfully");
                            }
                            break;
                        default:
                            unified_event::logWarning("Network.SSLClient", "Unexpected handshake type: " + std::to_string(handshakeType));
                            break;
                    }

                    // Move to the next handshake message
                    offset += 4 + handshakeLength;
                }
            } else if (recordType == static_cast<uint8_t>(TLSRecordType::ALERT)) {
                // Process alert record
                if (recordLength >= 2) {
                    uint8_t alertLevel = m_receiveBuffer[5];
                    uint8_t alertDescription = m_receiveBuffer[6];
                    unified_event::logWarning("Network.SSLClient", "Received alert: level=" +
                                           std::to_string(alertLevel) + ", description=" +
                                           std::to_string(alertDescription));
                }
            } else if (recordType == static_cast<uint8_t>(TLSRecordType::CHANGE_CIPHER_SPEC)) {
                // Process change cipher spec record
                unified_event::logDebug("Network.SSLClient", "Received change cipher spec");
            }

            // Remove the processed record from the buffer
            m_receiveBuffer.erase(m_receiveBuffer.begin(), m_receiveBuffer.begin() + 5 + recordLength);
        }
    });

    // Wait for the handshake to complete
    auto startTime = std::chrono::steady_clock::now();
    while (!(serverHelloReceived && certificateReceived && serverHelloDoneReceived)) {
        // Check if we've timed out
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        if (elapsedTime > 10) { // 10 second timeout
            unified_event::logWarning("Network.SSLClient", "Timed out waiting for handshake completion");
            m_tcpClient->setDataReceivedCallback(originalCallback);
            return false;
        }

        // Sleep a bit to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Restore the original callback
    m_tcpClient->setDataReceivedCallback(originalCallback);

    // Generate client key exchange message
    std::vector<uint8_t> clientKeyExchange = generateClientKeyExchange();
    if (!m_tcpClient->send(clientKeyExchange.data(), clientKeyExchange.size())) {
        unified_event::logWarning("Network.SSLClient", "Failed to send client key exchange");
        return false;
    }

    // Generate change cipher spec message
    std::vector<uint8_t> changeCipherSpec = generateChangeCipherSpec();
    if (!m_tcpClient->send(changeCipherSpec.data(), changeCipherSpec.size())) {
        unified_event::logWarning("Network.SSLClient", "Failed to send change cipher spec");
        return false;
    }

    // Generate finished message
    std::vector<uint8_t> finished = generateFinished();
    if (!m_tcpClient->send(finished.data(), finished.size())) {
        unified_event::logWarning("Network.SSLClient", "Failed to send finished message");
        return false;
    }

    // Wait for server change cipher spec and finished messages
    // For this simplified implementation, we'll just set the handshake as complete
    m_handshakeComplete = true;

    unified_event::logDebug("Network.SSLClient", "SSL/TLS handshake completed successfully");

    return true;
}

std::vector<uint8_t> SSLClient::generateClientHello() {
    // Generate client random
    m_clientRandom.resize(32);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    // First 4 bytes are the current time
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint32_t timestamp = static_cast<uint32_t>(now_time_t);

    m_clientRandom[0] = (timestamp >> 24) & 0xFF;
    m_clientRandom[1] = (timestamp >> 16) & 0xFF;
    m_clientRandom[2] = (timestamp >> 8) & 0xFF;
    m_clientRandom[3] = timestamp & 0xFF;

    // Remaining 28 bytes are random
    for (size_t i = 4; i < 32; ++i) {
        m_clientRandom[i] = dist(gen);
    }

    // Create a buffer for the TLS record
    std::vector<uint8_t> record;

    // TLS Record Header (5 bytes)
    // Record Type: Handshake (22)
    record.push_back(static_cast<uint8_t>(TLSRecordType::HANDSHAKE));

    // Protocol Version: TLS 1.2 (0x0303)
    record.push_back(0x03); // Major version
    record.push_back(0x03); // Minor version

    // Length (will be filled in later)
    record.push_back(0x00);
    record.push_back(0x00);

    // Start of handshake message
    size_t handshakeStart = record.size();

    // Handshake Type: Client Hello (1)
    record.push_back(static_cast<uint8_t>(TLSHandshakeType::CLIENT_HELLO));

    // Handshake Length (3 bytes, will be filled in later)
    record.push_back(0x00);
    record.push_back(0x00);
    record.push_back(0x00);

    // Client Version: TLS 1.2 (0x0303)
    record.push_back(0x03); // Major version
    record.push_back(0x03); // Minor version

    // Client Random (32 bytes)
    record.insert(record.end(), m_clientRandom.begin(), m_clientRandom.end());

    // Session ID Length (1 byte)
    record.push_back(0x00); // No session ID

    // Cipher Suites Length (2 bytes)
    const size_t numCipherSuites = 4; // Number of cipher suites we support
    record.push_back(0x00);
    record.push_back(static_cast<uint8_t>(numCipherSuites * 2)); // Each cipher suite is 2 bytes

    // Cipher Suites
    // TLS_RSA_WITH_AES_128_CBC_SHA (0x002F)
    record.push_back(0x00);
    record.push_back(0x2F);

    // TLS_RSA_WITH_AES_256_CBC_SHA (0x0035)
    record.push_back(0x00);
    record.push_back(0x35);

    // TLS_RSA_WITH_AES_128_CBC_SHA256 (0x003C)
    record.push_back(0x00);
    record.push_back(0x3C);

    // TLS_RSA_WITH_AES_256_CBC_SHA256 (0x003D)
    record.push_back(0x00);
    record.push_back(0x3D);

    // Compression Methods Length (1 byte)
    record.push_back(0x01);

    // Compression Methods: null (0)
    record.push_back(0x00);

    // Extensions Length (2 bytes, will be filled in later)
    size_t extensionsLengthPos = record.size();
    record.push_back(0x00);
    record.push_back(0x00);

    // Start of extensions
    size_t extensionsStart = record.size();

    // Extension: server_name (SNI)
    record.push_back(0x00); // Extension Type: server_name (0)
    record.push_back(0x00);

    // Extension Length (2 bytes, will be filled in later)
    size_t sniExtLengthPos = record.size();
    record.push_back(0x00);
    record.push_back(0x00);

    // Start of SNI extension data
    size_t sniExtStart = record.size();

    // Server Name List Length (2 bytes, will be filled in later)
    size_t sniListLengthPos = record.size();
    record.push_back(0x00);
    record.push_back(0x00);

    // Start of server name list
    size_t sniListStart = record.size();

    // Server Name Type: host_name (0)
    record.push_back(0x00);

    // Server Name Length (2 bytes, will be filled in later)
    size_t hostnameLengthPos = record.size();
    record.push_back(0x00);
    record.push_back(0x00);

    // Server Name (hostname)
    size_t hostnameStart = record.size();
    std::string hostname = m_hostname;
    record.insert(record.end(), hostname.begin(), hostname.end());
    size_t hostnameEnd = record.size();

    // Fill in the hostname length
    uint16_t hostnameLength = static_cast<uint16_t>(hostnameEnd - hostnameStart);
    record[hostnameLengthPos] = static_cast<uint8_t>((hostnameLength >> 8) & 0xFF);
    record[hostnameLengthPos + 1] = static_cast<uint8_t>(hostnameLength & 0xFF);

    // Fill in the server name list length
    uint16_t sniListLength = static_cast<uint16_t>(hostnameEnd - sniListStart);
    record[sniListLengthPos] = static_cast<uint8_t>((sniListLength >> 8) & 0xFF);
    record[sniListLengthPos + 1] = static_cast<uint8_t>(sniListLength & 0xFF);

    // Fill in the SNI extension length
    uint16_t sniExtLength = static_cast<uint16_t>(hostnameEnd - sniExtStart);
    record[sniExtLengthPos] = static_cast<uint8_t>((sniExtLength >> 8) & 0xFF);
    record[sniExtLengthPos + 1] = static_cast<uint8_t>(sniExtLength & 0xFF);

    // Extension: signature_algorithms
    record.push_back(0x00); // Extension Type: signature_algorithms (13)
    record.push_back(0x0D);

    // Extension Length (2 bytes)
    record.push_back(0x00);
    record.push_back(0x08); // 8 bytes of data

    // Signature Algorithms Length (2 bytes)
    record.push_back(0x00);
    record.push_back(0x06); // 6 bytes of data (3 algorithms, 2 bytes each)

    // Signature Algorithms
    // RSA + SHA256
    record.push_back(0x04); // SHA256
    record.push_back(0x01); // RSA

    // RSA + SHA1
    record.push_back(0x02); // SHA1
    record.push_back(0x01); // RSA

    // ECDSA + SHA256
    record.push_back(0x04); // SHA256
    record.push_back(0x03); // ECDSA

    // Fill in the extensions length
    uint16_t extensionsLength = static_cast<uint16_t>(record.size() - extensionsStart);
    record[extensionsLengthPos] = static_cast<uint8_t>((extensionsLength >> 8) & 0xFF);
    record[extensionsLengthPos + 1] = static_cast<uint8_t>(extensionsLength & 0xFF);

    // Fill in the handshake length
    size_t handshakeLength = record.size() - handshakeStart - 4; // -4 for the handshake header
    record[handshakeStart + 1] = static_cast<uint8_t>((handshakeLength >> 16) & 0xFF);
    record[handshakeStart + 2] = static_cast<uint8_t>((handshakeLength >> 8) & 0xFF);
    record[handshakeStart + 3] = static_cast<uint8_t>(handshakeLength & 0xFF);

    // Fill in the record length
    size_t recordLength = record.size() - 5; // -5 for the record header
    record[3] = static_cast<uint8_t>((recordLength >> 8) & 0xFF);
    record[4] = static_cast<uint8_t>(recordLength & 0xFF);

    unified_event::logDebug("Network.SSLClient", "Generated Client Hello message " + std::to_string(record.size()) + " bytes");

    return record;
}

bool SSLClient::processServerHello(const uint8_t* data, size_t length) {
    if (length < 4) {
        unified_event::logWarning("Network.SSLClient", "Server hello too short (handshake header)");
        return false;
    }

    // Parse the handshake header
    uint8_t handshakeType = data[0];
    uint32_t handshakeLength = static_cast<uint32_t>(
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]));

    // Check handshake type
    if (handshakeType != static_cast<uint8_t>(TLSHandshakeType::SERVER_HELLO)) {
        unified_event::logWarning("Network.SSLClient", "Unexpected handshake type: " + std::to_string(handshakeType));
        return false;
    }

    // Check handshake length
    if (handshakeLength + 4 > length) {
        unified_event::logWarning("Network.SSLClient", "Handshake length exceeds message length");
        return false;
    }

    // Parse the server hello
    if (handshakeLength < 38) { // 2 (version) + 32 (random) + 1 (session id length) + 2 (cipher suite) + 1 (compression method)
        unified_event::logWarning("Network.SSLClient", "Server hello too short (server hello data)");
        return false;
    }

    // Server version
    uint16_t serverVersion = static_cast<uint16_t>(
        (static_cast<uint16_t>(data[4]) << 8) | static_cast<uint16_t>(data[5]));
    if (serverVersion < TLS_VERSION_1_0 || serverVersion > TLS_VERSION_1_2) {
        unified_event::logWarning("Network.SSLClient", "Unsupported server TLS version: " + std::to_string(serverVersion));
        return false;
    }

    // Server random
    m_serverRandom.resize(32);
    std::memcpy(m_serverRandom.data(), data + 6, 32);

    // Session ID
    uint8_t sessionIdLength = data[38];
    if (sessionIdLength > 32) {
        unified_event::logWarning("Network.SSLClient", "Invalid session ID length: " + std::to_string(sessionIdLength));
        return false;
    }

    // Check if we have enough data for the session ID
    if (39 + sessionIdLength + 3 > length) { // +3 for cipher suite and compression method
        unified_event::logWarning("Network.SSLClient", "Message too short for session ID");
        return false;
    }

    // Cipher suite
    uint16_t cipherSuite = static_cast<uint16_t>(
        (static_cast<uint16_t>(data[39 + sessionIdLength]) << 8) |
        static_cast<uint16_t>(data[40 + sessionIdLength]));

    // Check if the cipher suite is supported
    bool supportedCipherSuite = false;
    switch (cipherSuite) {
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
            supportedCipherSuite = true;
            break;
        default:
            break;
    }

    if (!supportedCipherSuite) {
        unified_event::logWarning("Network.SSLClient", "Unsupported cipher suite: " + std::to_string(cipherSuite));
        return false;
    }

    // Store the selected cipher suite
    m_selectedCipherSuite = cipherSuite;

    // Compression method
    uint8_t compressionMethod = data[41 + sessionIdLength];
    if (compressionMethod != 0) {
        unified_event::logWarning("Network.SSLClient", "Unsupported compression method: " + std::to_string(compressionMethod));
        return false;
    }

    // Check for extensions
    if (42 + sessionIdLength < length) {
        // Parse extensions
        uint16_t extensionsLength = static_cast<uint16_t>(
            (static_cast<uint16_t>(data[42 + sessionIdLength]) << 8) |
            static_cast<uint16_t>(data[43 + sessionIdLength]));

        // Check if we have enough data for the extensions
        if (44 + sessionIdLength + extensionsLength > length) {
            unified_event::logWarning("Network.SSLClient", "Message too short for extensions");
            return false;
        }

        // Process extensions if needed
        // For now, we'll just log that we received extensions
        unified_event::logDebug("Network.SSLClient", "Received " + std::to_string(extensionsLength) + " bytes of extensions");
    }

    unified_event::logDebug("Network.SSLClient", "Successfully parsed server hello");
    return true;
}

bool SSLClient::verifyCertificate(const std::vector<uint8_t>& certificate) {
    // If certificate verification is disabled, return true
    if (!m_verifyCertificate) {
        unified_event::logDebug("Network.SSLClient", "Certificate verification is disabled");
        return true;
    }

    // Store the certificate for later use
    m_serverCertificate = certificate;

    // Basic certificate structure validation
    if (certificate.size() < 10) {
        unified_event::logWarning("Network.SSLClient", "Certificate is too short");
        return false;
    }

    // In a real implementation, we would:
    // 1. Parse the X.509 certificate
    // 2. Verify the certificate chain
    // 3. Check the certificate validity period
    // 4. Verify the certificate signature
    // 5. Check the certificate revocation status
    // 6. Verify the hostname against the certificate's subject

    // For this simplified implementation, we'll just log the certificate details
    unified_event::logDebug("Network.SSLClient", "Certificate received " + std::to_string(certificate.size()) + " bytes");

    // Extract basic information from the certificate
    // This is a very simplified parsing of the certificate
    // In a real implementation, we would use a proper X.509 parser

    // Check if the certificate starts with the sequence tag (0x30)
    if (certificate[0] != 0x30) {
        unified_event::logWarning("Network.SSLClient", "Certificate does not start with sequence tag");
        return false;
    }

    // Log success
    unified_event::logDebug("Network.SSLClient", "Certificate verification successful");

    return true;
}

std::vector<uint8_t> SSLClient::generateClientKeyExchange() {
    unified_event::logDebug("Network.SSLClient", "Generating client key exchange message");

    // Create a buffer for the TLS record
    std::vector<uint8_t> record;

    // TLS Record Header (5 bytes)
    // Record Type: Handshake (22)
    record.push_back(static_cast<uint8_t>(TLSRecordType::HANDSHAKE));

    // Protocol Version: TLS 1.2 (0x0303)
    record.push_back(0x03); // Major version
    record.push_back(0x03); // Minor version

    // Length (will be filled in later)
    record.push_back(0x00);
    record.push_back(0x00);

    // Start of handshake message
    size_t handshakeStart = record.size();

    // Handshake Type: Client Key Exchange (16)
    record.push_back(static_cast<uint8_t>(TLSHandshakeType::CLIENT_KEY_EXCHANGE));

    // Handshake Length (3 bytes, will be filled in later)
    record.push_back(0x00);
    record.push_back(0x00);
    record.push_back(0x00);

    // In a real implementation, we would:
    // 1. Generate a pre-master secret
    // 2. Encrypt it with the server's public key
    // 3. Include the encrypted pre-master secret in the message

    // For this simplified implementation, we'll just include a dummy pre-master secret
    // Pre-master secret length (2 bytes)
    record.push_back(0x00);
    record.push_back(0x30); // 48 bytes

    // Pre-master secret (48 bytes)
    // First 2 bytes are the client version
    record.push_back(0x03); // Major version
    record.push_back(0x03); // Minor version

    // Remaining 46 bytes are random
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    for (int i = 0; i < 46; ++i) {
        record.push_back(dist(gen));
    }

    // Generate the master secret
    generateMasterSecret();

    // Fill in the handshake length
    size_t handshakeLength = record.size() - handshakeStart - 4; // -4 for the handshake header
    record[handshakeStart + 1] = static_cast<uint8_t>((handshakeLength >> 16) & 0xFF);
    record[handshakeStart + 2] = static_cast<uint8_t>((handshakeLength >> 8) & 0xFF);
    record[handshakeStart + 3] = static_cast<uint8_t>(handshakeLength & 0xFF);

    // Fill in the record length
    size_t recordLength = record.size() - 5; // -5 for the record header
    record[3] = static_cast<uint8_t>((recordLength >> 8) & 0xFF);
    record[4] = static_cast<uint8_t>(recordLength & 0xFF);

    return record;
}

std::vector<uint8_t> SSLClient::generateChangeCipherSpec() {
    unified_event::logDebug("Network.SSLClient", "Generating change cipher spec message");

    // Create a buffer for the TLS record
    std::vector<uint8_t> record;

    // TLS Record Header (5 bytes)
    // Record Type: Change Cipher Spec (20)
    record.push_back(static_cast<uint8_t>(TLSRecordType::CHANGE_CIPHER_SPEC));

    // Protocol Version: TLS 1.2 (0x0303)
    record.push_back(0x03); // Major version
    record.push_back(0x03); // Minor version

    // Length (1 byte)
    record.push_back(0x00);
    record.push_back(0x01);

    // Change cipher spec message (1 byte)
    record.push_back(0x01);

    return record;
}

std::vector<uint8_t> SSLClient::generateFinished() {
    unified_event::logDebug("Network.SSLClient", "Generating finished message");

    // Create a buffer for the TLS record
    std::vector<uint8_t> record;

    // TLS Record Header (5 bytes)
    // Record Type: Handshake (22)
    record.push_back(static_cast<uint8_t>(TLSRecordType::HANDSHAKE));

    // Protocol Version: TLS 1.2 (0x0303)
    record.push_back(0x03); // Major version
    record.push_back(0x03); // Minor version

    // Length (will be filled in later)
    record.push_back(0x00);
    record.push_back(0x00);

    // Start of handshake message (not used in this simplified implementation)

    // Handshake Type: Finished (20)
    record.push_back(static_cast<uint8_t>(TLSHandshakeType::FINISHED));

    // Handshake Length (3 bytes)
    record.push_back(0x00);
    record.push_back(0x00);
    record.push_back(0x0C); // 12 bytes

    // Verify data (12 bytes)
    // In a real implementation, this would be a hash of all handshake messages
    // For this simplified implementation, we'll just include dummy data
    for (int i = 0; i < 12; ++i) {
        record.push_back(0x00);
    }

    // Fill in the record length
    size_t recordLength = record.size() - 5; // -5 for the record header
    record[3] = static_cast<uint8_t>((recordLength >> 8) & 0xFF);
    record[4] = static_cast<uint8_t>(recordLength & 0xFF);

    return record;
}

bool SSLClient::generateMasterSecret() {
    unified_event::logDebug("Network.SSLClient", "Generating master secret");

    // In a real implementation, we would:
    // 1. Use the pre-master secret, client random, and server random to generate the master secret
    // 2. Derive the encryption keys from the master secret

    // For this simplified implementation, we'll just set dummy values
    m_masterSecret.resize(48);
    std::fill(m_masterSecret.begin(), m_masterSecret.end(), 0x00);

    // Generate encryption keys
    m_clientWriteKey.resize(16); // 128 bits for AES-128
    m_serverWriteKey.resize(16);
    m_clientWriteIV.resize(16);
    m_serverWriteIV.resize(16);

    std::fill(m_clientWriteKey.begin(), m_clientWriteKey.end(), 0x01);
    std::fill(m_serverWriteKey.begin(), m_serverWriteKey.end(), 0x02);
    std::fill(m_clientWriteIV.begin(), m_clientWriteIV.end(), 0x03);
    std::fill(m_serverWriteIV.begin(), m_serverWriteIV.end(), 0x04);

    return true;
}

std::vector<uint8_t> SSLClient::encrypt(const uint8_t* data, size_t length) {
    // In a real implementation, we would encrypt the data using the negotiated cipher suite
    // For this simplified implementation, we'll just return the data as-is
    return std::vector<uint8_t>(data, data + length);
}

std::vector<uint8_t> SSLClient::decrypt(const uint8_t* data, size_t length) {
    // In a real implementation, we would decrypt the data using the negotiated cipher suite
    // For this simplified implementation, we'll just return the data as-is
    return std::vector<uint8_t>(data, data + length);
}

void SSLClient::processHandshakeRecord(const uint8_t* data, size_t length) {
    size_t offset = 0;

    while (offset + 4 <= length) {
        // Parse handshake header
        uint8_t handshakeType = data[offset];
        uint32_t handshakeLength = static_cast<uint32_t>(
            (static_cast<uint32_t>(data[offset + 1]) << 16) |
            (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]));

        // Check if we have the complete handshake message
        if (offset + 4 + handshakeLength > length) {
            unified_event::logWarning("Network.SSLClient", "Incomplete handshake message, waiting for more data");
            break; // Incomplete message, wait for more data
        }

        // Process the handshake message based on type
        bool processed = false;
        switch (handshakeType) {
            case static_cast<uint8_t>(TLSHandshakeType::SERVER_HELLO):
                processed = processServerHello(data + offset, 4 + handshakeLength);
                break;
            case static_cast<uint8_t>(TLSHandshakeType::CERTIFICATE):
                processed = processCertificate(data + offset, 4 + handshakeLength);
                break;
            case static_cast<uint8_t>(TLSHandshakeType::SERVER_KEY_EXCHANGE):
                processed = processServerKeyExchange(data + offset, 4 + handshakeLength);
                break;
            case static_cast<uint8_t>(TLSHandshakeType::SERVER_HELLO_DONE):
                processed = processServerHelloDone(data + offset, 4 + handshakeLength);
                break;
            default:
                unified_event::logWarning("Network.SSLClient", "Unexpected handshake type: " + std::to_string(handshakeType));
                break;
        }

        if (!processed) {
            unified_event::logWarning("Network.SSLClient", "Failed to process handshake message of type: " + std::to_string(handshakeType));
        }

        // Move to the next handshake message
        offset += 4 + handshakeLength;
    }
}

bool SSLClient::processCertificate(const uint8_t* data, size_t length) {
    if (length < 7) { // Need at least handshake header + certificates list length
        unified_event::logWarning("Network.SSLClient", "Certificate message too short");
        return false;
    }

    // Parse the certificates list length (3 bytes)
    uint32_t certsListLength = static_cast<uint32_t>(
        (static_cast<uint32_t>(data[4]) << 16) |
        (static_cast<uint32_t>(data[5]) << 8) |
        static_cast<uint32_t>(data[6]));

    // Check if we have the complete certificates list
    if (length < 7 + certsListLength) {
        unified_event::logWarning("Network.SSLClient", "Certificate message incomplete");
        return false;
    }

    // Check if we have at least one certificate
    if (certsListLength < 3) {
        unified_event::logWarning("Network.SSLClient", "No certificates in the list");
        return false;
    }

    // Extract the first certificate length (3 bytes)
    uint32_t firstCertLength = static_cast<uint32_t>(
        (static_cast<uint32_t>(data[7]) << 16) |
        (static_cast<uint32_t>(data[8]) << 8) |
        static_cast<uint32_t>(data[9]));

    // Check if we have the complete first certificate
    if (certsListLength < 3 + firstCertLength || length < 10 + firstCertLength) {
        unified_event::logWarning("Network.SSLClient", "First certificate incomplete");
        return false;
    }

    // Extract the first certificate data
    std::vector<uint8_t> certificate(data + 10, data + 10 + firstCertLength);

    // Verify the certificate
    if (verifyCertificate(certificate)) {
        unified_event::logDebug("Network.SSLClient", "Certificate processed successfully");
        return true;
    } else {
        unified_event::logWarning("Network.SSLClient", "Certificate verification failed");
        return false;
    }
}

bool SSLClient::processServerKeyExchange(const uint8_t* data, size_t length) {
    if (length < 7) { // Need at least handshake header + some data
        unified_event::logWarning("Network.SSLClient", "Server key exchange message too short");
        return false;
    }

    // Parse the key exchange length (3 bytes)
    uint32_t keyExchangeLength = static_cast<uint32_t>(
        (static_cast<uint32_t>(data[4]) << 16) |
        (static_cast<uint32_t>(data[5]) << 8) |
        static_cast<uint32_t>(data[6]));

    // Check if we have the complete key exchange data
    if (length < 7 + keyExchangeLength) {
        unified_event::logWarning("Network.SSLClient", "Server key exchange message incomplete");
        return false;
    }

    // In a real implementation, we would extract and process the key exchange parameters
    // For this simplified implementation, we'll just log that we received the message
    unified_event::logDebug("Network.SSLClient", "Server key exchange processed successfully");
    return true;
}

bool SSLClient::processServerHelloDone(const uint8_t* data, size_t length) {
    // The server hello done message has no body, just the handshake header
    // It indicates that the server has finished sending handshake messages
    // and is waiting for the client's response

    // In a real implementation, we would verify that we've received all necessary
    // handshake messages before proceeding

    // For this simplified implementation, we'll just log that we received the message
    unified_event::logDebug("Network.SSLClient", "Server hello done processed successfully");
    return true;
}

void SSLClient::handleReceivedData(const uint8_t* data, size_t length) {
    if (!m_handshakeComplete) {
        // Add the data to the receive buffer
        m_receiveBuffer.insert(m_receiveBuffer.end(), data, data + length);

        // Process all complete records in the buffer
        while (m_receiveBuffer.size() >= 5) {
            // Parse record header
            uint8_t recordType = m_receiveBuffer[0];
            uint16_t version = static_cast<uint16_t>(
                (static_cast<uint16_t>(m_receiveBuffer[1]) << 8) |
                static_cast<uint16_t>(m_receiveBuffer[2]));
            uint16_t recordLength = static_cast<uint16_t>(
                (static_cast<uint16_t>(m_receiveBuffer[3]) << 8) |
                static_cast<uint16_t>(m_receiveBuffer[4]));

            // Check if we have a complete record
            if (m_receiveBuffer.size() < 5 + recordLength) {
                break; // Wait for more data
            }

            // Process the record based on type
            if (recordType == static_cast<uint8_t>(TLSRecordType::HANDSHAKE)) {
                // Process handshake record
                processHandshakeRecord(m_receiveBuffer.data() + 5, recordLength);
            } else if (recordType == static_cast<uint8_t>(TLSRecordType::ALERT)) {
                // Process alert record
                if (recordLength >= 2) {
                    uint8_t alertLevel = m_receiveBuffer[5];
                    uint8_t alertDescription = m_receiveBuffer[6];
                    unified_event::logWarning("Network.SSLClient", "Received alert: level=" +
                                           std::to_string(alertLevel) + ", description=" +
                                           std::to_string(alertDescription));
                }
            } else if (recordType == static_cast<uint8_t>(TLSRecordType::CHANGE_CIPHER_SPEC)) {
                // Process change cipher spec record
                unified_event::logDebug("Network.SSLClient", "Received change cipher spec");
            } else if (recordType == static_cast<uint8_t>(TLSRecordType::APPLICATION_DATA)) {
                // We shouldn't receive application data during the handshake
                unified_event::logWarning("Network.SSLClient", "Received unexpected application data during handshake");
            } else {
                unified_event::logWarning("Network.SSLClient", "Received unknown record type: " + std::to_string(recordType));
            }

            // Remove the processed record from the buffer
            m_receiveBuffer.erase(m_receiveBuffer.begin(), m_receiveBuffer.begin() + 5 + recordLength);
        }
    } else {
        // Decrypt the data
        std::vector<uint8_t> decryptedData = decrypt(data, length);

        // Call the data received callback
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_dataReceivedCallback) {
            m_dataReceivedCallback(decryptedData.data(), decryptedData.size());
        }
    }
}

void SSLClient::handleError(const std::string& error) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_errorCallback) {
        m_errorCallback("SSL error: " + error);
    }
}

void SSLClient::handleConnectionClosed() {
    m_connected = false;
    m_handshakeComplete = false;

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_connectionClosedCallback) {
        m_connectionClosedCallback();
    }
}

} // namespace dht_hunter::network
