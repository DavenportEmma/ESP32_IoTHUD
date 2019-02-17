//dashboard available at http://www.hivemq.com/demos/websocket-client/
var mqtt    = require('mqtt');
var client  = mqtt.connect('mqtt://broker.mqttdashboard.com');

console.log("Starting Client");
console.log("Client Started");

client.on('connect', connectCallback); //when a 'connect' event is received call the connectCallback listener function
          
function connectCallback() {
  // publish a message to a topic, topic1/test
  client.publish('topic1/test', 'message', publishCallback);
}

function publishCallback(error) {     
   	if (error) {
		console.log("error publishingr data");
	} else {	 
        console.log("Message is published");
        client.end(); // Close the connection when published
    }
}
