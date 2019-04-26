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

/*
	list of fhir examples that will work seemlessly with hud
	co2
	erythrocyte
	excess
	glucose
	hemoglobin
*/
function lineCallback(line)
{
	var filePath = '../FHIR_examples/' + line + '.json';
	var content = fs.readFileSync(filePath);
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
	//client.publish('/topic/conor0',message,publishCallback);
	console.log(message);
	
}

function publishCallback()
{
	console.log("Published Message");
}
