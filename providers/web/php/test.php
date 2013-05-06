<?php

session_cache_limiter('nocache');
header('Content-type: text/plain; charset=UTF-8');

include_once "gda-utils.php";
include_once "gda-exception.php";
include_once "gda-config.php";

if (! try_include ("MDB2.php", true)) {
	echo "ERROR: The PEAR MDB2 extension is required\n";
	exit (1);
}

if (! extension_loaded ("SimpleXML")) {
	echo "ERROR: The SimpleXML extension is required\n";
	exit (1);
}

function handle_pear_error ($res)
{
	if (PEAR::isError($res)) {
		$cause = "\tStandard Message [".$res->getMessage()."]\n". 
			"\tUser Information [".$res->getUserInfo()."]\n".
			"\tDebug Information [".$res->getDebugInfo()."]";
		echo "ERROR: $cause\n";
	}
}

$mdb2 = MDB2::connect("mysql://vmalerba:Escobar@unix(/tmp/mysql.sock)/sales");
handle_pear_error ($mdb2);
$mdb2->loadModule('Reverse', null, true);

$prep = $mdb2->prepare((string) "SHOW VARIABLES WHERE Variable_name = 'lower_case_table_names'",
		       null, MDB2_PREPARE_RESULT);
//$prep = $mdb2->prepare((string) "Select * from information_schema.tables",
//		       null, MDB2_PREPARE_RESULT);
handle_pear_error ($prep);

$res = $prep->execute (null);
handle_pear_error ($res);

$reply = new SimpleXMLElement("<reply></reply>");
$node = $reply->addChild ("gda_array", null);
$ncols = $res->numCols();

/* compute field type */
echo "M1\n";
$tableinfo = $mdb2->reverse->tableInfo ($res, NULL);
echo "M2\n";
if (! PEAR::isError ($tableinfo)) {
	//var_dump($tableinfo);
	$gtypes = array();
	$dbtypes = array();
	for ($i = 0; $i < $ncols; $i++) {
		$gtypes [$i] = mdb2_type_to_gtype ($tableinfo[$i]['mdb2type']);
		$dbtypes [$i] = $tableinfo[$i]['type'];
	}
}

$colnames = $res->getColumnNames ();
if (PEAR::isError ($colnames))
	unset ($colnames);
for ($i = 0; $i < $ncols; $i++) {
	$field = $node->addChild ("gda_array_field", null);
	$field->addAttribute ("id", "FI".$i);
	if (isset ($colnames)) {
		foreach ($colnames as $name => $pos) {
			if ($pos == $i) {
				$field->addAttribute ("name", $name);
				break;
			}
		}
	}
	if (isset ($gtypes))
		$field->addAttribute ("gdatype", $gtypes[$i]);
	else
		$field->addAttribute ("gdatype", "string");
	if (isset ($dbtypes) && $dbtypes[$i])
		$field->addAttribute ("dbtype", $dbtypes[$i]);
	$field->addAttribute ("nullok", "TRUE");
}
		
$data = $node->addChild ("gda_array_data", null);
while (($row = $res->fetchRow())) {
	// MDB2's default fetchmode is MDB2_FETCHMODE_ORDERED
	$xmlrow = $data->addChild ("gda_array_row", null);
	for ($i = 0; $i < $ncols; $i++) {
		$val = $xmlrow->addChild ("gda_value");
		$val[0] = str_replace ("&", "&amp;", $row[$i]);
	}
}
echo $reply->asXml();

echo "OK.\n";

?>