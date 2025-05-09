#include "event_system/event_bus.hpp"
#include "event_system/common_events.hpp"
#include "network/udp_socket.hpp"
#include "utils/async_utils.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <csignal>
#include <atomic>

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running.store(false);
}

int main() {
    // Register signal handlers
    std::signal(SIGINT, signalHandler);  // Ctrl+C
    std::signal(SIGTERM, signalHandler); // Termination request
    
    // Get the event bus
    auto eventBus = event_system::getEventBus();
    
    // Subscribe to network events
    int networkSubscription = eventBus->subscribe(
        event_system::EventType::NETWORK,
        [](event_system::EventPtr event) {
            auto networkEvent = std::dynamic_pointer_cast<event_system::NetworkEvent>(event);
            if (networkEvent) {
                std::cout << "Network event: " << networkEvent->getName() << std::endl;
                
                // Handle specific network events
                switch (networkEvent->getNetworkEventType()) {
                    case event_system::NetworkEvent::NetworkEventType::CONNECTED:
                        std::cout << "Connected to: " << *networkEvent->getProperty<std::string>("address") << std::endl;
                        break;
                    case event_system::NetworkEvent::NetworkEventType::DISCONNECTED:
                        std::cout << "Disconnected from: " << *networkEvent->getProperty<std::string>("address") << std::endl;
                        break;
                    case event_system::NetworkEvent::NetworkEventType::DATA_RECEIVED: {
                        auto address = networkEvent->getProperty<std::string>("address");
                        auto size = networkEvent->getProperty<size_t>("size");
                        if (address && size) {
                            std::cout << "Received " << *size << " bytes from " << *address << std::endl;
                        }
                        break;
                    }
                    case event_system::NetworkEvent::NetworkEventType::DATA_SENT: {
                        auto address = networkEvent->getProperty<std::string>("address");
                        auto size = networkEvent->getProperty<size_t>("size");
                        if (address && size) {
                            std::cout << "Sent " << *size << " bytes to " << *address << std::endl;
                        }
                        break;
                    }
                    case event_system::NetworkEvent::NetworkEventType::CONNECTION_ERROR:
                        std::cout << "Connection error" << std::endl;
                        break;
                }
            }
        }
    );
    
    // Create a UDP socket
    network::UDPSocketConfig socketConfig;
    socketConfig.port = 12345;
    socketConfig.reuseAddress = true;
    
    network::UDPSocket socket(socketConfig);
    
    // Open the socket asynchronously
    auto openFuture = socket.open();
    
    // Wait for the socket to open
    if (openFuture.get()) {
        std::cout << "Socket opened successfully" << std::endl;
        
        // Get the local address
        auto localAddress = socket.getLocalAddress();
        std::cout << "Local address: " << localAddress.toString() << std::endl;
        
        // Create a periodic task to send a message
        auto periodicTask = utils::executePeriodicAsync(
            std::chrono::seconds(1),
            [&socket]() {
                // Create a message
                std::string message = "Hello, world! Time: " + 
                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                std::vector<uint8_t> data(message.begin(), message.end());
                network::NetworkMessage networkMessage(data);
                
                // Send the message to localhost
                network::NetworkAddress destination("127.0.0.1", 12345);
                socket.send(destination, networkMessage);
            }
        );
        
        // Main loop
        std::cout << "Running... Press Ctrl+C to exit" << std::endl;
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Stop the periodic task
        periodicTask.wait();
        
        // Close the socket
        socket.close().wait();
    } else {
        std::cerr << "Failed to open socket" << std::endl;
    }
    
    // Unsubscribe from network events
    eventBus->unsubscribe(networkSubscription);
    
    std::cout << "Exiting..." << std::endl;
    return 0;
}
