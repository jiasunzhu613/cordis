# Cordis - a toy Redis implementation in C++

> Mainly a project to learn more about TCP servers and Redis.

Cordis is a toy Redis implementation that is a RESP2 (Redis Serialization Protocol) compatible. 

### Developer quickstart
CMake is used to build Cordis.

In the project root directory:
1. Configure and prepare project for compilation: `cmake -S . -B build`
2. Build project: `cmake --build build`
3. OPTIONALLY run tests: `cd build && ctest`

CMake should generate executable for both the Cordis client and server under the `build/src` directories. 
To run server: `./build/src/server/server`
To run client: `./build/src/client/client`

### Roadmap (in order of priority)
- [ ] Support all RESP2 types
- [ ] Support Redis' ZSET commands (sorted set)