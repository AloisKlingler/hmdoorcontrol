# hmdoorcontrol
A small ESP8266 based project to IOT-enable a Hoermann door

Features:
 * detection of a defined parking position of a car based on values of two ultrasonic sensors
 * blink / light signals with a RGB LED strip to park the car in a defined range
 * detect the open / closed state of the door based on reading hoermann door drive light barrier voltage; push the doorstate periodically to a webserver
 * possibility to remotely open / close the door with a reed relay attached to the push putton terminals of hoermann door drive - password protected
 
Prerequisites:
 * Hoermann Door Drive Supramatic
 * Hoermann Light Barriers
 * ESP8266 (e.g. NodeMCU, Wemos) with two Ultrasonic Sensors, Voltage Sensor, LED Strip with Driver, Reed Relay
 * PHP5 enabled webserver for receiving doorstate and remote open / close the door
