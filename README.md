# minimdns
Extremely simple MDNS server for Linux. 

TODO: Flesh out readme and document code.


## Reasons I hate avahi
 * Takes up ___ % CPU
 * Takes up ___ MB
 * It needs a chroot helper.
 * Default installation needs it's own whole user
 * It's ___ LOC
 * Hides that it's listening on port 5353
 * Difficult to disable
 * It doens't even follow the multicast rules, and binds to 0.0.0.0:5353 / :::5353

## Also, wow IPv6 is a mess

I learned a lot about IPv6 when writing this.

Holy cow...
https://superuser.com/questions/1086954/how-do-i-use-mdns-for-ssh-6
ping6 is all over the place.

