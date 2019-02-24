//dashboard available at http://www.hivemq.com/demos/websocket-client/
var mqtt    = require('mqtt');
var client  = mqtt.connect('mqtt://broker.mqttdashboard.com');

console.log("Starting Client");
console.log("Client Started");

client.on('connect', connectCallback); //when a 'connect' event is received call the connectCallback listener function
client.on('message', messageCallback);

function connectCallback() {
  	// publish a message to a topic, topic1/test
  	//client.subscribe('topic1/test');
  	client.subscribe('/topic/qos1');
  	client.subscribe('/topic/qos0');
}

function messageCallback(topic, message, packet)
{
	console.log(message.toString());
}

