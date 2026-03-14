# Copilot Instructions

## Project Guidelines
- The engine design should support both camera spin and world spin. World spin should work on all three axes, initially in 90-degree increments, with possible support for arbitrary angles later.
- Gameplay should not hard-code wall/ground/ceiling. Use a separate gravity flip function that changes the gravity vector independently of world spin, so an upside-down world can still cause the player to fall if no supporting surface exists. Left/right/jump directions should be derived from the current gravity and orientation.
- Prefer keeping game-specific jump behavior out of the engine core by moving Tropic_jumpObject-like logic into the game layer, such as engine_test/player code.