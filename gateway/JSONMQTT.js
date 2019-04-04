//dashboard available at http://www.hivemq.com/demos/websocket-client/
var mqtt    = require('mqtt');
var client  = mqtt.connect('mqtt://broker.mqttdashboard.com');

console.log("Starting Client");
console.log("Client Started");

client.on('connect', connectCallback); //when a 'connect' event is received call the connectCallback listener function
client.on('message', messageCallback);

var msg = {
	name : "rpi",
	age : 10,
	admin : true
};

function connectCallback()
{
	console.log("Client Connected");
  	// publish a message to a topic, topic1/test
  	//client.subscribe('topic1/test');
  	client.subscribe('/topic/conor1',subscribeCallback);
  	//client.subscribe('/topic/qos0');
}

function subscribeCallback()
{
	console.log("Subscribed to Topic");
}

function messageCallback(topic, message, packet)
{
	console.log(message.toString());
	var JSONmsg = JSON.stringify(msg);
	client.publish('/topic/conor0',JSONmsg,publishCallback);
}

function publishCallback()
{
	console.log("Published Message");
}
