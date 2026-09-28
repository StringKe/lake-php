--TEST--
pdo_lake: exec() and rowCount() report the rows each DML statement affected
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
var_dump($db->exec('CREATE OR REPLACE TABLE t_rows (id INT, v STRING)'));
var_dump($db->exec("INSERT INTO t_rows VALUES (1, 'a'), (2, 'b'), (3, 'c')"));
var_dump($db->exec("UPDATE t_rows SET v = 'z' WHERE id >= 2"));
var_dump($db->exec('DELETE FROM t_rows WHERE id = 3'));
var_dump($db->exec("UPDATE t_rows SET v = 'y' WHERE id = 99"));

$st = $db->prepare('INSERT INTO t_rows VALUES (?, ?), (?, ?)');
$st->execute([10, 'p', 11, 'q']);
var_dump($st->rowCount(), $st->columnCount(), $st->fetch());

$st = $db->prepare('UPDATE t_rows SET v = ? WHERE id > ?');
$st->execute(['w', 5]);
var_dump($st->rowCount());

$st = $db->prepare('DELETE FROM t_rows WHERE id IN (?, ?)');
$st->execute([10, 11]);
var_dump($st->rowCount());

$db->exec('CREATE OR REPLACE TABLE t_rows_src (id INT, v STRING)');
$db->exec("INSERT INTO t_rows_src VALUES (1, 'm'), (4, 'n')");
$merged = $db->query('MERGE INTO t_rows USING t_rows_src s ON t_rows.id = s.id WHEN MATCHED THEN UPDATE SET v = s.v WHEN NOT MATCHED THEN INSERT *');
var_dump($merged->rowCount());

$select = $db->query('SELECT * FROM t_rows ORDER BY id');
var_dump($select->rowCount());
$select->fetchAll();
var_dump($select->rowCount());
?>
--EXPECT--
int(0)
int(3)
int(2)
int(1)
int(0)
int(2)
int(0)
bool(false)
int(2)
int(2)
int(2)
int(0)
int(3)
