# SyLog note

This document follows the pre-release type/policy API:

- severity classes derive from `sylog::severity_base`;
- delivery policies derive from `sylog::delivery::strategy_base`;
- rotation modes derive from `sylog::rotation::mode_base`;
- routes are configured with natural-language methods such as `send_to()`, `filtered_by()`, `block_when_full()`, and `drop_when_full()`.

See `docs/api/public_api.md` for the current public API.
