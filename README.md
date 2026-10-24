Tunlink3 is here!
Private use only.

This software is unstable/unfinished - if it does work at all.

# Why make a version 3?

There is a handful of improvements that rf24tunlink could use, and of course I am taking the challenge.

[x] Move to a Zero-Copy architecture (iovec - scatter/gather operations) : tunlink2 used to make at least one copy of the incoming data. Can do better.

[x] Move to a single-threaded EVENT-POLL loop structure : an attempt to avoid 100% CPU usage while getting faster response times.

[x] Refresh the HARQ algorithm not to wait for ACK/NACK timers.

[x] Tune timers based on the expected airtime of each frame (based on number of packets)

The current input implementation speed (TUN + HARQ first processing) is in the **gigabit range** on a Raspberry Pi. Plenty of processing power left for any radio we want to add.

[x] Implement a packet logger interface (and a webpage that reads CSV files)

[ ] Refine the HARQ algorithm to use a more efficient, unified and unmistakable packet structure

[ ] Refine the HARQ algorithm for interleaving (Complexity skyrocketed here, could be very hard with the current structure)

[ ] Reintroduce configurations

[ ] Simplify the Radio Module interface requirements.

[ ] Compress the TUN interface (remove headers and reintroduce later)

[ ] Or compress the network stream if necessary (ROHC, LZ4)

Probably more stuff will be added to this list as time goes on.

I almost lost these files, that's why I am publishing unfinished software. Tunlink2's performance can still be improved.

# I mean, why continue development, at all?

Tunlink2 already powers my RC buggy. And it includes remote controls, telemetry, onboard live video feed and head-tracking for the camera. I can somewhat drive it with a VR headset by using my bare hands as controllers. Which is already an enormous accomplishment if you ask me.

Though, there is still room for improvement, particularly in terms of stability and latency, which I want to sort with tunlink3. At the moment, seeing a 100% CPU usage does suggest there is a software bottleneck and the link could be faster. I would expect the maximum link speed to be around 1.6-1.7Mbps, while now tunlink2 is sitting at 1.2/1.3 Mbps.

Furthermore I also want to ensure future radio modules are handled even more easily by the software and are easier to include.

As you would have guessed at this point in time, the HARQ development and overall making the right design choices are some extremely complicated matters.

So here you go, tunlink3. We either get an awesome software, or an awesome life lesson.
