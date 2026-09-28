--TEST--
pdo_lake: persistent connections are reused and keep working
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$options = [PDO::ATTR_PERSISTENT => true, PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION];
$first = new PDO(lake_dsn(), lake_user(), lake_password(), $options);
$first->exec('SET max_threads = 3');
$first = null;

$second = new PDO(lake_dsn(), lake_user(), lake_password(), $options);
var_dump($second->query("SELECT value FROM system.settings WHERE name = 'max_threads'")->fetchColumn());
var_dump($second->query('SELECT 7')->fetchColumn());
?>
--EXPECT--
string(1) "3"
int(7)
