#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/network/network_address.hpp"
#include <string>
#include <memory>

namespace dht_hunter::unified_event {

/**
 * @brief Event for message reception
 */
class MessageReceivedEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param message The received message
     * @param sender The sender address
     */
    MessageReceivedEvent(const std::string& source, std::shared_ptr<dht_hunter::dht::Message> message, const dht_hunter::network::NetworkAddress& sender)
        : Event(EventType::MessageReceived, EventSeverity::Debug, source) {
        setProperty("message", message);
        setProperty("sender", sender);
    }
    
    /**
     * @brief Gets the received message
     * @return The received message
     */
    std::shared_ptr<dht_hunter::dht::Message> getMessage() const {
        auto message = getProperty<std::shared_ptr<dht_hunter::dht::Message>>("message");
        return message ? *message : nullptr;
    }
    
    /**
     * @brief Gets the sender address
     * @return The sender address
     */
    dht_hunter::network::NetworkAddress getSender() const {
        auto sender = getProperty<dht_hunter::network::NetworkAddress>("sender");
        return sender ? *sender : dht_hunter::network::NetworkAddress();
    }
};

/**
 * @brief Event for message sending
 */
class MessageSentEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param message The sent message
     * @param recipient The recipient address
     */
    MessageSentEvent(const std::string& source, std::shared_ptr<dht_hunter::dht::Message> message, const dht_hunter::network::NetworkAddress& recipient)
        : Event(EventType::MessageSent, EventSeverity::Debug, source) {
        setProperty("message", message);
        setProperty("recipient", recipient);
    }
    
    /**
     * @brief Gets the sent message
     * @return The sent message
     */
    std::shared_ptr<dht_hunter::dht::Message> getMessage() const {
        auto message = getProperty<std::shared_ptr<dht_hunter::dht::Message>>("message");
        return message ? *message : nullptr;
    }
    
    /**
     * @brief Gets the recipient address
     * @return The recipient address
     */
    dht_hunter::network::NetworkAddress getRecipient() const {
        auto recipient = getProperty<dht_hunter::network::NetworkAddress>("recipient");
        return recipient ? *recipient : dht_hunter::network::NetworkAddress();
    }
};

/**
 * @brief Event for message errors
 */
class MessageErrorEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param message The message that caused the error
     * @param errorMessage The error message
     */
    MessageErrorEvent(const std::string& source, std::shared_ptr<dht_hunter::dht::Message> message, const std::string& errorMessage)
        : Event(EventType::MessageError, EventSeverity::Error, source) {
        setProperty("message", message);
        setProperty("errorMessage", errorMessage);
    }
    
    /**
     * @brief Gets the message that caused the error
     * @return The message that caused the error
     */
    std::shared_ptr<dht_hunter::dht::Message> getMessage() const {
        auto message = getProperty<std::shared_ptr<dht_hunter::dht::Message>>("message");
        return message ? *message : nullptr;
    }
    
    /**
     * @brief Gets the error message
     * @return The error message
     */
    std::string getErrorMessage() const {
        auto errorMessage = getProperty<std::string>("errorMessage");
        return errorMessage ? *errorMessage : "";
    }
    
    /**
     * @brief Gets a string representation of the event
     * @return A string representation of the event
     */
    std::string toString() const override {
        std::string baseString = Event::toString();
        return baseString + ": " + getErrorMessage();
    }
};

} // namespace dht_hunter::unified_event
