#include <gtest/gtest.h>
#include "dht_hunter/logforge/formatter.hpp"

namespace lf = dht_hunter::logforge;

class FormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test basic formatting with no placeholders
TEST_F(FormatterTest, NoPlaceholders) {
    std::string result = lf::Formatter::format("Hello, World!");
    EXPECT_EQ(result, "Hello, World!");
}

// Test basic formatting with one placeholder
TEST_F(FormatterTest, SinglePlaceholder) {
    std::string result = lf::Formatter::format("Hello, {}!", "World");
    EXPECT_EQ(result, "Hello, World!");
}

// Test formatting with multiple placeholders
TEST_F(FormatterTest, MultiplePlaceholders) {
    std::string result = lf::Formatter::format("Hello, {}! Today is {} and the time is {}.", 
                                             "World", "Monday", "10:00 AM");
    EXPECT_EQ(result, "Hello, World! Today is Monday and the time is 10:00 AM.");
}

// Test formatting with numeric values
TEST_F(FormatterTest, NumericValues) {
    std::string result = lf::Formatter::format("The answer is {}", 42);
    EXPECT_EQ(result, "The answer is 42");
    
    result = lf::Formatter::format("Pi is approximately {}", 3.14159);
    EXPECT_EQ(result, "Pi is approximately 3.14159");
}

// Test formatting with positional arguments
TEST_F(FormatterTest, PositionalArguments) {
    std::string result = lf::Formatter::format("The order is {1}, {0}, {2}", "B", "A", "C");
    EXPECT_EQ(result, "The order is A, B, C");
}

// Test formatting with escaped braces
TEST_F(FormatterTest, EscapedBraces) {
    std::string result = lf::Formatter::format("Escaped {{braces}} and a {} placeholder", "normal");
    EXPECT_EQ(result, "Escaped {braces} and a normal placeholder");
}

// Test formatting with mixed types
TEST_F(FormatterTest, MixedTypes) {
    std::string result = lf::Formatter::format("String: {}, Integer: {}, Float: {}, Bool: {}", 
                                             "test", 42, 3.14, true);
    EXPECT_EQ(result, "String: test, Integer: 42, Float: 3.14, Bool: 1");
}

// Define a custom type for testing
struct Point {
    int x, y;
};

// Define the stream operator outside the test
std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << "(" << p.x << ", " << p.y << ")";
}

// Test formatting with custom objects
TEST_F(FormatterTest, CustomObjects) {
    Point p{10, 20};
    std::string result = lf::Formatter::format("The point is at {}", p);
    EXPECT_EQ(result, "The point is at (10, 20)");
}

// Test error cases
TEST_F(FormatterTest, ErrorCases) {
    // Unclosed brace
    std::string result = lf::Formatter::format("Unclosed brace {", "test");
    EXPECT_EQ(result, "Unclosed brace {");
    
    // Too few arguments
    result = lf::Formatter::format("Too {} few {} arguments", "many");
    EXPECT_EQ(result, "Too many few  arguments");
    
    // Too many arguments
    result = lf::Formatter::format("Too {} many arguments", "few", "extra", "arguments");
    EXPECT_EQ(result, "Too few many arguments");
}
