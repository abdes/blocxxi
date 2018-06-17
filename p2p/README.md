# Important reference material

  - [The Kademlia paper by Maymounkov and Mazieres](https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf)
  - [Wikipedia page on Kademlia](https://en.wikipedia.org/wiki/Kademlia)

# Distance : the Kademlia Metric
Kademlia's operations are based upon the use of exclusive OR, XOR, as a metric.
The distance between any two keys or nodeIDs x and y is defined as

    distance(x, y) = x ^ y

where ^ represents the XOR operator. The result is obtained by taking the
byte-wise exclusive OR of each byte of the operands.

**Note**
> Kademlia follows Pastry in interpreting keys (including nodeIDs) as big-endian
> numbers. This means that the low order byte in the byte array representing the
> key is the most significant byte and so if two keys are close together then
> the low order bytes in the distance array will be zero.

# The K-Bucket
A Kademlia node organizes its contacts, other nodes known to it, in buckets
which hold a maximum of k contacts. These are known as k-buckets.
The buckets are organized by the distance between the node and the contacts in
the bucket. Specifically, for bucket j, where 0 <= j < k, we are guaranteed that

      2^j <= distance(node, contact) < 2^(j+1)

As per the optimizations suggested in the paper, we do not implement a bucket
per bit. Instead the routing table starts with a single initial bucket covering
the entire address space. As buckets fill up, they get split int two halves and
nodes in the original bucket get distributed between them.

The last bucket is always the bucket that contains the ID of the routing node.

# Node Lookup
This section describes the algorithm that Kademlia uses for locating the k nodes
nearest to a key. It must be understood that these are not necessarily closest
in a strict sense. Also, the algorithm is iterative although the paper describes
it as recursive.

The search begins by selecting alpha contacts from the non-empty k-bucket
closest to the bucket appropriate to the key being searched on. If there are
fewer than alpha contacts in that bucket, contacts are selected from other
buckets. The contact closest to the target key, closestNode, is noted.

**Note**
> The criteria for selecting the contacts within the closest bucket are not
> specified. Where there are fewer than alpha contacts within that bucket and
> contacts are obtained from other buckets, there are no rules for selecting the
> other buckets or which contacts are to be used from such buckets.

The first alpha contacts selected are used to create a shortlist for the search.
The node then sends parallel, asynchronous FIND_* RPCs to the alpha contacts in
the shortlist. Each contact, if it is live, should normally return k triples. If
any of the alpha contacts fails to reply, it is removed from the shortlist, at
least temporarily. The node then fills the shortlist with contacts from the
replies received. These are those closest to the target. From the shortlist it
selects another alpha contacts. The only condition for this selection is that
they have not already been contacted. Once again a FIND_* RPC is sent to each in
parallel.

Each such parallel search updates closestNode, the closest node seen so far.
The sequence of parallel searches is continued until either no node in the sets
returned is closer than the closest node already seen or the initiating node has
accumulated k probed and known to be active contacts.

If a cycle doesn't find a closer node, if closestNode is unchanged, then the
initiating node sends a FIND_* RPC to each of the k closest nodes that it has
not already queried.

At the end of this process, the node will have accumulated a set of k active
contacts or (if the RPC was FIND_VALUE) may have found a data value. Either a
set of triples or the value is returned to the caller.

# Store
This is the Kademlia store operation. The initiating node does a FindNode,
collecting a set of k closest contacts, and then sends a primitive STORE RPC to
each. Store operations are used for publishing or replicating data on a Kademlia
network.

# FindNode
This is the basic Kademlia node lookup operation. As described above, the
initiating node builds a list of k "closest" contacts using iterative node
lookup and the FIND_NODE RPC. The list is returned to the caller.

# FindValue
This is the Kademlia search operation. It is conducted as a node lookup, and so
builds a list of k closest contacts. However, this is done using the FIND_VALUE
RPC instead of the FIND_NODE RPC. If at any time during the node lookup the
value is returned instead of a set of contacts, the search is abandoned and the
value is returned. Otherwise, if no value has been found, the list of k closest
contacts is returned to the caller.
When an iterativeFindValue succeeds, the initiator must store the key/value pair
at the closest node seen which did not return the value.

# Refresh
If no node lookups have been performed in any given bucket's range for tRefresh
(an hour in basic Kademlia), the node selects a random number in that range and
does a refresh, an iterativeFindNode using that number as key.

# Bootstrap
A node joins the network as follows:
 - if it does not already have a nodeID n, it generates one
 - it inserts the value of some known node c into the appropriate bucket as its
   first contact
 - it does an iterativeFindNode for n
 - it refreshes all buckets further away than its closest neighbor, which will be
   in the occupied bucket with the lowest index.
 - If the node saved a list of good contacts and used one of these as the "known
   node" it would be consistent with this protocol.


# Replication Rules (TODO)
Data is stored using Store, which has the effect of replicating it over the k
nodes closest to the key.
Each node republishes each key/value pair that it contains every hour. The
republishing node must not be seen as the original publisher of the key/value
pair.

The original publisher of a key/value pair republishes it every 24 hours.

When an FindValue succeeds, the initiator must store the key/value pair at the
closest node seen which did not return the value.

## Expiration Rules (TODO)
All key/value pairs expire 24 hours after the original publication.

All key/value pairs are assigned an expiration time which is "exponentially
inversely proportional to the number of nodes between the current node and the
node whose ID is closest to the key", where this number is "inferred from the
bucket structure of the current node".

The writer would calculate the expiration time when the key/value pair is stored
using something similar to the following:

```
find the index j of the bucket corresponding to the key
count the total number of contacts Ca in buckets 0..j-1
count the number of contacts Cb in bucket j closer than the key
if C = Ca + Cb, then the interval to the expiration time is
24 hours if C > k
24h * exp( k / C ) otherwise
```

**Note**
> The requirement that data expires one day after the original publication date
> is more than ambiguous and would seem to mean that no data can ever be
> republished.

In any case, the system is required to mark the stored key/value pair with an
original publication timestamp. If this is to be accurate, the timestamp must
be set by the publisher, which means that clocks must be at least loosely
synchronized across the network.

It would seem sensible to mark the key/value pair with a time to live (TTL) from
the arrival of the data, tExpire (one day) or a fraction thereof.

# Implementation Notes
## Node
It would be useful to add to the Contact data structure an RTT (round trip time)
value or a set of such values. The round trip time (RTT) to the contact could be
as measured using a PING RPC or using a conventional Internet network ping.

## The Sybil Attack
A paper by John Douceur, douceur02, describes a network attack in which
attackers select nodeIDs whose values enable them to position themselves in the
network in patterns optimal for disrupting operations. For example, to remove a
data item from the network, attackers might cluster around its key, accept any
attempts to store the key/value pair, but never return the value when presented
with the key.

A Sybil variation is the Spartacus attack, where an attacker joins the network
claiming to have the same nodeID as another member. As specified, Kademlia has
no defense. In particular, a long-lived node can always steal a short-lived
node's nodeID.

Douceur's solution is a requirement that all nodes get their nodeIDs from a
central server which is responsible at least for making sure that the
distribution of nodeIDs is even.

A weaker solution would be to require that nodeIDs be derived from the node's
network address or some other quasi-unique value.
