# Gemini

Gemini is an Application Server responsible for twinning VoIP clients with a mobile phone hosted on a native circuit-switched network.

Gemini routes terminating calls to any VoIP clients and the native device, with the exception of a VoIP client running on the native device (so it is not alerted twice by the same call).

It is implemented as a transaction-stateful but otherwise entirely-stateless AS, and builds on top of the AS infrastructure provided by Sprout.

## Building Gemini

Gemini is integrated into the Application Server framework provided by Sprout,
and can be run on the Sprout nodes or as a standalone node. It is built as part of the [Sprout build](https://github.com/Metaswitch/sprout/blob/dev/docs/Development.md).

## Gemini Configuration

Gemini is configured in the subscriber's IFCs, and is registered as a general terminating AS for INVITE and SUBSCRIBE requests.
It has an optional "twin-prefix" parameter - this specifies the prefix to apply to the user part of URIs to route to the mobile twin. The application server name should be set to `mobile-twinned` @ the cluster of nodes running the Gemini application servers (which will be the Sprout cluster, or a standalone application server cluster). 

An example IFC is:

```
<InitialFilterCriteria>
  <TriggerPoint>
  <ConditionTypeCNF>0</ConditionTypeCNF>
  <SPT>
    <ConditionNegated>0</ConditionNegated>
    <Group>0</Group>
    <SessionCase>1</SessionCase>
    <Extension></Extension>
  </SPT>
  <SPT>
    <ConditionNegated>0</ConditionNegated>
    <Group>0</Group>
    <Method>INVITE</Method>
    <Extension></Extension>
  </SPT>
  </TriggerPoint>
  <ApplicationServer><ServerName>sip:mobile-twinned@gemini.cw-ngv.com;twin-prefix=123</ServerName>
    <DefaultHandling>0</DefaultHandling>
  </ApplicationServer>
</InitialFilterCriteria>
```

This is triggered if the request is a terminating INVITE for a registered subscriber. The application server is gemini.cw-ngv.com, and the prefix that will be applied to the callee's URI to generate the mobile number is 123. 

To enable Gemini on a Sprout node or a standalone server, simply install the Gemini plugin and restart the sprout process.

## Scalability

Multiple Gemini nodes can be deployed using NAPTR/SRV load-balancing. Gemini is transaction-stateful but otherwise entirely stateless, so no clustering is required.
