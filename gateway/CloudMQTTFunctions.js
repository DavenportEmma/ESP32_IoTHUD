var mqtt = require('mqtt');
var options = {
	port: 17785,
	host: 'mqtt://m24.cloudmqtt.com',
	//clientId: 'rpi',
	username: 'ovjzlenw',
	password: 'KsQSe4xmunaE',
	//keepalive: 60,
	//protocolId: 'MQIsdp',
	//protocolVersion: 3,
	//clean: true,
	//encoding: 'utf8'
};

var client = mqtt.connect('mqtt://m24.cloudmqtt.com', options);

console.log("Starting Client");
console.log("Client Started");

client.on('connect', connectCallback);
client.on('message', messageCallback);

function connectCallback()
{
	client.subscribe('/topic/qos1');
	client.subscribe('/topic/qos0');
	client.publish('/topic/test','hello world',publishCallback);
}

function publishCallback(error)
{
	if(error)
	{
		console.log('error');
	}
	else
	{
		console.log('message published');
	}
}

function messageCallback(topic, message, packet)
{
	console.log(message.toString());
}
