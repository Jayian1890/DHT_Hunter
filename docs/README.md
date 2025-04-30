# DHT-Hunter Web UI Documentation

## Overview

This directory contains comprehensive documentation for the DHT-Hunter Web UI, a modern web-based interface for monitoring and interacting with the DHT-Hunter application.

## Documentation Files

- [Web UI User Guide](web_ui_user_guide.md): Instructions for end users on how to use the web interface
- [Web UI Architecture](web_ui_architecture.md): Technical overview of the web UI architecture and components
- [Web UI Development Guide](web_ui_development_guide.md): Guide for developers who want to modify or extend the web UI
- [Web UI API Reference](web_ui_api_reference.md): Reference documentation for the web UI API endpoints
- [Web UI Recommendations](web_ui_recommendations.md): Recommendations for future improvements and best practices

## Getting Started

If you're new to the DHT-Hunter Web UI, start with the [Web UI User Guide](web_ui_user_guide.md) to learn how to access and use the interface.

For developers who want to understand the technical details, the [Web UI Architecture](web_ui_architecture.md) document provides a comprehensive overview of the system design.

## Features

The DHT-Hunter Web UI provides:

- Real-time monitoring of DHT network activity
- Visualization of nodes, peers, and info hashes
- Interactive charts and tables
- API endpoints for data access
- Responsive design for desktop and mobile devices

## Requirements

- Modern web browser (Chrome, Firefox, Edge, or Safari recommended)
- DHT-Hunter application running with web server enabled

## Configuration

The web server can be configured using the following command line options:

- `--web-port` or `-p`: Specify the port for the web server (default: 8080)
- `--web-root` or `-w`: Specify the directory containing web UI files (default: "web")

Example:
```
./dht_hunter --web-port 8081 --web-root /path/to/custom/web/files
```

## Contributing

If you want to contribute to the DHT-Hunter Web UI, please refer to the [Web UI Development Guide](web_ui_development_guide.md) for information on the development workflow and best practices.

## License

The DHT-Hunter Web UI is released under the same license as the DHT-Hunter application.
