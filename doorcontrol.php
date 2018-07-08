<?php

$nodemcu = '192.168.0.247';

if (isset($_GET['switchpp3']) && $_GET['switchpp3'] == 1) {
	// generate one time URI
	if (isset($_SERVER['REMOTE_ADDR']) && $_SERVER['REMOTE_ADDR'] != $nodemcu) return;
	$dooropenuri = bin2hex(openssl_random_pseudo_bytes(5));
	echo $dooropenuri;
	file_put_contents('dooropenuri.txt',$dooropenuri);
	return;
}

if (php_sapi_name() != 'cgi-fcgi') {
	echo "forbidden!";
	//echo php_sapi_name();
	return;
} else if (isset($_POST['switchpp3']) && $_POST['switchpp3'] == 1) {
	if (isset($_POST['pp3passwd']) && $_POST['pp3passwd'] != 'testpassword') {
		echo "wrong password!";
		return;
	} else {
		// open door
		$uri = file_get_contents('dooropenuri.txt');
		$switch = file_get_contents('http://'.$nodemcu.'/'.$uri);
		echo $switch;
	}
}

?>
