<?php

function get_command_filename ($session_id)
{
	//return session_save_path ()."/command.gda";
	return session_save_path ()."/GDA".$session_id."Cpipe";
}

function get_reply_filename ($session_id)
{
	//return session_save_path ()."/reply.gda";
	return session_save_path ()."/GDA".$session_id."Rpipe";
}

function try_include ($file, $debug=false)
{
	$array = explode (":", get_include_path ());
	foreach ($array as $index => $path) {
		if (file_exists ($path.DIRECTORY_SEPARATOR.$file)) {
			include_once ($path.DIRECTORY_SEPARATOR.$file);
			if ($debug)
				echo "<b>Using PEAR MDB2:</b> ".$path.DIRECTORY_SEPARATOR.$file."\n";
			return true;
		}
	}
	return false;
}

function gda_add_hash ($key, $text)
{
	if ($key == "")
		return "NOHASH\n".$text;
	else
		return hmac ($key, $text)."\n".$text;
}

// Create an md5 HMAC
function hmac ($key, $data)
{
    // RFC 2104 HMAC implementation for php.
    // Creates an md5 HMAC.
    // Eliminates the need to install mhash to compute a HMAC
    // Hacked by Lance Rushing

    $b = 64; // byte length for md5
    if (strlen($key) > $b) {
        $key = pack("H*",md5($key));
    }
    $key  = str_pad($key, $b, chr(0x00));
    $ipad = str_pad('', $b, chr(0x36));
    $opad = str_pad('', $b, chr(0x5c));
    $k_ipad = $key ^ $ipad ;
    $k_opad = $key ^ $opad;

    return md5($k_opad  . pack("H*",md5($k_ipad . $data)));
}

// Generate a random character string
function rand_str($length = 32, $chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890')
{
    // Length of character list
    $chars_length = (strlen($chars) - 1);

    // Start our string
    $string = $chars{rand(0, $chars_length)};
   
    // Generate random string
    for ($i = 1; $i < $length; $i = strlen($string))
    {
        // Grab a random character from our list
        $r = $chars{rand(0, $chars_length)};
       
        // Make sure the same two characters don't appear next to each other
        if ($r != $string{$i - 1}) $string .=  $r;
    }
   
    // Return the string
    return $string;
}

function mdb2_type_to_gtype ($mdb2type)
{
	switch ($mdb2type) {
	default:
	case "text":
	case "clob":
		return "gchararray";
	case "integer":
		return "gint";
	case "boolean":
		return "boolean";
	case "decimal":
		return "GdaNumerical";
	case "float":
		return "gdouble";
	case "date":
		return "GDate";
	case "time":
		return "GdaTime";
	case "timestamp":
		return "GDateTime";
	case "blob":
		return "GdaBinary";
	}
}

/**
 * Logging class:
 * - contains lfile, lopen and lwrite methods
 * - lfile sets path and name of log file
 * - lwrite will write message to the log file
 * - first call of the lwrite will open log file implicitly
 * - message is written with the following format: hh:mm:ss (script name) message
 */
class Logging{
    // define default log file
    private $log_file = '/tmp/gdalog';
    // define file pointer
    private $fp = null;
    // set log file (path and name)
    public function lfile($path) {
        $this->log_file = $path;
    }
    // write message to the log file
    public function lwrite($message){
        // if file pointer doesn't exist, then open log file
        if (!$this->fp) $this->lopen();
        // define script name
        $script_name = pathinfo($_SERVER['PHP_SELF'], PATHINFO_FILENAME);
        // define current time
        $time = date('H:i:s');
        // write current time, script name and message to the log file
        fwrite($this->fp, "$time ($script_name) $message\n");
    }
    // open log file
    private function lopen(){
        // define log file path and name
        $lfile = $this->log_file;
        // define the current date (it will be appended to the log file name)
        $today = date('Y-m-d');
        // open log file for writing only; place the file pointer at the end of the file
        // if the file does not exist, attempt to create it
        $this->fp = fopen($lfile . '_' . $today, 'a') or exit("Can't open $lfile!");
    }
}

/*
 * Define this variable to enable logging
 */
//$log = new Logging();

?>