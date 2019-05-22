/*
Final Year Project - IoT HUD
Conor Davenport - C15444808
Last Modified: 22 May 2018
*/
//dashboard available at http://www.hivemq.com/demos/websocket-client/
var mqtt    = require('mqtt');	// mqtt library
var client  = mqtt.connect('mqtt://broker.mqttdashboard.com');	// connect to broker and return handle
const readline = require('readline');	// library for reading user input
var fs = require('fs');	// library for file reading

const r1 = readline.createInterface({	// create a user input interface
	input: process.stdin,	// readable stream to listen to
	output: process.stdout	// stream to write to
});

console.log("Starting Client");	// status messages for user
console.log("Client Started");

client.on('connect', connectCallback); 	// when an mqtt 'connect' event is received call the connectCallback listener function
client.on('message', messageCallback);	// messageCallback called on mqtt event 'message'
r1.on('line', lineCallback);	// lineCallback called when user inputs a string

/*
	list of fhir examples that will work seemlessly with hud
	co2
	erythrocyte
	excess
	glucose
	hemoglobin
*/

function lineCallback(line)	// called on user input
{
	// filepath to json fhir message, user inputted line is inserted into string
	var filePath = '../FHIR_examples/' + line + '.json';
	var content;	// variable to hold file contents
	// attempt to read file, return error if file doesn't exist
	try
	{
		content = fs.readFileSync(filePath);
	}
	catch(e)
	{
		return console.error(e);
	}

	var jsonContent;
	// attempt to parse json file, return error if parse failed
	try
	{
		jsonContent = JSON.parse(content);	
	}
	catch(e)
	{
		return console.error(e);
	}
	delete jsonContent.text;	// delete html content of fhir message to reduce size
	console.log(jsonContent);	// output file on terminal
	var newJSON = JSON.stringify(jsonContent);	// parse fhir message into new json object
	client.publish('/topic/conor0',newJSON,publishCallback);	// send to client
}

function connectCallback()	// on mqtt connect to client
{
	console.log("Client Connected");	// status output for user
  	client.subscribe('/topic/conor1',subscribeCallback);	// subscribe to topic for HUD to gateway comms
}

function subscribeCallback()	// called on topic subscription
{
	console.log("Subscribed to Topic");	// status output for user
}

function messageCallback(topic, message, packet)	// called when a message is received	
{
	console.log(message);	// print message to terminal
	
}

function publishCallback()	// called when message is published to topic
{
	console.log("Published Message");	// user feedback
}
