# DATUM Gateway
**Decentralized Alternative Templates for Universal Mining**
(c) 2024 Bitcoin Ocean, LLC & Jason Hughes

The DATUM Gateway implements lightweight efficient client side decentralized block template creation for pooled solo mining.  It facilitates communication with the pool, the local Bitcoin node creating block templates, and the mining hardware performing hash operations.  The pool is responsible for coordinating the block reward split based on work done for the pool by the miner, but does not create work for the miner.

The work provided by the gateway to mining hardware is generated only from the local node generating templates for the miner. The real miner is always whoever is running the Bitcoin node. With DATUM, that's not the pool. As the protocol is intended solely for mining of decentralized block templates, the DATUM protocol has no mechanisms for the pool providing the information needed to construct work or a block template.

Currently the DATUM Gateway supports communication with mining hardware using the Stratum v1 protocol with version rolling extensions (aka "ASICBoost").  Communication with the Bitcoin node is via RPC and must support GBT ("getblocktemplate").  Finally, communication with the pool is via the DATUM protocol.

**Using Bitcoin Knots is highly recommended**. This gives miners fine controls over how they wish to construct their block templates.  Other node implementations that support GBT can also be used.  This includes Bitcoin Core, but it is severely lacking in template control options.  That is unfortunately a centralizing force which partly defeats the purpose of decentralizing block template creation in the first place.

The DATUM Gateway only supports mining Bitcoin.  Modifying the code to support non-Bitcoin is not straightforward, as many optimizations and design considerations are tightly tied to Bitcoin-specific restraints for efficiency.

Finally, as a side effect of doing all work construction and distribution locally with your own node, the DATUM Gateway can also be used without a pool for **true** solo mining. Note that popular "solo mining pools" are **not actually solo mining**, since the pool is still doing the actual template construction. However, using the DATUM Gateway, you are truly creating your own blocks, and 100% of the block reward goes to you (no need to give a cut to some "solo pool").

## DATUM Protocol
The DATUM Gateway's communication with the mining pool is via the DATUM Protocol.  This is an encrypted communication link between the DATUM Gateway (client) and DATUM Prime (pool side).

The protocol itself was made from the ground up as a custom protocol.  Its specification is evolving, subject to change, and will be published elsewhere.

The core concepts of the protocol:

 - Encrypt communications between the Gateway and pool
 - Obfuscate the communications somewhat so a MITM is unable to glean useful or accurate insight into the miner's operation via analysis of the still-ciphered communications.
 - Retrieve proper generation transaction payout splits from the pool for locally constructed templates
 - Submit work with to the pool with sufficient data to efficiently validate and accept the work for proper rewards
 - Communicate minimal guardrails and requirements for a valid template to earn pooled rewards

With the current version of the protocol, the pool does block validation after coordinating with the miner. This is strictly to ensure miners are not accidentally creating invalid blocks while DATUM is still undergoing testing. In a future version of the protocol, the pool will not be in charge of this function and will be almost completely blinded to the contents of the miner's block template.

The protocol is not specific to a pooled reward system, as the Gateway coordinates the appropriate generation transaction with the pool.  However, in the spirit of maximum decentralization, the pool should implement rewarding miners directly from generated payouts, such as with OCEAN's TIDES reward system.

## Requirements

 - 64-bit AMD or Intel system. Other systems may work, but at this time it is at your own risk.
 - Linux-based operating system. Other OSs will be supported in the future.
 - Bitcoin full node ([Bitcoin Knots](https://bitcoinknots.org/) recommended) fully synced with the Bitcoin network.
 - Fast storage recommended for the Bitcoin node.
 - Stable internet connection for both the Bitcoin node and Gateway's communication with pool.
 - CPU powerful enough to run the Bitcoin node without validation delays.
 - Approximately 1GB/RAM, plus 1GB/RAM per 1000 Stratum clients, plus Bitcoin node RAM requirements.
 - Bitcoin mining hardware able to reach the system running the DATUM Gateway.

This list is not extensive, but the main goal is the have a stable system for your Bitcoin node and the Gateway such that your node is processing new incoming blocks and getting templates to the Gateway as quickly as possible.  While this may all work on relatively low end hardware, your mileage may vary.

## Node Configuation
Your Bitcoin node must be configured to construct blocks as you desire.  Bitcoin Knots provides many options for configuring your node's policy and is highly recommended.

At this time, you must also reserve some block space for the pool's generation transaction.  The following options are currently recommended:

    blockmaxsize=3985000
    blockmaxweight=3985000

Note: This reservation requirement will be removed for Bitcoin Knots users in a future version of the DATUM Gateway thanks to support for on-the-fly specification of these metrics by the client in Knots (as of version 27.1).

You must also configuration a "*blocknotify*" setting to alert the Gateway of new blocks.  See Installation section.

Finally, the Gateway must have RPC access to your node, and you must add an RPC user to your configuration to facilitate this, as well as ensuring the service running the Gateway is whitelisted for RPC access (if not on the same machine).

Some additional recommendations:

    maxmempool=1000
    blockreconstructionextratxn=1000000

As a true miner, you'll most likely want as many valid transactions that meet your node policy in your mempool for mining.

## Installation
Install and fully sync your Bitcoin full node. Instructions for this are beyond the scope of this document.

Configure your node to create block templates as you desire. Be sure to reserve some space for the generation transaction, otherwise your work will not be able to fit a reward split.  See node configuration recommendations above.

Install the required libraries and development packages for dependencies: cmake, jansson, libmicrohttpd, libsodium, and libcurl.

Compile DATUM by running:

    cmake . && make

Run the datum_gateway executable with the -? flag for detailed configuration information, descriptions, and required options.  Then construct a configuration file (defaults to "datum_gateway_config.json" in the current working directory). Be sure to also set your coinbase tags.  The primary tag setting is unused in pooled mining, however the secondary tag is intended to show on things like block explorers when you mine a block.

To avoid mining stale work, you will need to ensure the DATUM Gateway receives new block notifications from your node. It is suggested you run the DATUM Gateway as the same user as your full node and utilize the following configuration line in your bitcoin.conf:

    blocknotify=killall -USR1 datum_gateway

Ensure you have "killall" installed on your system (*psmisc* package on Debian-like OSs).

If the node and Gateway are on different systems, you may need to utilize the "NOTIFY" endpoint on the Gateway's dashboard/API instead.

## Template/Share Requirements for Pooled Mining

 - Must be a valid block and conform to current Bitcoin consensus rules
 - Submitted work must be for the current latest block height, valid time, etc
 - Must include generation transaction outputs provided by the pool in the order provided
 - Must include the primary coinbase tag as provided by the pool
 - Must include the unique identifier provided by the pool
 - Must include the work target and meet/exceed that target
 - Any additional requirements by pool documentation

## Notes/Known Issues/Limitations

 - By default, the Gateway fails over to non-pooled mining when the connection with the pool is lost or disabled
 - Accepted/rejected share counts on mining hardware may not match with the pool, and the delta vary more depending on the Gateway's configuration. This is because shares are first accepted or rejected as valid for your local template based on your local node, and then again accepted or rejected based on the pool's requirements, latency to the pool (stale work), latency between your node and the network (stale work), etc.  Stratum v1 has no mechanism to report back to the miner that previously accepted work is now rejected, and it doesn't make sense to wait for the pool before responding, either.

More. TODO.

## License

The DATUM Gateway (including the DATUM Protocol) is free open source software and released under the terms of the MIT license.  See LICENSE.

