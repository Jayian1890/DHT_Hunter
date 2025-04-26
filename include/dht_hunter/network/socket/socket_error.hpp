#pragma once

#include <string>

namespace dht_hunter::network {

/**
 * @enum SocketError
 * @brief Defines the possible socket errors.
 */
enum class SocketError {
    None,
    InvalidSocket,
    AddressInUse,
    ConnectionRefused,
    NetworkUnreachable,
    HostUnreachable,
    ConnectionReset,
    ConnectionAborted,
    ConnectionTimedOut,
    NotConnected,
    AlreadyConnected,
    WouldBlock,
    InProgress,
    Interrupted,
    InvalidArgument,
    NoMemory,
    AccessDenied,
    NotSupported,
    NotInitialized,
    AddressNotAvailable,
    NetworkDown,
    NetworkReset,
    NoBufferSpace,
    ProtocolNotSupported,
    OperationNotSupported,
    AddressFamilyNotSupported,
    SystemLimitReached,
    Unknown
};

/**
 * @brief Converts a socket error to a string.
 * @param error The socket error.
 * @return The string representation of the error.
 */
std::string socketErrorToString(SocketError error);

/**
 * @brief Gets the last socket error.
 * @return The last socket error.
 */
SocketError getLastSocketError();

/**
 * @brief Gets the error message for the last socket error.
 * @return The error message.
 */
std::string getLastSocketErrorMessage();

} // namespace dht_hunter::network
