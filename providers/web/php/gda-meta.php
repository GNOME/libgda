<?php

/*
 * _gda_web_meta__info
 */
function do_meta_info ($reply, &$mdb2)
{
	$catalog = get_catalog ($reply, &$mdb2);

	$node = $reply->addChild ("gda_array", null);
	declare_column ($node, 0, "name", "string", false);
	$data = $node->addChild ("gda_array_data", null);
	$xmlrow = $data->addChild ("gda_array_row", null);

	add_value_child ($xmlrow, $catalog);
}

/*
 * _gda_web_meta__btypes
 */
function do_meta_btypes ($reply, &$mdb2)
{
	$res = $mdb2->datatype->getValidTypes();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);

	$col = 0;
	declare_column ($node, $col++, "short_type_name", "string", false);
	declare_column ($node, $col++, "full_type_name", "string", false);
	declare_column ($node, $col++, "gtype", "string", false);
	declare_column ($node, $col++, "comments", "string", true);
	declare_column ($node, $col++, "synonyms", "string", true);
	declare_column ($node, $col++, "internal", "boolean", false);

	$data = $node->addChild ("gda_array_data", null);
	foreach ($res as $i => $value) {
		$xmlrow = $data->addChild ("gda_array_row", null);
		add_value_child ($xmlrow, $i);
		add_value_child ($xmlrow, $i);
		$field = array ("type" => $i);
		$mtype = $mdb2->datatype->mapNativeDatatype ($field);
		if (PEAR::isError($mtype))
			add_value_child ($xmlrow, "gchararray"); // FIXME, maybe use mapNativeDatatype()
		else {
			$done = false;
			foreach ($mtype[0] as $j => $type) {
				if ($type == "text") {
					add_value_child ($xmlrow, $type);
					$done = true;
					break;
				}
			}
			if (!$done)
				add_value_child ($xmlrow, $mtype[0][0]);
		}
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, false, true);
	}
}

/*
 * _gda_web_meta__schemata
 */
function do_meta_schemas ($reply, &$mdb2, $schema_name = null)
{
	if (determine_db_type ($mdb2) ==  "PostgreSQL") {
		do_meta_schemas_pgsql ($reply, &$mdb2);
		return;
	}
	$catalog = get_catalog ($reply, &$mdb2);

	$res = $mdb2->manager->listDatabases();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	declare_column ($node, 0, "catalog_name", "string", false);
	declare_column ($node, 1, "schema_name", "string", false);
	declare_column ($node, 2, "schema_owner", "string", false);
	declare_column ($node, 3, "schema_internal", "boolean", false);
	$data = $node->addChild ("gda_array_data", null);
	
	foreach ($res as $i => $value) {
		if ($schema_name && ($schema_name != $value))
			continue;
		$xmlrow = $data->addChild ("gda_array_row", null);

		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $value);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, false, true);
	}
}

function do_meta_schemas_pgsql ($reply, &$mdb2)
{
	$sql = "SELECT current_database()::information_schema.sql_identifier AS catalog_name, n.nspname::information_schema.sql_identifier AS schema_name, u.rolname::information_schema.sql_identifier AS schema_owner, CASE WHEN n.nspname::information_schema.sql_identifier ~ '^pg_' THEN TRUE WHEN n.nspname::information_schema.sql_identifier = 'information_schema' THEN TRUE ELSE FALSE END FROM pg_namespace n, pg_roles u WHERE n.nspowner = u.oid";
	$res = &$mdb2->query ($sql);
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	declare_column ($node, 0, "catalog_name", "string", false);
	declare_column ($node, 1, "schema_name", "string", false);
	declare_column ($node, 2, "schema_owner", "string", false);
	declare_column ($node, 3, "schema_internal", "boolean", false);
	$data = $node->addChild ("gda_array_data", null);
	while (($row = $res->fetchRow())) {
		$xmlrow = $data->addChild ("gda_array_row", null);

		add_value_child ($xmlrow, $row[0]);
		add_value_child ($xmlrow, $row[1]);
		add_value_child ($xmlrow, $row[2]);
		add_value_child ($xmlrow, $row[3] == 't' ? "TRUE" : "FALSE");
	}
}

/*
 * _gda_web_meta__schemata
 */
