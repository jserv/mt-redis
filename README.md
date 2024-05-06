# mt-redis
A multi-thread Redis implementation with Read-Copy-Update(RCU) support to achieve high performance. 
1. Using event-loop per thread I/O model.
2. Support scheduling request operation between threads.
3. RCU can support lock-free sharing between 1 writer thread and multiple reader threads to boost read operation performence. 
4. Can achieve over 1 millions op/s powered by a normal commercial server.
