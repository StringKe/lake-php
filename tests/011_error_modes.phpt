--TEST--
pdo_lake: server errors surface through every PDO error mode with SQLSTATE and native code
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
var_dump($db->query('SELECT * FROM pdo_lake_missing_table'));
[$state, $code, $message] = $db->errorInfo();
var_dump($state, is_int($code) && $code > 0, str_contains($message, 'pdo_lake_missing_table'), str_contains($message, '[v'));
var_dump($db->errorCode());
var_dump($db->query('SELECT 1')->fetchColumn(), $db->errorCode());

var_dump($db->exec('SELEC 1'), $db->errorInfo()[0]);

$st = $db->prepare('SELECT 1 / ?');
var_dump($st->execute(['x']), $st->errorInfo()[0]);

$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);
var_dump($db->query('SELECT * FROM pdo_lake_missing_table'));

$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
try {
    $db->query('SELECT * FROM pdo_lake_missing_table');
} catch (PDOException $e) {
    var_dump($e->getCode(), $e->errorInfo[0], str_starts_with($e->getMessage(), 'SQLSTATE[42S02]'));
}
$db->exec('CREATE OR REPLACE TABLE t_errors (id INT)');
foreach (['CREATE TABLE t_errors (id INT)', 'USE pdo_lake_missing_db', 'INSERT INTO t_errors VALUES (1, 2)', "SELECT 'x'::INT"] as $sql) {
    try {
        $db->exec($sql);
    } catch (PDOException $e) {
        var_dump($e->errorInfo[0]);
    }
}
?>
--EXPECTF--
bool(false)
string(5) "42S02"
bool(true)
bool(true)
bool(false)
string(5) "42S02"
int(1)
string(5) "00000"
bool(false)
string(5) "42000"
bool(false)
string(5) "22000"

Warning: PDO::query(): SQLSTATE[42S02]: Base table or view not found: 1025 %a on line %d
bool(false)
string(5) "42S02"
string(5) "42S02"
bool(true)
string(5) "42S01"
string(5) "3D000"
string(5) "21S01"
string(5) "22000"
