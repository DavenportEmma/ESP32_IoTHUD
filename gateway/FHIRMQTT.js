//dashboard available at http://www.hivemq.com/demos/websocket-client/
var mqtt    = require('mqtt');
var client  = mqtt.connect('mqtt://broker.mqttdashboard.com');
const readline = require('readline');

console.log("Starting Client");
console.log("Client Started");

client.on('connect', connectCallback); //when a 'connect' event is received call the connectCallback listener function
client.on('message', messageCallback);

const r1 = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

r1.on('line', lineCallback);

function lineCallback(line)
{
    client.publish('/topic/conor0',line,publishCallback);
}

function connectCallback()
{
	console.log("Client Connected");
  	client.subscribe('/topic/conor1',subscribeCallback);
}

function subscribeCallback()
{
	console.log("Subscribed to Topic");
}

function messageCallback(topic, message, packet)
{
	client.publish('/topic/conor0',message,publishCallback);
}

function publishCallback()
{
	console.log("Published Message");
}
