<?php
/*
 * This file should be put outside the server's ROOT directory to avoid
 * malicious accessing, for example in /etc.
 *
 * It is read only by the gda-config.php script.
 */

/*
 * initial shared secret: will have to be passed as the SECRET argument when opening
 * the connection from Libgda
 */
$init_shared = "MySecret";

/*
 * declared connections: for each connection which can be opened by Libgda, the
 * the connection's password and the real connection's DSN need to be added respectively
 * to the $cnc and $dsn arrays, using the connection name as a key. The connection name
 * and password have no significance outside of the Libgda's context and be arbitrary.
 * However the real connection's DSN need to be valid for the PEAR's MDB2 module, as
 * per http://pear.php.net/manual/en/package.database.mdb2.intro-dsn.php
 *
 */

/* sample connection cnc1 */
$cnc["cnc1"] = "MyPass1";
$dsn["cnc1"] = "pgsql://gdauser:GdaUser@127.0.0.1/test";

/* sample connection cnc2 */
$cnc["cnc2"] = "MyPass2";
$dsn["cnc2"] = "mysql://gdauser:GdaUser@unix(/var/run/mysqld/mysqld.sock)/test1";

?>
