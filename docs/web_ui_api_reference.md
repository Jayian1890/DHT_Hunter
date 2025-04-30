# DHT-Hunter Web UI API Reference

## Overview

This document provides a comprehensive reference for the DHT-Hunter Web UI API endpoints. These endpoints allow frontend components to retrieve data from the DHT-Hunter application.

## Base URL

All API endpoints are relative to the base URL of the DHT-Hunter Web UI, which is typically:

```
http://localhost:8080
```

The port may vary if configured differently using the `--web-port` command line option.

## API Endpoints

### Statistics API

#### Get Statistics

Retrieves general statistics about the DHT network.

- **URL**: `/api/statistics`
- **Method**: `GET`
- **Response Format**: JSON

**Response Example**:
```json
{
  "nodesDiscovered": 1250,
  "nodesAdded": 850,
  "nodesInTable": 320,
  "peersDiscovered": 4500,
  "messagesReceived": 12500,
  "messagesSent": 8700,
  "errors": 15,
  "infoHashes": 750
}
```

**Response Fields**:
- `nodesDiscovered`: Total number of DHT nodes discovered
- `nodesAdded`: Number of nodes added to the routing table
- `nodesInTable`: Current number of nodes in the routing table
- `peersDiscovered`: Total number of peers discovered
- `messagesReceived`: Total number of DHT messages received
- `messagesSent`: Total number of DHT messages sent
- `errors`: Total number of errors encountered
- `infoHashes`: Total number of unique info hashes discovered

### Nodes API

#### Get Nodes

Retrieves information about DHT nodes.

- **URL**: `/api/nodes`
- **Method**: `GET`
- **Query Parameters**:
  - `limit` (optional): Maximum number of nodes to return (default: 10)
- **Response Format**: JSON

**Response Example**:
```json
[
  {
    "nodeId": "04e8c836586b3c",
    "ip": "203.0.113.1",
    "port": 6881,
    "lastSeen": 1619712345678
  },
  {
    "nodeId": "a1b2c3d4e5f6",
    "ip": "198.51.100.2",
    "port": 6889,
    "lastSeen": 1619712340000
  }
]
```

**Response Fields**:
- `nodeId`: The ID of the DHT node
- `ip`: The IP address of the node
- `port`: The port number of the node
- `lastSeen`: Timestamp (in milliseconds since epoch) when the node was last seen

### Info Hashes API

#### Get Info Hashes

Retrieves information about discovered info hashes.

- **URL**: `/api/infohashes`
- **Method**: `GET`
- **Query Parameters**:
  - `limit` (optional): Maximum number of info hashes to return (default: 10)
- **Response Format**: JSON

**Response Example**:
```json
[
  {
    "hash": "03ed6f5cfd4d",
    "peers": 15,
    "firstSeen": 1619712345678
  },
  {
    "hash": "7a74a13a5d6f",
    "peers": 8,
    "firstSeen": 1619712340000
  }
]
```

**Response Fields**:
- `hash`: The info hash (usually shown in shortened form)
- `peers`: Number of peers associated with this info hash
- `firstSeen`: Timestamp (in milliseconds since epoch) when the info hash was first seen

### Uptime API

#### Get Uptime

Retrieves the uptime of the DHT-Hunter application.

- **URL**: `/api/uptime`
- **Method**: `GET`
- **Response Format**: JSON

**Response Example**:
```json
{
  "uptime": 3600
}
```

**Response Fields**:
- `uptime`: The uptime of the application in seconds

## Error Handling

All API endpoints return appropriate HTTP status codes:

- `200 OK`: The request was successful
- `400 Bad Request`: The request was invalid (e.g., invalid parameters)
- `404 Not Found`: The requested resource was not found
- `500 Internal Server Error`: An error occurred on the server

Error responses include a JSON body with an error message:

```json
{
  "error": "Invalid parameter: limit must be a positive integer"
}
```

## Rate Limiting

Currently, there is no rate limiting implemented for the API endpoints. However, clients should avoid making excessive requests to prevent performance issues.

## CORS Support

Cross-Origin Resource Sharing (CORS) is not currently implemented. The API is intended to be used by the DHT-Hunter Web UI running on the same origin.

## Authentication

The API does not currently implement authentication. Access control should be handled at the network level if needed.

## Future API Endpoints

The following endpoints may be implemented in future versions:

- `/api/config`: Get and set configuration options
- `/api/search`: Search for specific nodes or info hashes
- `/api/history`: Retrieve historical data for trend analysis
- `/api/actions`: Perform actions like initiating searches or lookups

## Conclusion

This API reference provides the information needed to interact with the DHT-Hunter application through its Web UI API. Developers can use these endpoints to build custom visualizations or integrations with the DHT-Hunter application.
