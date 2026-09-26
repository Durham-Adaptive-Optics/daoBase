# Network SHMs: examples

Every dao SHM of the network, opened by its name. One small service per machine
(`daoShmNetd`); nothing else to set up. Full documentation: daoBase docs,
"Network SHMs" (`docs/source/shm_net.rst`).

## One machine, one minute: `demo.py`

```bash
python demo.py
```

It starts two services on this machine as two machines ("camera" and "display"),
writes a moving spot at 100 Hz on "camera", opens it on "display" by its name with a
plain `daoShm.shm(...)`, reads 2 s of frames woken by the semaphore, writes a
command back, prints what happens, and removes everything. From the repository
(daoBase not installed):

```bash
cd daoBase && waf build
PYTHONPATH=src/python LD_LIBRARY_PATH=build/src python examples/shmNet/demo.py
```

## Two machines

On **each** machine, once:

```bash
daoShmNet.py start            # the service, in the background (daoShmNet.py stop)
```

On the machine that **owns** the SHM:

```bash
python writer.py --rate 100   # /tmp/netDemo.im.shm, and /tmp/netDemoCmd.im.shm for commands
```

On **any other** machine:

```bash
daoShmNet.py ls               # every SHM of the network, and where it is
python reader.py              # opens /tmp/netDemo.im.shm: the same line as a local SHM
python reader.py --send 1.5 -0.5 0.25 0     # a command back: printed by writer.py
```

or from C (nothing network-specific in the code):

```bash
cc reader.c -o reader -I$DAOROOT/include -L$DAOROOT/lib -ldao -Wl,-rpath,$DAOROOT/lib
./reader /tmp/netDemo.im.shm
```

`reader.py` prints the frames per second, their age (the write there to ready here:
it needs the two machines' clocks in sync, NTP or better PTP), the frames skipped
(a reader slower than the writer always gets the newest) and the link.

If `daoShmNet.py ls` shows nothing of the other machine:

- the same domain on both (`daoShmNet.py start --domain myInstrument`, default `dao`);
- multicast allowed on that network, else name the other machine:
  `daoShmNet.py start --peer otherMachine`;
- the ports open in the firewalls: TCP 7710 (data), UDP 7711 (discovery).

## Files

| file | what |
|---|---|
| `demo.py` | the whole thing on one machine, cleaned up after |
| `writer.py` | the owner: an image written at a rate, and a command SHM |
| `reader.py` | any other machine: the image opened by name, a command sent back |
| `reader.c` | the same from C: `daoShmOpen` + `daoShmWaitSemTimeout`, nothing else |
