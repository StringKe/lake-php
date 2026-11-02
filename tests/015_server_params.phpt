--TEST--
pdo_lake: native server-side parameters round-trip when the server supports them
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php
require __DIR__.'/skipif.inc';
require_once __DIR__.'/config.inc';
if (!PDO::connect(lake_dsn(), lake_user(), lake_password())->getAttribute(Pdo\Lake::ATTR_SERVER_SIDE_PARAMS)) {
    die('skip server does not support server-side parameters');
}
?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect([PDO::ATTR_EMULATE_PREPARES => false]);
var_dump($db->getAttribute(PDO::ATTR_EMULATE_PREPARES));

$tricky = "q'uo\"te \\ ?mark :name";
$st = $db->prepare('SELECT :s, :a, :a, :n, 0.5 + :f::DOUBLE');
$st->bindValue('s', $tricky);
$st->bindValue('a', PHP_INT_MAX, PDO::PARAM_INT);
$st->bindValue('n', null, PDO::PARAM_NULL);
$st->bindValue('f', 0.25);
$st->execute();
$row = $st->fetch(PDO::FETCH_NUM);
var_dump($row[0] === $tricky, $row[1] === PHP_INT_MAX, $row[2] === PHP_INT_MAX, $row[3], $row[4]);

$st = $db->prepare('SELECT ?, ? FROM numbers(3) WHERE number > ?');
$st->execute(['x', true, 0]);
echo json_encode($st->fetchAll(PDO::FETCH_NUM)), "\n";

$st = $db->prepare('SELECT CASE WHEN number = ? THEN ? WHEN number = ? THEN ? ELSE ? END FROM numbers(3) ORDER BY number');
foreach ([0, 'a', 1, 'b', 'c'] as $i => $value) {
    $st->bindValue($i + 1, $value, is_int($value) ? PDO::PARAM_INT : PDO::PARAM_STR);
}
$st->execute();
echo json_encode($st->fetchAll(PDO::FETCH_COLUMN)), "\n";

$st = $db->prepare('SELECT CASE WHEN number = :zero THEN :a WHEN number = :one THEN :b ELSE :a END FROM numbers(3) ORDER BY number');
$st->bindValue('zero', 0, PDO::PARAM_INT);
$st->bindValue('one', 1, PDO::PARAM_INT);
$st->bindValue('a', 'a');
$st->bindValue('b', 'b');
$st->execute();
echo json_encode($st->fetchAll(PDO::FETCH_COLUMN)), "\n";

$db->exec('CREATE OR REPLACE TABLE t_native (id INT, raw BINARY NULL)');
$ins = $db->prepare('INSERT INTO t_native VALUES (?, ?)');
$ins->bindValue(1, 1, PDO::PARAM_INT);
$ins->bindValue(2, "\x00'\xff", PDO::PARAM_LOB);
$ins->execute();
var_dump($ins->rowCount(), bin2hex($db->query('SELECT raw FROM t_native')->fetchColumn()));

var_dump($db->query("SELECT {'a':1}['a']")->fetchColumn());

$st = $db->prepare('SELECT ?, ?');
$st->bindValue(1, 1);
try {
    $st->execute();
} catch (PDOException $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
bool(false)
bool(true)
bool(true)
bool(true)
NULL
float(0.75)
[["x","1"],["x","1"]]
["a","b","c"]
["a","b","a"]
int(1)
string(6) "0027ff"
int(1)
SQLSTATE[22000]: Data exception: 1006 parameter substitution failed: %s
