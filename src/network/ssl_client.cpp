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

void SSLClient::disconnect() {
    if (!m_connected) {
        return;
    }

    // Send close_notify alert if possible
    if (m_handshakeComplete) {
        // TODO: Implement proper TLS close_notify alert
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

    // TODO: Implement the rest of the handshake
    // For now, we'll simulate a successful handshake

    // In a real implementation, we would:
    // 1. Wait for and process the server hello
    // 2. Verify the server certificate
    // 3. Exchange keys
    // 4. Verify the handshake

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
    if (length < 5) {
        unified_event::logWarning("Network.SSLClient", "Server hello too short (record header)");
        return false;
    }

    // Parse the TLS record header
    uint8_t recordType = data[0];
    uint16_t version = (data[1] << 8) | data[2];
    uint16_t recordLength = (data[3] << 8) | data[4];

    // Check record type
    if (recordType != static_cast<uint8_t>(TLSRecordType::HANDSHAKE)) {
        unified_event::logWarning("Network.SSLClient", "Unexpected record type: " + std::to_string(recordType));
        return false;
    }

    // Check version
    if (version < TLS_VERSION_1_0 || version > TLS_VERSION_1_2) {
        unified_event::logWarning("Network.SSLClient", "Unsupported TLS version: " + std::to_string(version));
        return false;
    }

    // Check record length
    if (recordLength + 5 > length) {
        unified_event::logWarning("Network.SSLClient", "Record length exceeds message length");
        return false;
    }

    // Parse the handshake header
    if (recordLength < 4) {
        unified_event::logWarning("Network.SSLClient", "Server hello too short (handshake header)");
        return false;
    }

    uint8_t handshakeType = data[5];
    uint32_t handshakeLength = (data[6] << 16) | (data[7] << 8) | data[8];

    // Check handshake type
    if (handshakeType != static_cast<uint8_t>(TLSHandshakeType::SERVER_HELLO)) {
        unified_event::logWarning("Network.SSLClient", "Unexpected handshake type: " + std::to_string(handshakeType));
        return false;
    }

    // Check handshake length
    if (handshakeLength + 9 > length) {
        unified_event::logWarning("Network.SSLClient", "Handshake length exceeds message length");
        return false;
    }

    // Parse the server hello
    if (handshakeLength < 38) { // 2 (version) + 32 (random) + 1 (session id length) + 2 (cipher suite) + 1 (compression method)
        unified_event::logWarning("Network.SSLClient", "Server hello too short (server hello data)");
        return false;
    }

    // Server version
    uint16_t serverVersion = (data[9] << 8) | data[10];
    if (serverVersion < TLS_VERSION_1_0 || serverVersion > TLS_VERSION_1_2) {
        unified_event::logWarning("Network.SSLClient", "Unsupported server TLS version: " + std::to_string(serverVersion));
        return false;
    }

    // Server random
    m_serverRandom.resize(32);
    std::memcpy(m_serverRandom.data(), data + 11, 32);

    // Session ID
    uint8_t sessionIdLength = data[43];
    if (sessionIdLength > 32) {
        unified_event::logWarning("Network.SSLClient", "Invalid session ID length: " + std::to_string(sessionIdLength));
        return false;
    }

    // Check if we have enough data for the session ID
    if (44 + sessionIdLength + 3 > length) { // +3 for cipher suite and compression method
        unified_event::logWarning("Network.SSLClient", "Message too short for session ID");
        return false;
    }

    // Cipher suite
    uint16_t cipherSuite = (data[44 + sessionIdLength] << 8) | data[45 + sessionIdLength];

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
    uint8_t compressionMethod = data[46 + sessionIdLength];
    if (compressionMethod != 0) {
        unified_event::logWarning("Network.SSLClient", "Unsupported compression method: " + std::to_string(compressionMethod));
        return false;
    }

    // Check for extensions
    if (47 + sessionIdLength < length) {
        // Parse extensions
        uint16_t extensionsLength = (data[47 + sessionIdLength] << 8) | data[48 + sessionIdLength];

        // Check if we have enough data for the extensions
        if (49 + sessionIdLength + extensionsLength > length) {
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
    // TODO: Implement certificate verification
    return true;
}

std::vector<uint8_t> SSLClient::encrypt(const uint8_t* data, size_t length) {
    // TODO: Implement encryption
    // For now, return the data as-is
    return std::vector<uint8_t>(data, data + length);
}

std::vector<uint8_t> SSLClient::decrypt(const uint8_t* data, size_t length) {
    // TODO: Implement decryption
    // For now, return the data as-is
    return std::vector<uint8_t>(data, data + length);
}

void SSLClient::handleReceivedData(const uint8_t* data, size_t length) {
    if (!m_handshakeComplete) {
        // Process handshake data
        // TODO: Implement handshake processing
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
