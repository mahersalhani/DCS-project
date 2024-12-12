const WebSocket = require("ws");

// Create a WebSocket server listening on port 3002
const wss = new WebSocket.Server({ port: 3002 });

wss.on("connection", (ws) => {
  console.log("New client connected");

  // When a message is received from a client, broadcast it to all other clients
  ws.on("message", (message) => {
    console.log("received: %s", message);

    // Broadcast the message to all other clients
    wss.clients.forEach((client) => {
      if (client !== ws && client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    });
  });

  // Handle client disconnection
  ws.on("close", () => {
    console.log("Client disconnected");
  });
});

console.log("WebSocket server running on ws://localhost:3002");
