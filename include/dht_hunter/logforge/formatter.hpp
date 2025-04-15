#pragma once

#include <string>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <functional>
#include <type_traits>
#include <utility>

namespace dht_hunter::logforge {

/**
 * @class Formatter
 * @brief Provides string formatting capabilities with {} placeholders.
 *
 * This class implements a string formatter that supports {} placeholders
 * and positional arguments like {0}, {1}, etc.
 */
class Formatter {
public:
    // Delete constructor to indicate this class is not meant to be instantiated
    Formatter() = delete;
    /**
     * @brief Formats a string with the given arguments.
     * @param format The format string with {} placeholders.
     * @param args The arguments to format.
     * @return The formatted string.
     */
    template<typename... Args>
    static std::string format(const std::string& format, Args&&... args) {
        if constexpr (sizeof...(args) == 0) {
            return format;
        } else {
            // Store arguments in a tuple for random access
            auto argsTuple = std::forward_as_tuple(std::forward<Args>(args)...);
            return formatWithTuple(format, argsTuple, std::make_index_sequence<sizeof...(Args)>());
        }
    }

private:
    // Helper to check if a string is a valid integer
    static bool isInteger(const std::string& str) {
        if (str.empty()) return false;

        // Check for optional minus sign at the beginning
        size_t start = 0;
        if (str[0] == '-') {
            if (str.size() == 1) return false; // Just a minus sign is not a valid integer
            start = 1;
        }

        // Check that all remaining characters are digits
        return str.find_first_not_of("0123456789", start) == std::string::npos;
    }

    // Helper to handle escaped braces
    static std::string handleEscapedBraces(const std::string& format) {
        std::string result;
        result.reserve(format.size());

        // Use iterator-based approach to avoid index bounds checking
        for (auto it = format.begin(); it != format.end(); ++it) {
            if (*it == '{' && std::next(it) != format.end() && *std::next(it) == '{') {
                result.push_back('{');
                ++it; // Skip the second '{'
            } else if (*it == '}' && std::next(it) != format.end() && *std::next(it) == '}') {
                result.push_back('}');
                ++it; // Skip the second '}'
            } else {
                result.push_back(*it);
            }
        }

        return result;
    }

    // Process a single placeholder in the format string
    template<typename Tuple, size_t... Is>
    static void processPlaceholder(std::string& result, const std::string& format,
                                 size_t openBrace, size_t closeBrace,
                                 size_t& defaultArgIndex, const Tuple& args,
                                 std::index_sequence<Is...>) {
        // Check if there's a positional index inside the braces
        std::string indexStr = format.substr(openBrace + 1, closeBrace - openBrace - 1);
        size_t argIndex = defaultArgIndex++;

        // Handle explicit index if present
        if (!indexStr.empty() && isInteger(indexStr)) {
            try {
                argIndex = std::stoul(indexStr);
                // Don't increment defaultArgIndex for explicit indices
            } catch (const std::exception&) {
                // Invalid index format, treat as sequential
            }
        }

        // Append the argument if the index is valid
        if (argIndex < sizeof...(Is)) {
            appendArg(result, args, argIndex, std::index_sequence<Is...>());
        }
        // If index is invalid, don't add anything to the result
    }

    // Format with tuple and index sequence
    template<typename Tuple, size_t... Is>
    static std::string formatWithTuple(const std::string& format, const Tuple& args, std::index_sequence<Is...>) {
        std::string result;
        result.reserve(format.size() * 2); // Reserve some space to avoid reallocations

        size_t nextPos = 0;
        size_t defaultArgIndex = 0;

        while (nextPos < format.size()) {
            // Find the next opening brace
            size_t openBrace = format.find('{', nextPos);

            if (openBrace == std::string::npos) {
                // No more placeholders, append the rest and we're done
                result.append(format, nextPos, format.size() - nextPos);
                break;
            }

            // Append everything before the placeholder
            result.append(format, nextPos, openBrace - nextPos);

            // Check for escaped braces {{ which should be output as a single {
            if (openBrace + 1 < format.size() && format[openBrace + 1] == '{') {
                result.push_back('{');
                nextPos = openBrace + 2;
                continue;
            }

            // Find the closing brace
            size_t closeBrace = format.find('}', openBrace);
            if (closeBrace == std::string::npos) {
                // Unclosed brace, treat as literal
                result.push_back('{');
                nextPos = openBrace + 1;
                continue;
            }

            // Process this placeholder
            processPlaceholder(result, format, openBrace, closeBrace,
                             defaultArgIndex, args, std::index_sequence<Is...>());

            // Move past the closing brace
            nextPos = closeBrace + 1;
        }

        // Handle any escaped braces in the result
        return handleEscapedBraces(result);
    }

    // Helper to append an argument from a tuple at a specific index
    template<typename Tuple, size_t... Is>
    static void appendArg(std::string& result, const Tuple& args, size_t index, std::index_sequence<Is...>) {
        // Use a more straightforward approach to avoid side effects in short-circuit evaluation
        bool found = false;
        // Using a fold expression with comma operator to iterate through all indices
        ((found || (index == Is && (found = true, appendArgImpl(result, std::get<Is>(args)), true))), ...);
    }

    // Append implementation for string types
    template<typename T>
    static void appendArgImpl(std::string& result, const T& arg) requires
        std::is_same_v<std::decay_t<T>, std::string> ||
        std::is_same_v<std::decay_t<T>, const char*> {
        result.append(arg);
    }

    // Append implementation for all other types
    template<typename T>
    static void appendArgImpl(std::string& result, const T& arg) requires
        (!std::is_same_v<std::decay_t<T>, std::string> &&
         !std::is_same_v<std::decay_t<T>, const char*>) {
        std::ostringstream oss;
        oss << arg;
        result.append(oss.str());
    }
};

} // namespace dht_hunter::logforge
