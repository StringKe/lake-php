--TEST--
pdo_lake: session database and settings, query ids, statistics and killQuery
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
$db->exec('CREATE DATABASE IF NOT EXISTS pdo_lake_other');

var_dump($db->useDatabase('pdo_lake_other'));
var_dump($db->query('SELECT currentDatabase()')->fetchColumn(), $db->getAttribute(Pdo\Lake::ATTR_DATABASE), $db->info()['database']);
var_dump($db->setAttribute(Pdo\Lake::ATTR_DATABASE, LAKE_TEST_DATABASE));
var_dump($db->query('SELECT currentDatabase()')->fetchColumn());

var_dump($db->setSessionSetting('max_threads', '3'));
var_dump($db->query("SELECT value FROM system.settings WHERE name = 'max_threads'")->fetchColumn());

try {
    $db->useDatabase('pdo_lake_missing');
} catch (PDOException $e) {
    echo $e->errorInfo[0], "\n";
}
var_dump($db->query('SELECT currentDatabase()')->fetchColumn());

$st = $db->query('SELECT number FROM numbers(1000)');
$id = $st->getAttribute(Pdo\Lake::ATTR_QUERY_ID);
var_dump(is_string($id) && $id !== '', $id === $db->lastQueryId());
$st->fetchAll();
$stats = $st->getAttribute(Pdo\Lake::ATTR_QUERY_STATS);
var_dump(array_keys($stats), $stats['read_rows']);

$long = $db->query('SELECT number FROM numbers(10000000000)');
$long->fetch();
var_dump($db->killQuery($long->getAttribute(Pdo\Lake::ATTR_QUERY_ID)));
try {
    while ($long->fetch()) {
    }
    echo "drained\n";
} catch (PDOException $e) {
    echo "killed\n";
}
var_dump($db->query('SELECT 1')->fetchColumn());
try {
    $db->killQuery('pdo-lake-no-such-query');
} catch (PDOException $e) {
    echo $e->errorInfo[1], "\n";
}

$db->exec('DROP DATABASE pdo_lake_other');
?>
--EXPECT--
bool(true)
string(14) "pdo_lake_other"
string(14) "pdo_lake_other"
string(14) "pdo_lake_other"
bool(true)
string(13) "pdo_lake_test"
bool(true)
string(1) "3"
3D000
string(13) "pdo_lake_test"
bool(true)
bool(true)
array(9) {
  [0]=>
  string(10) "total_rows"
  [1]=>
  string(11) "total_bytes"
  [2]=>
  string(9) "read_rows"
  [3]=>
  string(10) "read_bytes"
  [4]=>
  string(10) "write_rows"
  [5]=>
  string(11) "write_bytes"
  [6]=>
  string(15) "running_time_ms"
  [7]=>
  string(15) "spill_file_nums"
  [8]=>
  string(11) "spill_bytes"
}
int(1000)
bool(true)
killed
int(1)
404
