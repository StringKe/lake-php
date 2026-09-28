--TEST--
pdo_lake: PDO fetch modes, bound columns and iteration
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$db = lake_connect();
$db->exec('CREATE OR REPLACE TABLE t_fetch (id INT, grp STRING, name STRING)');
$db->exec("INSERT INTO t_fetch VALUES (1, 'a', 'one'), (2, 'a', 'two'), (3, 'b', 'three')");
$sql = 'SELECT id, grp, name FROM t_fetch ORDER BY id';

final class Item
{
    public int $id;
    public string $grp;
    public string $name;
}

echo json_encode($db->query($sql)->fetch(PDO::FETCH_NUM)), "\n";
echo json_encode($db->query($sql)->fetch(PDO::FETCH_BOTH)), "\n";
echo json_encode($db->query($sql)->fetch(PDO::FETCH_OBJ)), "\n";
echo get_class($db->query($sql)->fetchObject(Item::class)), "\n";
echo json_encode($db->query($sql)->fetchAll(PDO::FETCH_COLUMN, 2)), "\n";
echo json_encode($db->query('SELECT id, name FROM t_fetch ORDER BY id')->fetchAll(PDO::FETCH_KEY_PAIR)), "\n";
echo json_encode($db->query('SELECT grp, name FROM t_fetch ORDER BY id')->fetchAll(PDO::FETCH_GROUP | PDO::FETCH_COLUMN)), "\n";
echo json_encode($db->query($sql)->fetchAll(PDO::FETCH_UNIQUE | PDO::FETCH_ASSOC)), "\n";
echo json_encode($db->query($sql)->fetchAll(PDO::FETCH_FUNC, fn (int $id, string $grp, string $name) => "{$grp}{$id}")), "\n";
echo json_encode(array_map(fn (Item $i) => $i->name, $db->query($sql)->fetchAll(PDO::FETCH_CLASS, Item::class))), "\n";

$st = $db->query($sql);
$st->bindColumn('name', $name);
$st->bindColumn(1, $id, PDO::PARAM_STR);
$st->fetch(PDO::FETCH_BOUND);
var_dump($id, $name);

$ids = [];
foreach ($db->query($sql, PDO::FETCH_ASSOC) as $row) {
    $ids[] = $row['id'];
}
echo json_encode($ids), "\n";

$st = $db->query($sql);
var_dump($st->fetchColumn(1), $st->columnCount(), $st->rowCount());
$st->fetchAll();
var_dump($st->rowCount(), $st->fetch());
?>
--EXPECT--
[1,"a","one"]
{"id":1,"0":1,"grp":"a","1":"a","name":"one","2":"one"}
{"id":1,"grp":"a","name":"one"}
Item
["one","two","three"]
{"1":"one","2":"two","3":"three"}
{"a":["one","two"],"b":["three"]}
{"1":{"grp":"a","name":"one"},"2":{"grp":"a","name":"two"},"3":{"grp":"b","name":"three"}}
["a1","a2","b3"]
["one","two","three"]
string(1) "1"
string(3) "one"
[1,2,3]
string(1) "a"
int(3)
int(1)
int(3)
bool(false)