function do_meta_tables ($reply, &$mdb2, $table_schema = null, $table_name = null)
{
	$catalog = get_catalog ($reply, &$mdb2);
	$res = $mdb2->manager->listTables();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "table_catalog", "string", false);
	declare_column ($node, $col++, "table_schema", "string", false);
	declare_column ($node, $col++, "table_name", "string", false);
	declare_column ($node, $col++, "table_type", "string", false);
	declare_column ($node, $col++, "is_insertable_into", "boolean", true);
	declare_column ($node, $col++, "table_comments", "string", true);
	declare_column ($node, $col++, "table_short_name", "string", false);
	declare_column ($node, $col++, "table_full_name", "string", false);
	declare_column ($node, $col++, "table_owner", "string", true);
	$data = $node->addChild ("gda_array_data", null);
	
	foreach ($res as $i => $value) {
		if ($table_name && ($table_name != $value))
			continue;
		$xmlrow = $data->addChild ("gda_array_row", null);
		
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $value);
		add_value_child ($xmlrow, "BASE TABLE");
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, $value);
		add_value_child ($xmlrow, $catalog.".".$value);
		add_value_child ($xmlrow, null);
	}
}

function do_meta_views ($reply, &$mdb2, $table_schema = null, $table_name = null)
{
	$catalog = get_catalog ($reply, &$mdb2);

	$res = $mdb2->manager->listViews();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "table_catalog", "string", false);
	declare_column ($node, $col++, "table_schema", "string", false);
	declare_column ($node, $col++, "table_name", "string", false);
	declare_column ($node, $col++, "view_definition", "string", true);
	declare_column ($node, $col++, "check_option", "string", true);
	declare_column ($node, $col++, "is_updatable", "boolean", true);
	$data = $node->addChild ("gda_array_data", null);
	
	foreach ($res as $i => $value) {
		if ($table_name && ($table_name != $value))
			continue;

		$xmlrow = $data->addChild ("gda_array_row", null);

		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $value);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
	}
}

function do_meta_columns_named ($reply, &$mdb2, $data, $table_name)
{
	$catalog = get_catalog ($reply, &$mdb2);
	$res = $mdb2->reverse->tableInfo($table_name);
	if (PEAR::isError($res))
		return;

	foreach ($res as $i => $value) {
		$xmlrow = $data->addChild ("gda_array_row", null);

		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $table_name);
		add_value_child ($xmlrow, $value['name']);
		add_value_child ($xmlrow, $i+1);

		if ($value['default'] != "")
			add_value_child ($xmlrow, $value['default']);
		else
			add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, true, true);

		add_value_child ($xmlrow, $value['nativetype']);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, mdb2_type_to_gtype ($value['mdb2type']));
		add_value_child ($xmlrow, $value['len']);

		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);

		if ($value['autoincrement'])
			add_value_child ($xmlrow, "AUTO_INCREMENT");
		else
			add_value_child ($xmlrow, null);

		add_value_child ($xmlrow, null);
		add_value_child ($xmlrow, null);
	}
}

function do_meta_columns ($reply, &$mdb2, $table_schema=null, $table_name=null)
{
	if (! $table_name) {
		$res = $mdb2->manager->listTables();
		handle_pear_error ($res, $reply);
	}

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "table_catalog", "string", false);
	declare_column ($node, $col++, "table_schema", "string", false);
	declare_column ($node, $col++, "table_name", "string", false);
	declare_column ($node, $col++, "column_name", "string", false);
	declare_column ($node, $col++, "ordinal_position", "gint", false);
	declare_column ($node, $col++, "column_default", "string", true);
	declare_column ($node, $col++, "is_nullable", "boolean", false);
	declare_column ($node, $col++, "data_type", "string", true);
	declare_column ($node, $col++, "array_spec", "string", true);
	declare_column ($node, $col++, "gtype", "string", false);
	declare_column ($node, $col++, "character_maximum_length", "gint", true);
	declare_column ($node, $col++, "character_octet_length", "gint", true);
	declare_column ($node, $col++, "numeric_precision", "gint", true);
	declare_column ($node, $col++, "numeric_scale", "gint", true);
	declare_column ($node, $col++, "datetime_precision", "gint", true);
	declare_column ($node, $col++, "character_set_catalog", "string", true);
	declare_column ($node, $col++, "character_set_schema", "string", true);
	declare_column ($node, $col++, "character_set_name", "string", true);
	declare_column ($node, $col++, "collation_catalog", "string", true);
	declare_column ($node, $col++, "collation_schema", "string", true);
	declare_column ($node, $col++, "collation_name", "string", true);
	declare_column ($node, $col++, "extra", "string", true);
	declare_column ($node, $col++, "is_updatable", "boolean", true);
	declare_column ($node, $col++, "column_comments", "string", true);

	$data = $node->addChild ("gda_array_data", null);

	if ($table_name)
		do_meta_columns_named ($reply, $mdb2, $data, $table_name);
	else {
		foreach ($res as $i => $value)
			do_meta_columns_named ($reply, $mdb2, $data, $value);
	}
}

