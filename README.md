# Tropic Engine

## Requirements
<a href="https://github.com/ProgrammingCube/c_vector" target="_blank">CVector</a>  
<a href="https://github.com/DaveGamble/cJSON" target="_blank">cJSON</a>

Tropic Engine is an extremely simple game engine in C that utilizes a few C tools and OpenGL 3.3+.  
The goal of Tropic Engine is rapid development of extremely simple 3D graphic shapes, allowing the game programmer to focus on gameplay mechanics and shaders totally separately.

This isn't a full-on engine like Unity where there is scripting, texturing, shading, etc. This is to get objects on your screen and have hooks to allow you to move them around.


# Functions
`Tropic_init( Tropic* engine )`  
`Tropic_parseLevel( Tropic* engine, const char* level_path )`  
`Tropic_newObject( Tropic* engine, const Object* obj )`
`Tropic_cleanup( &engine )`  
`Tropic_loadObjects()`