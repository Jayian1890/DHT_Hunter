# DHT-Hunter Web UI Development Guide

## Introduction

This guide is intended for developers who want to modify or extend the DHT-Hunter Web UI. It covers the structure of the codebase, development workflow, and best practices.

## Codebase Structure

The Web UI codebase is organized as follows:

### Backend Components

- `include/dht_hunter/network/http_server.hpp`: HTTP server interface
- `src/network/http_server.cpp`: HTTP server implementation
- `include/dht_hunter/web/web_server.hpp`: Web server component interface
- `src/web/web_server.cpp`: Web server component implementation
- `include/dht_hunter/utility/json/json.hpp`: JSON utility interface
- `src/utility/json/json.cpp`: JSON utility implementation

### Frontend Components

- `web/index.html`: Main HTML structure
- `web/css/styles.css`: CSS styling
- `web/js/dashboard.js`: JavaScript for the dashboard

## Development Workflow

### Setting Up the Development Environment

1. Clone the DHT-Hunter repository
2. Build the project using CMake
3. Make sure the web files are accessible to the application

### Making Changes to the Backend

1. Modify the C++ code in the appropriate files
2. Rebuild the project using CMake
3. Run the application with the `--web-root` option pointing to your web files

### Making Changes to the Frontend

1. Modify the HTML, CSS, or JavaScript files
2. No rebuild is necessary for frontend changes
3. Refresh the browser to see your changes

### Testing Your Changes

1. Run the DHT-Hunter application
2. Access the Web UI in your browser
3. Test all affected functionality
4. Check browser console for any JavaScript errors

## Adding New Features

### Adding a New API Endpoint

1. Define the endpoint handler in `web_server.hpp`
2. Implement the handler in `web_server.cpp`
3. Register the route in the `registerApiRoutes()` method
4. Update the frontend to use the new endpoint

Example:
```cpp
// In web_server.hpp
network::HttpResponse handleNewFeatureRequest(const network::HttpRequest& request);

// In web_server.cpp
void WebServer::registerApiRoutes() {
    // Existing routes...
    
    // New route
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/new-feature",
        [this](const network::HttpRequest& request) {
            return handleNewFeatureRequest(request);
        }
    );
}

network::HttpResponse WebServer::handleNewFeatureRequest(const network::HttpRequest& request) {
    network::HttpResponse response;
    response.statusCode = 200;
    response.setJsonContentType();
    
    auto dataObj = Json::createObject();
    dataObj->set("key", "value");
    
    response.body = Json::stringify(JsonValue(dataObj));
    return response;
}
```

### Adding a New UI Component

1. Update the HTML structure in `index.html`
2. Add CSS styles in `styles.css`
3. Implement JavaScript functionality in `dashboard.js`

Example:
```html
<!-- In index.html -->
<div class="new-component">
    <h3>New Component</h3>
    <div id="new-component-content"></div>
</div>
```

```css
/* In styles.css */
.new-component {
    background-color: var(--card-background);
    border-radius: 8px;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.05);
    padding: 20px;
}
```

```javascript
// In dashboard.js
function updateNewComponent() {
    fetch('/api/new-feature')
        .then(response => response.json())
        .then(data => {
            document.getElementById('new-component-content').textContent = data.key;
        })
        .catch(error => {
            console.error('Error fetching new feature data:', error);
        });
}

// Call this function periodically or when needed
updateNewComponent();
```

## Best Practices

### Backend Development

1. **Error Handling**: Always include proper error handling in API endpoints
2. **Input Validation**: Validate all input parameters
3. **Resource Management**: Ensure resources are properly cleaned up
4. **Thread Safety**: Use appropriate synchronization for shared resources
5. **Logging**: Include appropriate logging for debugging

### Frontend Development

1. **Responsive Design**: Ensure UI works on different screen sizes
2. **Error Handling**: Handle API errors gracefully
3. **Performance**: Minimize DOM manipulations and optimize rendering
4. **Accessibility**: Follow web accessibility guidelines
5. **Browser Compatibility**: Test on multiple browsers

### API Design

1. **Consistency**: Use consistent naming and response formats
2. **Versioning**: Consider versioning APIs for future compatibility
3. **Documentation**: Document all API endpoints
4. **Pagination**: Implement pagination for large data sets
5. **Filtering**: Allow filtering of data where appropriate

## Performance Considerations

### Backend Performance

1. **Caching**: Cache frequently accessed data
2. **Connection Pooling**: Reuse connections where possible
3. **Asynchronous Processing**: Use non-blocking I/O for long operations
4. **Resource Limits**: Implement limits on resource usage

### Frontend Performance

1. **Minimize HTTP Requests**: Combine and minify resources
2. **Lazy Loading**: Load data only when needed
3. **Efficient DOM Updates**: Minimize DOM manipulations
4. **Debouncing**: Debounce frequent events like resize or scroll
5. **Memory Management**: Clean up event listeners and intervals

## Security Considerations

1. **Input Sanitization**: Sanitize all user input
2. **Output Encoding**: Encode output to prevent XSS
3. **CORS**: Implement proper CORS headers
4. **Authentication**: Consider adding authentication for production use
5. **Content Security Policy**: Implement CSP headers

## Conclusion

By following this development guide, you should be able to effectively modify and extend the DHT-Hunter Web UI. Remember to follow best practices for web development and ensure your changes are well-tested before deployment.