function do_meta_constraints_tab ($reply, &$mdb2, $table_schema=null, $table_name=null, $constraint_name=null)
{
	$catalog = get_catalog ($reply, &$mdb2);
	$res = $mdb2->manager->listTables();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "constraint_catalog", "string", true);
	declare_column ($node, $col++, "constraint_schema", "string", true);
	declare_column ($node, $col++, "constraint_name", "string", false);
	declare_column ($node, $col++, "table_catalog", "string", false);
	declare_column ($node, $col++, "table_schema", "string", false);
	declare_column ($node, $col++, "table_name", "string", false);
	declare_column ($node, $col++, "constraint_type", "string", false);
	declare_column ($node, $col++, "check_clause", "string", true);
	declare_column ($node, $col++, "is_deferrable", "boolean", true);
	declare_column ($node, $col++, "initially_deferred", "boolean", true);

	$data = $node->addChild ("gda_array_data", null);
	foreach ($res as $i => $value) {
		if ($table_name && ($table_name != $value))
			continue;

		$constraints = $mdb2->manager->listTableConstraints ($value);
		if (PEAR::isError($constraints))
			continue;

		foreach ($constraints as $cindex => $cname) {
			if ($constraint_name && ($constraint_name != $cname))
				continue;

			$cdef = $mdb2->reverse->getTableConstraintDefinition ($value, $cname);
			if (PEAR::isError($cdef))
			continue;

			$xmlrow = $data->addChild ("gda_array_row", null);
			
			add_value_child ($xmlrow, null);
			add_value_child ($xmlrow, null);
			add_value_child ($xmlrow, $cname);
			add_value_child ($xmlrow, $catalog);
			add_value_child ($xmlrow, $catalog);
			add_value_child ($xmlrow, $value);
			if ($cdef ["primary"]) {
				add_value_child ($xmlrow, "PRIMARY KEY");
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, null);
			}
			else if ($cdef ["unique"]) {
				add_value_child ($xmlrow, "UNIQUE");
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, null);
			}
			else if ($cdef ["foreign"]) {
				add_value_child ($xmlrow, "FOREIGN KEY");
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, $cdef ["deferrable"] ? true : false, true);
				add_value_child ($xmlrow, $cdef ["initiallydeferred"] ? true : false, true);
			}
			else if ($cdef ["check"]) {
				add_value_child ($xmlrow, "CHECK");
				add_value_child ($xmlrow, null); /* FIXME: how to obtain that ? */
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, null);
			}
			else {
				add_value_child ($xmlrow, "UNKNOWN");
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, null);
				add_value_child ($xmlrow, null);
			}
		}
	}
}


