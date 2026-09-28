--TEST--
pdo_lake: large results stream page by page, closeCursor abandons a stream and statements re-execute
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();

$before = memory_get_usage();
$sum = 0;
$count = 0;
foreach ($db->query('SELECT number, repeat(\'x\', 64) AS pad FROM numbers(300000)', PDO::FETCH_NUM) as [$n]) {
    $sum += $n;
    $count++;
}
var_dump($count, $sum === 299999 * 300000 / 2, memory_get_usage() - $before < 1024 * 1024);

$st = $db->query('SELECT number FROM numbers(1000000)');
var_dump(is_int($st->fetchColumn()));
var_dump($st->closeCursor(), $st->fetch());
var_dump($db->query('SELECT 42')->fetchColumn());

$st = $db->prepare('SELECT number FROM numbers(10) WHERE number >= ? ORDER BY number');
foreach ([7, 3, 11] as $from) {
    $st->execute([$from]);
    echo json_encode($st->fetchAll(PDO::FETCH_COLUMN)), "\n";
}

$abandoned = $db->query('SELECT number FROM numbers(500000)');
$abandoned->fetch();
unset($abandoned);
var_dump($db->query('SELECT count(*) FROM numbers(5)')->fetchColumn());
?>
--EXPECT--
int(300000)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
int(42)
[7,8,9]
[3,4,5,6,7,8,9]
[]
int(5)
