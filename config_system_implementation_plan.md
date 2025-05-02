# JSON Configuration System Implementation Plan

This document outlines the plan for implementing a comprehensive JSON configuration system for the DHT Hunter application. The system will allow end users to modify all application settings through a single JSON configuration file.

## Table of Contents
- [Overview](#overview)
- [Configuration Categories](#configuration-categories)
- [Implementation Tasks](#implementation-tasks)
- [Testing Plan](#testing-plan)
- [Documentation](#documentation)

## Overview

The configuration system will:
1. Use a single JSON file to store all application settings
2. Load settings at application startup
3. Override command-line arguments when specified in the config file
4. Provide sensible defaults for all settings
5. Include detailed comments in the default config file
6. Validate configuration values to ensure they are within acceptable ranges
7. Support hot-reloading of configuration changes (where applicable)

## Configuration Categories

Based on the codebase analysis, the following configuration categories have been identified:

1. **General Application Settings**
   - Configuration directory path
   - Log file settings
   - Node ID persistence

2. **DHT Core Settings**
   - DHT port
   - K-bucket size
   - Alpha parameter
   - Max results
   - Bootstrap nodes
   - Token rotation interval
   - Bucket refresh interval
   - Max iterations
   - Max queries

3. **Network Settings**
   - UDP server settings
   - Transaction timeout
   - Maximum transactions
   - MTU size

4. **Web Interface Settings**
   - Web root directory
   - Web server port
   - API settings

5. **Persistence Settings**
   - Save intervals
   - File paths
   - Data formats

6. **Crawler Settings**
   - Parallel crawls
   - Refresh interval
   - Max nodes
   - Max InfoHashes
   - Auto-start

7. **Metadata Acquisition Settings**
   - Processing interval
   - Max concurrent acquisitions
   - Acquisition timeout
   - Max retry count
   - Retry delay

8. **Event System Settings**
   - Logging settings
   - Component communication settings
   - Statistics settings
   - Asynchronous processing settings

## Implementation Tasks

### 1. Create Configuration Structure and Default File ✅

- [x] Define the JSON schema for the configuration file
- [x] Create a default configuration file with all settings and detailed comments
- [x] Implement validation rules for each configuration parameter

### 2. Implement Configuration Loading System ✅

- [x] Create a ConfigurationManager class to handle loading and parsing the config file
- [x] Implement error handling for malformed JSON
- [x] Add validation of configuration values
- [x] Create a singleton pattern for the ConfigurationManager

### 3. Integrate with Command Line Arguments ✅

- [x] Modify the command-line argument parsing to check for a config file path
- [x] Implement priority rules (config file vs. command line arguments)
- [x] Add a command-line option to generate a default config file

### 4. Update DHT Core Components ✅

- [x] Modify DHTConfig to load from the configuration system
- [x] Update the DHT node initialization to use the configuration
- [x] Implement configuration for bootstrap nodes

### 5. Update Network Components ✅

- [x] Modify UDPServer to use the configuration system
- [x] Update transaction manager to use configurable limits

### 6. Update Web Interface ✅

- [x] Modify WebServer to use the configuration system
- [x] Update API endpoints to reflect configuration changes

### 7. Update Persistence System ✅

- [x] Modify PersistenceManager to use the configuration system
- [x] Update save intervals and file paths based on configuration

### 8. Update Crawler Components ✅

- [x] Modify Crawler to use the configuration system
- [x] Update crawler parameters based on configuration

### 9. Update Metadata Acquisition System ✅

- [x] Modify MetadataAcquisitionManager to use the configuration system
- [x] Update acquisition parameters based on configuration

### 10. Update Event System ✅

- [x] Modify unified event system to use the configuration system
- [x] Update logging, component, and statistics processors

### 11. Implement Hot-Reloading (Optional) ⬜

- [ ] Add file watching for the configuration file
- [ ] Implement safe reloading of configuration values
- [ ] Add events for configuration changes

### 12. Add Configuration API Endpoints ⬜

- [ ] Create API endpoints to view current configuration
- [ ] Add endpoints to modify configuration (if applicable)

## Testing Plan

1. **Unit Tests**
   - Test configuration loading and parsing
   - Test validation of configuration values
   - Test error handling for malformed JSON

2. **Integration Tests**
   - Test interaction between configuration system and application components
   - Test command-line argument overrides

3. **End-to-End Tests**
   - Test application startup with various configuration files
   - Test hot-reloading of configuration (if implemented)

## Documentation ✅

1. **User Documentation** ✅
   - [x] Create detailed documentation for all configuration options
   - [x] Include examples for common configuration scenarios
   - [x] Document the default values and acceptable ranges

2. **Developer Documentation** ✅
   - [x] Document the configuration system architecture
   - [x] Provide guidelines for adding new configuration options
   - [x] Document the validation rules and error handling

---

## Progress Tracking

### 1. Create Configuration Structure and Default File
- [ ] Define the JSON schema for the configuration file
- [ ] Create a default configuration file with all settings and detailed comments
- [ ] Implement validation rules for each configuration parameter

### 2. Implement Configuration Loading System
- [ ] Create a ConfigurationManager class to handle loading and parsing the config file
- [ ] Implement error handling for malformed JSON
- [ ] Add validation of configuration values
- [ ] Create a singleton pattern for the ConfigurationManager

### 3. Integrate with Command Line Arguments
- [ ] Modify the command-line argument parsing to check for a config file path
- [ ] Implement priority rules (config file vs. command line arguments)
- [ ] Add a command-line option to generate a default config file

### 4. Update DHT Core Components
- [ ] Modify DHTConfig to load from the configuration system
- [ ] Update the DHT node initialization to use the configuration
- [ ] Implement configuration for bootstrap nodes

### 5. Update Network Components
- [ ] Modify UDPServer to use the configuration system
- [ ] Update transaction manager to use configurable limits

### 6. Update Web Interface
- [ ] Modify WebServer to use the configuration system
- [ ] Update API endpoints to reflect configuration changes

### 7. Update Persistence System
- [ ] Modify PersistenceManager to use the configuration system
- [ ] Update save intervals and file paths based on configuration

### 8. Update Crawler Components
- [ ] Modify Crawler to use the configuration system
- [ ] Update crawler parameters based on configuration

### 9. Update Metadata Acquisition System
- [ ] Modify MetadataAcquisitionManager to use the configuration system
- [ ] Update acquisition parameters based on configuration

### 10. Update Event System
- [ ] Modify unified event system to use the configuration system
- [ ] Update logging, component, and statistics processors

### 11. Implement Hot-Reloading (Optional)
- [ ] Add file watching for the configuration file
- [ ] Implement safe reloading of configuration values
- [ ] Add events for configuration changes

### 12. Add Configuration API Endpoints
- [ ] Create API endpoints to view current configuration
- [ ] Add endpoints to modify configuration (if applicable)