function do_meta_constraints_ref ($reply, &$mdb2, $table_schema=null, $table_name=null, $constraint_name=null)
{
	$catalog = get_catalog ($reply, &$mdb2);
	$res = $mdb2->manager->listTables();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "table_catalog", "string", false);
	declare_column ($node, $col++, "table_schema", "string", false);
	declare_column ($node, $col++, "table_name", "string", false);
	declare_column ($node, $col++, "constraint_name", "string", false);
	declare_column ($node, $col++, "ref_table_catalog", "string", false);
	declare_column ($node, $col++, "ref_table_schema", "string", false);
	declare_column ($node, $col++, "ref_table_name", "string", false);
	declare_column ($node, $col++, "ref_constraint_name", "string", false);	
	declare_column ($node, $col++, "match_option", "string", true);
	declare_column ($node, $col++, "update_rule", "string", true);
	declare_column ($node, $col++, "delete_rule", "string", true);


	$data = $node->addChild ("gda_array_data", null);
	foreach ($res as $i => $value) {
		if ($table_name && ($table_name != $value))
			continue;

		$constraints = $mdb2->manager->listTableConstraints ($value);
		if (PEAR::isError($constraints))
			continue;

		foreach ($constraints as $cindex => $cname) {
			if ($constraint_name && ($constraint_name != $cname))
				continue;

			$cdef = $mdb2->reverse->getTableConstraintDefinition ($value, $cname);
			if (PEAR::isError($cdef))
			continue;

			if (! $cdef ["foreign"])
				continue;

			$xmlrow = $data->addChild ("gda_array_row", null);
			
			add_value_child ($xmlrow, $catalog);
			add_value_child ($xmlrow, $catalog);
			add_value_child ($xmlrow, $value);
			add_value_child ($xmlrow, $cname);

			add_value_child ($xmlrow, $catalog);
			add_value_child ($xmlrow, $catalog);
			add_value_child ($xmlrow, $cdef["references"]["table"]);
			add_value_child ($xmlrow, _get_table_pk_constraint ($mdb2, $cdef["references"]));

			if (isset ($cdef["match"]) && ($cdef["match"] != "UNSPECIFIED"))
				add_value_child ($xmlrow, $cdef["match"]);
			else
				add_value_child ($xmlrow, null);
			add_value_child ($xmlrow, $cdef["onupdate"]);
			add_value_child ($xmlrow, $cdef["ondelete"]);
		}
	}
}

function _get_table_pk_constraint (&$mdb2, $references)
{
	$table_name = $references["table"];
	$constraints = $mdb2->manager->listTableConstraints ($table_name);
	if (PEAR::isError($constraints))
		return "";
	foreach ($constraints as $cindex => $cname) {
		$cdef = $mdb2->reverse->getTableConstraintDefinition ($table_name, $cname);
		if (PEAR::isError($constraints))
			continue;
		if (! $cdef ["primary"])
			continue;

		$rfields = $references["fields"];
		$allmatch = true;
		foreach ($cdef ["fields"] as $fname => $attrs) {
			if (!isset ($rfields [$fname]) ||
			    ($rfields [$fname]["position"] != $attrs["position"])) {
				$allmatch = false;
				break;
			}
		}

		if ($allmatch)
			return $cname;
	}
	return "";
}

function do_meta_key_columns ($reply, &$mdb2, $table_schema=null, $table_name=null, $constraint_name=null)
{
	$catalog = get_catalog ($reply, &$mdb2);
	$res = $mdb2->manager->listTables();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "table_catalog", "string", false);
	declare_column ($node, $col++, "table_schema", "string", false);
	declare_column ($node, $col++, "table_name", "string", false);
	declare_column ($node, $col++, "constraint_name", "string", false);
	declare_column ($node, $col++, "column_name", "string", false);
	declare_column ($node, $col++, "ordinal_position", "gint", false);

	$data = $node->addChild ("gda_array_data", null);
	foreach ($res as $i => $value) {
		if ($table_name && ($table_name != $value))
			continue;

		$constraints = $mdb2->manager->listTableConstraints ($value);
		if (PEAR::isError($constraints))
			continue;

		foreach ($constraints as $cindex => $cname) {
			if ($constraint_name && ($constraint_name != $cname))
				continue;

			$cdef = $mdb2->reverse->getTableConstraintDefinition ($value, $cname);
			if (PEAR::isError($cdef))
				continue;

			if (! $cdef ["primary"])
				continue;

			$fields = $cdef["fields"];
			if (!$fields)
				continue;

			foreach ($fields as $fname => $attrs) {
				$xmlrow = $data->addChild ("gda_array_row", null);
			
				add_value_child ($xmlrow, $catalog);
				add_value_child ($xmlrow, $catalog);
				add_value_child ($xmlrow, $value);
				add_value_child ($xmlrow, $cname);
				add_value_child ($xmlrow, $fname);
				add_value_child ($xmlrow, $attrs["position"]);
			}
		}
	}
}

