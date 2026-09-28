--TEST--
pdo_lake: connection failures carry SQLSTATE classes
--EXTENSIONS--
pdo_lake
--SKIPIF--
<?php require __DIR__.'/skipif.inc'; ?>
--FILE--
<?php
require __DIR__.'/config.inc';

function attempt(string $dsn, ?string $user, ?string $password, array $options = []): void
{
    try {
        PDO::connect($dsn, $user, $password, $options);
        echo "connected\n";
    } catch (PDOException $e) {
        echo $e->errorInfo[0], ' ', $e->getCode() === $e->errorInfo[1] ? 'native' : 'mismatch', "\n";
    }
}

attempt(lake_dsn(), lake_user(), 'wrong-password');
attempt('lake:host=127.0.0.1;port=1;sslmode=disable', lake_user(), lake_password(), [PDO::ATTR_TIMEOUT => 2]);
attempt('lake:port=18000', lake_user(), lake_password());
attempt('lake:mysql://x', lake_user(), lake_password());
?>
--EXPECT--
28000 native
08001 native
08001 native
08001 native
