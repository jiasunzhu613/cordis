# Cordis - a toy Redis implementation in C++

> Mainly a project to learn more about TCP servers and Redis.

Cordis is a toy Redis implementation that is a RESP2 (Redis Serialization Protocol) compatible. 

### Developer quickstart
1. Build all executables: `make all`
2. Start Cordis server: `./main.out`
3. Test with sample client calls: `./client.out`

### Roadmap (in order of priority)
- [ ] Support all RESP2 types
- [ ] Support Redis' ZSET commands (sorted set)