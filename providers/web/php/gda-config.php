<?php

/*
 * Add this directive to use the implementation of PEAR MDB2 provided
 * in Libgda
 */
//set_include_path (get_include_path().":PEAR_MDB2");

/*
 * Add the directory to which the gda-secure-config.php has been copied (here /etc for example)
 */
set_include_path (get_include_path().":/etc");





/*
 * NO MODIFICATION BELOW
 */
$found = false;
$array = explode (":", get_include_path ());
foreach ($array as $index => $path) {
	if (file_exists ($path.DIRECTORY_SEPARATOR."gda-secure-config.php")) {
		include_once ($path.DIRECTORY_SEPARATOR."gda-secure-config.php");
		$found = true;
	}
}
if (! $found) {
	echo "Missing gda-secure-config.php file!";
	exit (1);
}

?>
