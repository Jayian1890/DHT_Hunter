#pragma once

#include "dht_hunter/network/tcp_client.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <array>

namespace dht_hunter::network {

// Forward declarations for TLS enums
enum class TLSAlertLevel : uint8_t;
enum class TLSAlertDescription : uint8_t;

/**
 * @brief SSL/TLS client for secure connections
 *
 * This is a basic implementation of SSL/TLS without external libraries.
 * It supports TLS 1.2 with a limited set of cipher suites.
 */
class SSLClient {
public:
    /**
     * @brief Constructor
     */
    SSLClient();

    /**
     * @brief Destructor
     */
    ~SSLClient();

    /**
     * @brief Connect to a server using SSL/TLS
     * @param host The hostname
     * @param port The port
     * @param timeoutSec Connection timeout in seconds
     * @return True if the connection was established, false otherwise
     */
    bool connect(const std::string& host, uint16_t port, int timeoutSec = 10);

    /**
     * @brief Disconnect from the server
     */
    void disconnect();

    /**
     * @brief Check if the client is connected
     * @return True if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * @brief Send data over the secure connection
     * @param data The data to send
     * @param length The length of the data
     * @return True if the data was sent, false otherwise
     */
    bool send(const uint8_t* data, size_t length);

    /**
     * @brief Set the data received callback
     * @param callback The callback function
     */
    void setDataReceivedCallback(std::function<void(const uint8_t*, size_t)> callback);

    /**
     * @brief Set the error callback
     * @param callback The callback function
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Set the connection closed callback
     * @param callback The callback function
     */
    void setConnectionClosedCallback(std::function<void()> callback);

    /**
     * @brief Set whether to verify the server certificate
     * @param verify True to verify, false to skip verification
     */
    void setVerifyCertificate(bool verify);

private:
    /**
     * @brief Perform the SSL/TLS handshake
     * @return True if the handshake was successful, false otherwise
     */
    bool performHandshake();

    /**
     * @brief Generate a client hello message
     * @return The client hello message
     */
    std::vector<uint8_t> generateClientHello();

    /**
     * @brief Process the server hello message
     * @param data The server hello message
     * @param length The length of the message
     * @return True if the message was processed successfully, false otherwise
     */
    bool processServerHello(const uint8_t* data, size_t length);

    /**
     * @brief Verify the server certificate
     * @param certificate The server certificate
     * @return True if the certificate is valid, false otherwise
     */
    bool verifyCertificate(const std::vector<uint8_t>& certificate);

    /**
     * @brief Process a handshake record
     * @param data The handshake record data (without the record header)
     * @param length The length of the record data
     */
    void processHandshakeRecord(const uint8_t* data, size_t length);

    /**
     * @brief Process a certificate message
     * @param data The certificate message data (including the handshake header)
     * @param length The length of the message data
     * @return True if the certificate was processed successfully, false otherwise
     */
    bool processCertificate(const uint8_t* data, size_t length);

    /**
     * @brief Process a server key exchange message
     * @param data The server key exchange message data (including the handshake header)
     * @param length The length of the message data
     * @return True if the message was processed successfully, false otherwise
     */
    bool processServerKeyExchange(const uint8_t* data, size_t length);

    /**
     * @brief Process a server hello done message
     * @param data The server hello done message data (including the handshake header)
     * @param length The length of the message data
     * @return True if the message was processed successfully, false otherwise
     */
    bool processServerHelloDone(const uint8_t* data, size_t length);

    /**
     * @brief Generate a client key exchange message
     * @return The client key exchange message
     */
    std::vector<uint8_t> generateClientKeyExchange();

    /**
     * @brief Generate a change cipher spec message
     * @return The change cipher spec message
     */
    std::vector<uint8_t> generateChangeCipherSpec();

    /**
     * @brief Generate a finished message
     * @return The finished message
     */
    std::vector<uint8_t> generateFinished();

    /**
     * @brief Generate the master secret
     * @return True if the master secret was generated successfully, false otherwise
     */
    bool generateMasterSecret();

    /**
     * @brief Encrypt data using the negotiated cipher suite
     * @param data The data to encrypt
     * @param length The length of the data
     * @return The encrypted data
     */
    std::vector<uint8_t> encrypt(const uint8_t* data, size_t length);

    /**
     * @brief Decrypt data using the negotiated cipher suite
     * @param data The data to decrypt
     * @param length The length of the data
     * @return The decrypted data
     */
    std::vector<uint8_t> decrypt(const uint8_t* data, size_t length);

    /**
     * @brief Handle received data from the TCP client
     * @param data The received data
     * @param length The length of the data
     */
    void handleReceivedData(const uint8_t* data, size_t length);

    /**
     * @brief Handle errors from the TCP client
     * @param error The error message
     */
    void handleError(const std::string& error);

    /**
     * @brief Handle connection closed from the TCP client
     */
    void handleConnectionClosed();

    /**
     * @brief Generate a TLS alert message
     * @param level The alert level (warning or fatal)
     * @param description The alert description
     * @return The alert message
     */
    std::vector<uint8_t> generateAlert(TLSAlertLevel level, TLSAlertDescription description);

    // TCP client for the underlying connection
    std::shared_ptr<TCPClient> m_tcpClient;

    // Connection state
    bool m_connected;
    bool m_handshakeComplete;
    bool m_verifyCertificate;

    // Callbacks
    std::function<void(const uint8_t*, size_t)> m_dataReceivedCallback;
    std::function<void(const std::string&)> m_errorCallback;
    std::function<void()> m_connectionClosedCallback;
    std::mutex m_callbackMutex;

    // SSL/TLS state
    std::string m_hostname;
    std::vector<uint8_t> m_clientRandom;
    std::vector<uint8_t> m_serverRandom;
    std::vector<uint8_t> m_masterSecret;
    std::vector<uint8_t> m_serverCertificate;
    uint16_t m_selectedCipherSuite;

    // Encryption keys
    std::vector<uint8_t> m_clientWriteKey;
    std::vector<uint8_t> m_serverWriteKey;
    std::vector<uint8_t> m_clientWriteIV;
    std::vector<uint8_t> m_serverWriteIV;

    // Sequence numbers for encryption
    uint64_t m_clientSeqNum;
    uint64_t m_serverSeqNum;

    // Buffer for received data
    std::vector<uint8_t> m_receiveBuffer;
};

} // namespace dht_hunter::network
