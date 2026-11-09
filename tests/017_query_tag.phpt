--TEST--
pdo_lake: query tag rides along with later statements, client name option, failures carry the query id
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect([Pdo\Lake::ATTR_CLIENT_NAME => 'pdo-lake-test/1']);
$tag = "SELECT value FROM system.settings WHERE name = 'query_tag'";

var_dump($db->setQueryTag('report-daily'));
var_dump($db->query($tag)->fetchColumn());
var_dump($db->query($tag)->fetchColumn());
var_dump($db->setQueryTag(''));
var_dump($db->query($tag)->fetchColumn());

$long = $db->query('SELECT number FROM numbers(10000000000)');
$long->fetch();
$id = $long->getAttribute(Pdo\Lake::ATTR_QUERY_ID);
$db->killQuery($id);
try {
    while ($long->fetch()) {
    }
} catch (PDOException $e) {
    var_dump(str_ends_with($e->getMessage(), "[query_id={$id}]"));
}
?>
--EXPECT--
bool(true)
string(12) "report-daily"
string(12) "report-daily"
bool(true)
string(0) ""
bool(true)
