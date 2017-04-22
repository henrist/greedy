# Greedy TCP

This is a simple client/server which attempts to always have data in the
Linux TCP stack available to dequeue to the network. It basicly tries to
fill the TCP window at all times.

This tool has primarily been made because using SSH/SCP and iperf didn't
yield stable enough results, so we got inaccurate results when testing an
active queue manager or the TCP congestion control algorithm, because
the client/server influenced the data rate (e.g. SSH has it's own window
implementation on top of TCP's).

## Usage

```
make
./greedy # it will show you a help message
```

Example:

```
# on server
./greedy -v -s 8888

# on client
./greedy -v server-host 8888
```

The server will now send data to the client.

## Changing buffer sizes in Linux

The TCP stack limits the number of bytes that can be queued in the different
layers, and also limits the TCP window size.

- `net.ipv4.tcp_wmem` limits the sender window and the amount of data the
  kernel is buffering (e.g. sent from the application, but not ACKed by the
  receiver).
- `net.ipv4.tcp_rmem` limits the receive window. A sender can never send more
  than the receiver has announced it can buffer.

My testing, while disabling tso (tcp segmentation offload) and gro (generic
receive offload), has given a few observations:

- `tcp_rmem` has to be double of the maximum receiver window * mss
- `tcp_wmem` has to be tripple of the maximum packets in flight * mss

On my Ubuntu 16.10, the defaults values are:

```
net.ipv4.tcp_rmem = 4096        87380   6291456
net.ipv4.tcp_wmem = 4096        16384   4194304
```

Which gives:

```
wmem=4194304: maximum tcp window of 965 (4194304 / 1448 / 3)
rmem=6921456: maximum tcp window of 4780 (6921456 / 1448)
```

So if this is the same for the receiver, the window will stop increasing
at 965.

To get a higher window, you will need to increase wmem (and possibly rmem
depending how high you want to go). I've successfully run experiments with
a window highter than 100 000 packets.

E.g. to be able to get a window of 2000 packets:

```
sysctl -w net.ipv4.tcp_rmem='4096 87380 5792000'
sysctl -w net.ipv4.tcp_wmem='4096 16384 8688000'
```
