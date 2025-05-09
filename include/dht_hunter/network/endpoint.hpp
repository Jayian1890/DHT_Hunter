#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace dht_hunter {
namespace network {

// Network address class
class NetworkAddress {
public:
    NetworkAddress(const std::string& address)
        : m_address(address) {}
    
    const std::string& getAddress() const { return m_address; }
    
private:
    std::string m_address;
};

// Endpoint class
class EndPoint {
public:
    EndPoint(const NetworkAddress& address, uint16_t port)
        : m_address(address), m_port(port) {}
    
    const NetworkAddress& getAddress() const { return m_address; }
    uint16_t getPort() const { return m_port; }
    
    std::string toString() const {
        return m_address.getAddress() + ":" + std::to_string(m_port);
    }
    
private:
    NetworkAddress m_address;
    uint16_t m_port;
};

} // namespace network
} // namespace dht_hunter
