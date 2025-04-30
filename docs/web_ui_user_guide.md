# DHT-Hunter Web UI User Guide

## Introduction

The DHT-Hunter Web UI provides a graphical interface for monitoring and interacting with the DHT-Hunter application. This guide explains how to access and use the web interface.

## Accessing the Web UI

By default, the DHT-Hunter Web UI is available at `http://localhost:8080` when the application is running. You can access it using any modern web browser.

### Command Line Options

You can customize the web server using the following command line options:

- `--web-port` or `-p`: Specify the port for the web server (default: 8080)
- `--web-root` or `-w`: Specify the directory containing web UI files (default: "web")

Example:
```
./dht_hunter --web-port 8081 --web-root /path/to/custom/web/files
```

## Dashboard Overview

The DHT-Hunter Web UI dashboard provides a comprehensive view of the DHT network activity:

### Statistics Cards

At the top of the dashboard, you'll find statistics cards showing:

- **Nodes**: The number of discovered nodes and nodes in the routing table
- **Peers**: The number of discovered peers
- **Messages**: The number of messages received and sent
- **Info Hashes**: The number of discovered info hashes

### Charts

The dashboard includes two main charts:

1. **Network Activity**: Shows the message traffic over time
2. **Node Distribution**: Shows the distribution of nodes (discovered vs. in routing table)

### Data Tables

The dashboard includes tables showing:

1. **Recent Nodes**: The most recently seen DHT nodes
2. **Recent Info Hashes**: The most recently discovered info hashes

## Real-Time Updates

The dashboard automatically updates every second to show the latest data. You'll see:

- Updated statistics in the cards
- New data points in the charts
- Refreshed entries in the tables

## Status Indicator

In the top-right corner of the dashboard, you'll find a status indicator showing:

- **Green**: The DHT-Hunter application is active and running
- **Red**: There's an issue with the DHT-Hunter application

## Uptime Display

At the bottom of the dashboard, you'll see the current uptime of the DHT-Hunter application displayed in days, hours, minutes, and seconds.

## Browser Compatibility

The DHT-Hunter Web UI is compatible with all modern browsers, including:

- Google Chrome (recommended)
- Mozilla Firefox
- Microsoft Edge
- Safari

For the best experience, use the latest version of your preferred browser.

## Troubleshooting

### Web UI Not Loading

If the web UI doesn't load:

1. Verify that the DHT-Hunter application is running
2. Check that you're using the correct port in your browser URL
3. Look for any error messages in the DHT-Hunter console output
4. Try using a different port with the `--web-port` option

### Data Not Updating

If the data in the web UI isn't updating:

1. Check your internet connection
2. Verify that the DHT-Hunter application is still running
3. Try refreshing the page
4. Check the browser console for any JavaScript errors

### Browser Compatibility Issues

If you experience display issues:

1. Try using a different browser
2. Update your current browser to the latest version
3. Clear your browser cache and reload the page

## Conclusion

The DHT-Hunter Web UI provides a user-friendly way to monitor and interact with the DHT network. By following this guide, you should be able to effectively use all features of the web interface.
