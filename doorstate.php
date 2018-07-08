<?php
date_default_timezone_set("Europe/Vienna");
$system_time = time();

$doorstate = isset($_GET['state']) ? $_GET['state'] : '6';
$skipwrite = 0;

if ($doorstate == 1) {
	// open
	$timestamp = "Timestamp: " . $system_time . "\n";
	$doorstate = 'Garagentor: offen';
	echo "door open received";
} else if ($doorstate == 2) {
	// closed
	$timestamp = "Timestamp: " . $system_time . "\n";
	$doorstate = 'Garagentor: geschlossen';
	echo "door closed received";
} else if ($doorstate == 3) {
	// sensor error
	$timestamp = "Timestamp: " . $system_time-500 . "\n";
	$doorstate = 'Garagentor: Sensorfehler!';
	echo "sensor error received";
} else if ($doorstate == 4) {
	// doorstate unknown
	$timestamp = "Timestamp: " . $system_time-500 . "\n";
	$doorstate = 'Garagentor: Unbekannter Fehler!';
	echo "unknown error received";
} else if ($doorstate == 5) {
	// doorstate test
	$skipwrite = 1;
	echo "door TEST received";
	return;
} else {
	// unknown state!
	$timestamp = "Timestamp: " . ($system_time-500) . "\n";
	fwrite($fp, $timestamp);
	$doorstate = 'Garagentor: Fehlerhafter Aufruf!';
	echo "unknown state requested";
}
if ($skipwrite != 1) {
	$fp = fopen('/tmp/door', 'w');
	fwrite($fp, $timestamp);
	fwrite($fp, $doorstate);
	fclose($fp);
}
?>
