--TEST--
pdo_lake: positional, named and repeated placeholders with every parameter type round-trip exactly
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

$tricky = "q'uo\"te \\back\\slash \\' \0nul ?mark :name \u{4E2D}\u{6587} \u{1F600}";
$binary = "\x00\x01\xff'\\\x80";

foreach ([true, false] as $emulate) {
    $db = lake_connect([PDO::ATTR_EMULATE_PREPARES => $emulate]);
    $db->exec('CREATE OR REPLACE TABLE t_bind (i BIGINT NULL, f DOUBLE NULL, b BOOLEAN NULL, s STRING NULL, raw BINARY NULL)');

    $insert = $db->prepare('INSERT INTO t_bind VALUES (?, ?, ?, ?, ?)');
    $insert->bindValue(1, PHP_INT_MAX, PDO::PARAM_INT);
    $insert->bindValue(2, 0.1);
    $insert->bindValue(3, false, PDO::PARAM_BOOL);
    $insert->bindValue(4, $tricky);
    $insert->bindValue(5, $binary, PDO::PARAM_LOB);
    $insert->execute();
    $insert->execute([null, null, null, null, null]);

    $select = $db->prepare("SELECT i, f, b, s, raw, '?' AS literal FROM t_bind WHERE s = :s AND s = :s AND i = :i -- trailing ? comment");
    $select->execute(['s' => $tricky, 'i' => PHP_INT_MAX]);
    $row = $select->fetch(PDO::FETCH_ASSOC);
    var_dump($row['i'] === PHP_INT_MAX, $row['f'] === 0.1, $row['b'], $row['s'] === $tricky, $row['raw'] === $binary, $row['literal']);

    $by_ref = $db->prepare('SELECT count(*) FROM t_bind WHERE i IS NULL AND s IS NULL AND raw IS NULL');
    $by_ref->execute();
    var_dump($by_ref->fetchColumn());

    $value = 'first';
    $param = $db->prepare('SELECT :v');
    $param->bindParam('v', $value);
    $param->execute();
    $first = $param->fetchColumn();
    $value = 'second';
    $param->execute();
    var_dump($first, $param->fetchColumn());

    try {
        $db->prepare('SELECT ?, :named')->execute([1]);
    } catch (PDOException $e) {
        echo $e->getCode(), "\n";
    }
}
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
string(1) "?"
int(1)
string(5) "first"
string(6) "second"
HY093
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
string(1) "?"
int(1)
string(5) "first"
string(6) "second"
HY093
