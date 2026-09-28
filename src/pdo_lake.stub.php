<?php

/** @generate-class-entries */

namespace Pdo;

/**
 * @strict-properties
 * @not-serializable
 */
class Lake extends \PDO
{
    /** @cvalue PDO_LAKE_ATTR_WAREHOUSE */
    public const int ATTR_WAREHOUSE = UNKNOWN;

    /** @cvalue PDO_LAKE_ATTR_ROLE */
    public const int ATTR_ROLE = UNKNOWN;

    /** @cvalue PDO_LAKE_ATTR_DATABASE */
    public const int ATTR_DATABASE = UNKNOWN;

    /** @cvalue PDO_LAKE_ATTR_SERVER_SIDE_PARAMS */
    public const int ATTR_SERVER_SIDE_PARAMS = UNKNOWN;

    /** @cvalue PDO_LAKE_ATTR_QUERY_ID */
    public const int ATTR_QUERY_ID = UNKNOWN;

    /** @cvalue PDO_LAKE_ATTR_QUERY_STATS */
    public const int ATTR_QUERY_STATS = UNKNOWN;

    /** @cvalue LAKE_LOAD_STAGE */
    public const int LOAD_STAGE = UNKNOWN;

    /** @cvalue LAKE_LOAD_STREAMING */
    public const int LOAD_STREAMING = UNKNOWN;

    public function lastQueryId(): ?string {}

    public function killQuery(string $queryId): bool {}

    public function info(): array|false {}

    public function useWarehouse(string $warehouse): bool {}

    public function useDatabase(string $database): bool {}

    public function useRole(string $role): bool {}

    public function setSessionSetting(string $name, string $value): bool {}

    public function presign(string $operation, string $stage): array|false {}

    public function uploadToStage(string $stage, string $data): bool {}

    public function uploadFileToStage(string $stage, string $filename): bool {}

    public function loadData(string $sql, string $data, int $method = Lake::LOAD_STAGE): array|false {}

    public function loadFile(string $sql, string $filename, int $method = Lake::LOAD_STAGE, ?array $fileFormatOptions = null, ?array $copyOptions = null): array|false {}

    public function streamLoad(string $sql, array $rows, int $method = Lake::LOAD_STREAMING): array|false {}

    public function putFiles(string $localFiles, string $stage): array|false {}

    public function getFiles(string $stage, string $localDirectory): array|false {}
}
