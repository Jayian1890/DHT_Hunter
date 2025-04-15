#pragma once

#include <string>
#include <sstream>
#include <stdexcept>
#include <tuple>

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
    // Format with tuple and index sequence
    template<typename Tuple, size_t... Is>
    static std::string formatWithTuple(const std::string& format, const Tuple& args, std::index_sequence<Is...>) {
        std::string result;
        result.reserve(format.size() * 2); // Reserve some space to avoid reallocations

        size_t nextPos = 0;
        size_t defaultArgIndex = 0;

        while (nextPos < format.size()) {
            // Find the next opening brace
            const size_t openBrace = format.find('{', nextPos);

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
            const size_t closeBrace = format.find('}', openBrace);
            if (closeBrace == std::string::npos) {
                // Unclosed brace, treat as literal
                result.push_back('{');
                nextPos = openBrace + 1;
                continue;
            }

            // Check if there's a positional index inside the braces
            std::string indexStr = format.substr(openBrace + 1, closeBrace - openBrace - 1);
            size_t argIndex = defaultArgIndex++;

            // Check if the string is a valid integer (inlined from isInteger function)
            if (!indexStr.empty() && indexStr.find_first_not_of("0123456789") == std::string::npos) {
                argIndex = std::stoul(indexStr);
                // Don't increment defaultArgIndex for explicit indices
            }

            // Append the argument if the index is valid
            if (argIndex < sizeof...(Is)) {
                appendArg(result, args, argIndex, std::index_sequence<Is...>());
            } else {
                // Skip this placeholder if we don't have enough arguments
                // Don't add anything to the result
            }

            // Move past the closing brace
            nextPos = closeBrace + 1;
        }

        // Handle any escaped braces in the result (inlined from handleEscapedBraces function)
        std::string finalResult;
        finalResult.reserve(result.size());

        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == '{' && i + 1 < result.size() && result[i + 1] == '{') {
                finalResult.push_back('{');
                ++i; // Skip the second '{'
            } else if (result[i] == '}' && i + 1 < result.size() && result[i + 1] == '}') {
                finalResult.push_back('}');
                ++i; // Skip the second '}'
            } else {
                finalResult.push_back(result[i]);
            }
        }

        return finalResult;
    }

    // Helper to append an argument from a tuple at a specific index
    template<typename Tuple, size_t... Is>
    static void appendArg(std::string& result, const Tuple& args, size_t index, std::index_sequence<Is...>) {
        // Use fold expression to check each index
        (void)((index == Is && (appendArgImpl(result, std::get<Is>(args)), true)) || ...);
    }

    // Append implementation for different types
    template<typename T>
    static void appendArgImpl(std::string& result, const T& arg) {
        // Combined conditions for string types that can be directly appended
        if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                     std::is_same_v<std::decay_t<T>, const char*>) {
            result.append(arg);
        } else {
            // For all other types, use stream insertion
            std::ostringstream oss;
            oss << arg;
            result.append(oss.str());
        }
    }
};

} // namespace dht_hunter::logforge
