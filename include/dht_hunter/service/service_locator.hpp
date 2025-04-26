#pragma once

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <typeinfo>

namespace dht_hunter::service {

/**
 * @brief A simple service locator that allows components to access services without direct dependencies
 */
class ServiceLocator {
public:
    /**
     * @brief Gets the singleton instance of the service locator
     * @return The singleton instance
     */
    static ServiceLocator& getInstance() {
        static ServiceLocator instance;
        return instance;
    }

    /**
     * @brief Registers a service
     * @tparam T The service type
     * @param service The service instance
     */
    template<typename T>
    void registerService(std::shared_ptr<T> service) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_services[std::type_index(typeid(T))] = service;
    }

    /**
     * @brief Gets a service
     * @tparam T The service type
     * @return The service instance, or nullptr if not found
     */
    template<typename T>
    std::shared_ptr<T> getService() {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_services.find(std::type_index(typeid(T)));
        if (it != m_services.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    /**
     * @brief Unregisters a service
     * @tparam T The service type
     */
    template<typename T>
    void unregisterService() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_services.erase(std::type_index(typeid(T)));
    }

private:
    ServiceLocator() = default;
    ~ServiceLocator() = default;
    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;

    std::mutex m_mutex;
    std::map<std::type_index, std::shared_ptr<void>> m_services;
};

} // namespace dht_hunter::service
