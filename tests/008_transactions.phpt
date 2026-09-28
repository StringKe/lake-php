--TEST--
pdo_lake: explicit transactions commit, roll back and track their state
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
$db->exec('CREATE OR REPLACE TABLE t_tx (id INT)');

var_dump($db->beginTransaction(), $db->inTransaction());
$db->exec('INSERT INTO t_tx VALUES (1)');
var_dump($db->rollBack(), $db->inTransaction());
var_dump($db->query('SELECT count(*) FROM t_tx')->fetchColumn());

$db->beginTransaction();
$db->exec('INSERT INTO t_tx VALUES (2), (3)');
var_dump($db->commit());
var_dump($db->query('SELECT count(*) FROM t_tx')->fetchColumn());

$db->beginTransaction();
try {
    $db->beginTransaction();
} catch (PDOException $e) {
    echo $e->getMessage(), "\n";
}
$db->rollBack();

try {
    $db->commit();
} catch (PDOException $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
int(0)
bool(true)
int(2)
There is already an active transaction
There is no active transaction
