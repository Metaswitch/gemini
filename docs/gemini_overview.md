# Gemini

Gemini is an Application Server responsible for twinning VoIP clients with a mobile phone hosted on a native circuit-switched network.

It is implemented as a transaction-stateful but otherwise entirely-stateless AS, and builds on top of the AS infrastructure provided by Sprout.

Gemini routes terminating calls to any VoIP clients and the native device, with the exception of a VoIP client running on the native device (so it is not alerted twice by the same call).

## Configuration

Gemini is configured in the subscriber's IFCs, and is registered as a general terminating AS for INVITE and SUBSCRIBE requests.
It has a mandatory "twin-prefix" parameter - this specifies the prefix to apply to the user part of URIs to route to the mobile twin.

## Scalability

Multiple gemini nodes can be deployed using NAPTR/SRV load-balancing. Gemini is transaction-stateful but otherwise entirely stateless, so no clustering is required.
