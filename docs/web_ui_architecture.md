# DHT-Hunter Web UI Architecture

## Overview

This document describes the architecture of the DHT-Hunter web UI, including its components, data flow, and integration with the main application.

## Architecture Components

The DHT-Hunter web UI consists of the following main components:

1. **HTTP Server**
   - Handles HTTP requests and responses
   - Serves static files (HTML, CSS, JavaScript)
   - Provides API endpoints for data access

2. **Web Server Component**
   - Integrates the HTTP server with the main application
   - Manages the lifecycle of the HTTP server
   - Provides data from the DHT components to the HTTP server

3. **Frontend UI**
   - HTML structure for the dashboard
   - CSS styling for responsive design
   - JavaScript for dynamic content and interactivity

4. **JSON Utility**
   - Custom JSON serialization and deserialization
   - Supports data exchange between backend and frontend

## Component Diagram

```
+----------------------------------+
|          DHT-Hunter App          |
|                                  |
|  +-------------+  +----------+   |
|  | DHT Core    |  | Web      |   |
|  | Components  |  | Server   |   |
|  +------+------+  +-----+----+   |
|         |               |        |
|         +-------+-------+        |
|                 |                |
|         +-------+-------+        |
|         | HTTP Server   |        |
|         +---------------+        |
+----------------------------------+
            |
            | HTTP
            |
+----------------------------------+
|          Web Browser             |
|                                  |
|  +-------------+  +----------+   |
|  | HTML/CSS    |  | Charts   |   |
|  | UI          |  |          |   |
|  +------+------+  +-----+----+   |
|         |               |        |
|         +-------+-------+        |
|                 |                |
|         +-------+-------+        |
|         | JavaScript    |        |
|         +---------------+        |
+----------------------------------+
```

## Data Flow

1. **DHT Data Collection**
   - DHT components collect data about nodes, peers, and info hashes
   - Statistics are aggregated by the StatisticsService

2. **Data Exposure**
   - The WebServer component accesses data from DHT components
   - Data is formatted as JSON using the custom JSON utility
   - API endpoints expose the data to the frontend

3. **Frontend Data Consumption**
   - JavaScript code fetches data from API endpoints
   - Data is processed and displayed in the UI
   - Charts and tables are updated with the latest data

4. **User Interaction**
   - User interacts with the UI (filtering, sorting, etc.)
   - JavaScript handles user interactions
   - API requests are made based on user actions

## HTTP Server Implementation

The HTTP server is implemented as a standalone component that:

1. Listens for incoming HTTP connections on a configurable port
2. Parses HTTP requests and extracts relevant information
3. Routes requests to appropriate handlers based on URL paths
4. Serves static files from the web directory
5. Provides API endpoints for dynamic data
6. Formats and sends HTTP responses

## API Endpoints

The web UI exposes the following API endpoints:

1. `/api/statistics`
   - Returns general statistics about the DHT network
   - Includes node counts, message counts, and peer counts

2. `/api/nodes`
   - Returns information about DHT nodes
   - Supports pagination through query parameters

3. `/api/infohashes`
   - Returns information about discovered info hashes
   - Includes peer counts for each info hash

4. `/api/uptime`
   - Returns the uptime of the DHT-Hunter application

## Frontend Implementation

The frontend is implemented using:

1. **HTML5** for structure
   - Semantic elements for better accessibility
   - Responsive layout using CSS Grid and Flexbox

2. **CSS3** for styling
   - Custom styling for all UI components
   - Responsive design for different screen sizes
   - CSS variables for consistent theming

3. **JavaScript** for interactivity
   - Fetch API for data retrieval
   - Chart.js for data visualization
   - DOM manipulation for dynamic content

## Integration with Main Application

The web UI is integrated with the main application through:

1. **WebServer Class**
   - Created and managed by the main application
   - Started and stopped with the main application lifecycle
   - Accesses DHT components through dependency injection

2. **Command Line Options**
   - Web server port is configurable via command line
   - Web root directory is configurable via command line

3. **Resource Management**
   - Web server resources are properly cleaned up on application shutdown
   - Error handling ensures the main application continues even if the web server fails

## Custom JSON Utility

The custom JSON utility provides:

1. **Serialization**
   - Converts C++ objects to JSON strings
   - Supports all basic JSON types (null, boolean, number, string, object, array)
   - Handles proper escaping and formatting

2. **Deserialization**
   - Parses JSON strings into C++ objects
   - Validates JSON syntax
   - Provides error handling for invalid JSON

3. **Type Safety**
   - Strong typing for JSON values
   - Type checking and conversion utilities
   - Exception handling for type mismatches

## Conclusion

The DHT-Hunter web UI architecture provides a clean separation of concerns between the backend DHT functionality and the frontend user interface. The modular design allows for easy maintenance and future enhancements while ensuring efficient data flow between components.
