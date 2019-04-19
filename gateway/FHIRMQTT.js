//dashboard available at http://www.hivemq.com/demos/websocket-client/
var mqtt    = require('mqtt');
var client  = mqtt.connect('mqtt://broker.mqttdashboard.com');
const readline = require('readline');
var fs = require('fs');

const r1 = readline.createInterface({
	input: process.stdin,
	output: process.stdout
});

console.log("Starting Client");
console.log("Client Started");

client.on('connect', connectCallback); //when a 'connect' event is received call the connectCallback listener function
client.on('message', messageCallback);
r1.on('line', lineCallback);

function lineCallback(line)
{
	if(line == "john")
	{
		client.publish('/topic/conor0',"john's code",publishCallback);
		var content = fs.readFileSync('../FHIR_examples/glucose.json');
	}
	else if(line == "alice")
	{
		client.publish('/topic/conor0',"alice's code",publishCallback);
		var content = fs.readFileSync('../FHIR_examples/co2.json');
	
	}
	else
	{
		client.publish('/topic/conor0',"bob's code",publishCallback);
		var content = fs.readFileSync('../FHIR_examples/excess.json');
	}
	var jsonContent = JSON.parse(content);
	delete jsonContent.text;
	console.log(jsonContent);
	var newJSON = JSON.stringify(jsonContent);
	client.publish('/topic/conor0',newJSON,publishCallback);
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
