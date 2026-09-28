--TEST--
pdo_lake: FlightSQL transport queries, binds, streams, switches session state and runs transactions
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php
require __DIR__.'/skipif.inc';
require_once __DIR__.'/config.inc';
try {
    PDO::connect(lake_flight_dsn(), lake_user(), lake_password());
} catch (PDOException $e) {
    die('skip FlightSQL endpoint unavailable: '.$e->getMessage());
}
?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect([], lake_flight_dsn());
var_dump($db->info()['handler']);

$db->exec('CREATE OR REPLACE TABLE t_flight (id INT, s STRING NULL, raw BINARY NULL)');
$st = $db->prepare('INSERT INTO t_flight VALUES (:id, :s, :raw)');
$st->bindValue('id', 1, PDO::PARAM_INT);
$st->bindValue('s', "it's \\ ok");
$st->bindValue('raw', "\x00\xff", PDO::PARAM_LOB);
$st->execute();
var_dump($st->rowCount());

$row = $db->query('SELECT id, s, raw FROM t_flight')->fetch(PDO::FETCH_NUM);
var_dump($row[0], $row[1], bin2hex($row[2]));

$n = 0;
foreach ($db->query('SELECT number FROM numbers(200000)', PDO::FETCH_NUM) as $_) {
    $n++;
}
var_dump($n);

$db->exec('CREATE DATABASE IF NOT EXISTS pdo_lake_other');
var_dump($db->useDatabase('pdo_lake_other'), $db->query('SELECT currentDatabase()')->fetchColumn());
$db->useDatabase(LAKE_TEST_DATABASE);
var_dump($db->setSessionSetting('max_threads', '2'), $db->query("SELECT value FROM system.settings WHERE name = 'max_threads'")->fetchColumn());

try {
    $db->useWarehouse('wh');
} catch (PDOException $e) {
    echo $e->errorInfo[0], "\n";
}

$db->beginTransaction();
$db->exec('INSERT INTO t_flight (id) VALUES (2)');
$db->rollBack();
var_dump($db->query('SELECT count(*) FROM t_flight')->fetchColumn());
$db->exec('DROP DATABASE pdo_lake_other');
?>
--EXPECT--
string(9) "FlightSQL"
int(1)
int(1)
string(9) "it's \ ok"
string(4) "00ff"
int(200000)
bool(true)
string(14) "pdo_lake_other"
bool(true)
string(1) "2"
HY000
int(1)
