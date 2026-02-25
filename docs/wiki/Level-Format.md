# Level Format

Current reference parser: `tests/level_loader.c`.

## Top-level JSON shape

- `metadata`
  - `game_title` (string)
  - `level_name` (string)
  - `play_speed` (number)
- `platforms` (array)
- `spikes` (array)
- `jumppads` (array)

## Object fields

Each object may provide:

- `pos` or `position`
  - `x`, `y`, `z`
  - for platforms, parser also reads optional `width`, `height`, `length`
- `scale`
  - `x`, `y`, `z`
- `rot` or `rotation`
  - `x`, `y`, `z`

## Type mapping

- `platform` -> `TYPE_PLATFORM`
- `spike` -> `TYPE_SPIKE`
- `jumppad` -> `TYPE_JUMPPAD`
- unknown -> `TYPE_GENERIC`

## Runtime flow

1. `parseLevel` reads JSON into `LevelSpec`
2. `levelspec_to_objects` flattens arrays into `ObjectSpec[]`
3. `Tropic_loadObjects` creates runtime objects from the array

## Example asset

- `assets/levels/test_level.json`
