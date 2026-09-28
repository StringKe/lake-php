<?php

const LAKE_TEST_DATABASE = 'pdo_lake_test';

function lake_dsn(): string
{
    return getenv('PDO_LAKE_TEST_DSN') ?: 'lake:host=127.0.0.1;port=8000;sslmode=disable';
}

function lake_flight_dsn(): string
{
    return getenv('PDO_LAKE_TEST_FLIGHT_DSN') ?: 'lake:host=127.0.0.1;port=8900;protocol=flight;sslmode=disable';
}

function lake_user(): string
{
    return getenv('PDO_LAKE_TEST_USER') ?: 'lake';
}

function lake_password(): string
{
    return getenv('PDO_LAKE_TEST_PASSWORD') ?: 'lake';
}

function lake_connect(array $options = [], ?string $dsn = null): Pdo\Lake
{
    $db = PDO::connect($dsn ?? lake_dsn(), lake_user(), lake_password(), $options + [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
    $db->exec('CREATE DATABASE IF NOT EXISTS '.LAKE_TEST_DATABASE);
    $db->exec('USE '.LAKE_TEST_DATABASE);
    return $db;
}