function do_meta_check_columns ($reply, &$mdb2, $table_schema=null, $table_name=null, $constraint_name=null)
{
	$catalog = get_catalog ($reply, &$mdb2);
	$res = $mdb2->manager->listTables();
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "table_catalog", "string", false);
	declare_column ($node, $col++, "table_schema", "string", false);
	declare_column ($node, $col++, "table_name", "string", false);
	declare_column ($node, $col++, "constraint_name", "string", false);
	declare_column ($node, $col++, "column_name", "string", false);

	$data = $node->addChild ("gda_array_data", null);
	foreach ($res as $i => $value) {
		if ($table_name && ($table_name != $value))
			continue;

		$constraints = $mdb2->manager->listTableConstraints ($value);
		if (PEAR::isError($constraints))
			continue;

		foreach ($constraints as $cindex => $cname) {
			if ($constraint_name && ($constraint_name != $cname))
				continue;

			$cdef = $mdb2->reverse->getTableConstraintDefinition ($value, $cname);
			if (PEAR::isError($cdef))
				continue;

			if (! $cdef ["check"])
				continue;

			$fields = $cdef["fields"];
			if (!$fields)
				continue;

			foreach ($fields as $fname => $attrs) {
				$xmlrow = $data->addChild ("gda_array_row", null);
			
				add_value_child ($xmlrow, $catalog);
				add_value_child ($xmlrow, $catalog);
				add_value_child ($xmlrow, $value);
				add_value_child ($xmlrow, $cname);
				add_value_child ($xmlrow, $fname);
			}
		}
	}
}

function do_meta_triggers ($reply, &$mdb2, $table_schema=null, $table_name=null)
{
	$catalog = get_catalog ($reply, &$mdb2);
	$res = $mdb2->manager->listTableTriggers ($table_name);
	handle_pear_error ($res, $reply);

	$node = $reply->addChild ("gda_array", null);
	$col = 0;
	declare_column ($node, $col++, "trigger_catalog", "string", false);
	declare_column ($node, $col++, "trigger_schema", "string", false);
	declare_column ($node, $col++, "trigger_name", "string", false);
	declare_column ($node, $col++, "event_manipulation", "string", false);
	declare_column ($node, $col++, "event_object_catalog", "string", false);
	declare_column ($node, $col++, "event_object_schema", "string", false);
	declare_column ($node, $col++, "event_object_table", "string", false);
	declare_column ($node, $col++, "action_statement", "string", true);
	declare_column ($node, $col++, "action_orientation", "string", false);
	declare_column ($node, $col++, "condition_timing", "string", false);
	declare_column ($node, $col++, "trigger_comments", "string", true);
	declare_column ($node, $col++, "trigger_short_name", "string", false);
	declare_column ($node, $col++, "trigger_full_name", "string", false);

	$data = $node->addChild ("gda_array_data", null);
	foreach ($res as $i => $value) {
		$tdef = $mdb2->reverse->getTriggerDefinition($value);
		if (PEAR::isError($constraints))
			continue;

		print_r ($tdef);

		$xmlrow = $data->addChild ("gda_array_row", null);
		
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $value);
		add_value_child ($xmlrow, $tdef ["trigger_event"]);
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $catalog);
		add_value_child ($xmlrow, $tdef ["table_name"]);
		add_value_child ($xmlrow, $tdef ["trigger_body"]);
		add_value_child ($xmlrow, "ROW"); /* FIXME */
		add_value_child ($xmlrow, $tdef ["trigger_type"]);
		add_value_child ($xmlrow, $tdef ["trigger_comment"]);
		add_value_child ($xmlrow, $value);
		add_value_child ($xmlrow, $catalog.".".$value);
	}
}

/*
 * get the connection's catalog
 */
$catalog = null;
function get_catalog ($reply, &$mdb2)
{
	global $catalog;
	if (!$catalog) {
		$res = MDB2::parseDSN ($_SESSION['dsn']);
		handle_pear_error ($res, $reply);
		$catalog = $res ['database'];
	}
	return $catalog;
}

/*
 * Declare a column in the resulting XML tree
 */
function declare_column ($arraynode, $id, $name, $gdatype, $nullok)
{
	$field = $arraynode->addChild ("gda_array_field", null);

	$field->addAttribute ("id", "FI".$id);
	$field->addAttribute ("name", $name);
	$field->addAttribute ("gdatype", $gdatype);
	$field->addAttribute ("nullok", $nullok ? "TRUE" : "FALSE");
}

/*
 * Add a <gda_value> node to @node
 */
function add_value_child ($node, $value, $isbool = false)
{
	$val = $node->addChild ("gda_value");
	if (isset ($value)) {
		if ($isbool)
			$val[0] = ($value === true || $value == 't') ? "TRUE" : "FALSE";
		else
			$val[0] = str_replace ("&", "&amp;", $value);
	}
	else
		$val->addAttribute ("isnull", "t");
	return $val;
}
?>