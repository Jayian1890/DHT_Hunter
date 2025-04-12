#include <iostream>

int main() {
    std::cout << "Color Test Program" << std::endl;
    std::cout << "=================" << std::endl;
    
    // ANSI color codes
    const char* reset = "\033[0m";
    const char* black = "\033[30m";
    const char* red = "\033[31m";
    const char* green = "\033[32m";
    const char* yellow = "\033[33m";
    const char* blue = "\033[34m";
    const char* magenta = "\033[35m";
    const char* cyan = "\033[36m";
    const char* white = "\033[37m";
    const char* brightRed = "\033[1;31m";
    
    // Test basic colors
    std::cout << black << "This is BLACK text" << reset << std::endl;
    std::cout << red << "This is RED text" << reset << std::endl;
    std::cout << green << "This is GREEN text" << reset << std::endl;
    std::cout << yellow << "This is YELLOW text" << reset << std::endl;
    std::cout << blue << "This is BLUE text" << reset << std::endl;
    std::cout << magenta << "This is MAGENTA text" << reset << std::endl;
    std::cout << cyan << "This is CYAN text" << reset << std::endl;
    std::cout << white << "This is WHITE text" << reset << std::endl;
    std::cout << brightRed << "This is BRIGHT RED text" << reset << std::endl;
    
    // Test log level colors (same as in our logger)
    std::cout << "\nLog Level Colors:" << std::endl;
    std::cout << "\033[90m" << "[TRACE] This is a trace message" << reset << std::endl;
    std::cout << "\033[36m" << "[DEBUG] This is a debug message" << reset << std::endl;
    std::cout << "\033[32m" << "[INFO] This is an info message" << reset << std::endl;
    std::cout << "\033[33m" << "[WARNING] This is a warning message" << reset << std::endl;
    std::cout << "\033[31m" << "[ERROR] This is an error message" << reset << std::endl;
    std::cout << "\033[1;31m" << "[CRITICAL] This is a critical message" << reset << std::endl;
    
    return 0;
}
