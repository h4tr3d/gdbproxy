# Testin

Smoke testing for proxy implementation.

From terminal #1 run:

```bash
while true; do gdbserver localhost:3002 /bin/cat; done
```

Run `gdbserver` on terminal #2

On terminal #3 run client program:

```bash
gdb -ex 'target remote localhost:4002' -ex 'c' --args /bin/cat
```