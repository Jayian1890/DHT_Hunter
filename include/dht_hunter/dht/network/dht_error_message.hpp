#pragma once

#include "dht_hunter/dht/network/dht_message.hpp"
#include <string>

namespace dht_hunter::dht {

/**
 * @brief DHT error message
 */
class ErrorMessage : public DHTMessage {
public:
    /**
     * @brief Constructs an error message
     * @param transactionID The transaction ID
     * @param code The error code
     * @param message The error message
     */
    ErrorMessage(const std::string& transactionID = "", int code = 0, const std::string& message = "");

    /**
     * @brief Gets the message type
     * @return The message type (always Error)
     */
    MessageType getType() const override;

    /**
     * @brief Gets the error code
     * @return The error code
     */
    int getCode() const;

    /**
     * @brief Sets the error code
     * @param code The error code
     */
    void setCode(int code);

    /**
     * @brief Gets the error message
     * @return The error message
     */
    const std::string& getMessage() const;

    /**
     * @brief Sets the error message
     * @param message The error message
     */
    void setMessage(const std::string& message);

    /**
     * @brief Encodes the error message to a bencode dictionary
     * @return The encoded error message
     */
    bencode::BencodeDict encode() const override;

    /**
     * @brief Decodes an error message from a bencode dictionary
     * @param dict The bencode dictionary
     * @return The decoded error message, or nullptr if decoding failed
     */
    static std::shared_ptr<ErrorMessage> decode(const bencode::BencodeDict& dict);

    /**
     * @brief Gets the node ID of the sender
     * @return The node ID of the sender, if available
     */
    std::optional<NodeID> getNodeID() const override {
        return m_nodeID;
    }

    /**
     * @brief Sets the node ID of the sender
     * @param nodeID The node ID of the sender
     */
    void setNodeID(const NodeID& nodeID) {
        m_nodeID = nodeID;
    }

private:
    int m_code;
    std::string m_message;
    std::optional<NodeID> m_nodeID;
};

} // namespace dht_hunter::dht
