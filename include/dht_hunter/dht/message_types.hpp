#pragma once

#include <string>
#include <memory>
#include <vector>
#include "../types/node_id.hpp"
#include "../network/endpoint.hpp"

namespace dht_hunter {
namespace dht {

// Node class
class Node {
public:
    Node(const types::NodeID& id, const network::EndPoint& endpoint)
        : m_id(id), m_endpoint(endpoint) {}
    
    const types::NodeID& getID() const { return m_id; }
    const network::EndPoint& getEndpoint() const { return m_endpoint; }
    
private:
    types::NodeID m_id;
    network::EndPoint m_endpoint;
};

// Message types
enum class MessageType {
    Query,
    Response,
    Error
};

// Base message class
class Message {
public:
    Message(MessageType type, const types::NodeID& sender)
        : m_type(type), m_sender(sender) {}
    
    MessageType getType() const { return m_type; }
    const types::NodeID& getSender() const { return m_sender; }
    
private:
    MessageType m_type;
    types::NodeID m_sender;
};

// Query message class
class QueryMessage : public Message {
public:
    QueryMessage(const types::NodeID& sender, const std::string& query)
        : Message(MessageType::Query, sender), m_query(query) {}
    
    const std::string& getQuery() const { return m_query; }
    
private:
    std::string m_query;
};

// Response message class
class ResponseMessage : public Message {
public:
    ResponseMessage(const types::NodeID& sender, const std::string& response)
        : Message(MessageType::Response, sender), m_response(response) {}
    
    const std::string& getResponse() const { return m_response; }
    
private:
    std::string m_response;
};

} // namespace dht
} // namespace dht_hunter
