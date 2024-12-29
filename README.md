# Various esphome components

## ACL
A simple access control list for esphome. Requires sd card usage (via sdmmc component).

Injects itself into web server as a handler and allows pulling/pushing acl config and pulling logs.

### Fetch ACL
`curl http://<host>/acl/acl.json`

*Push ACL*
`curl -d @acl.json -H -H "Content-Type: application/json" http://<host>/acl/acl.json`

### Fetch logs for specific day
`curl http://<host>/acl/logs/yyyy-mm-dd.log`

### Fetch latest logs
`curl http://<host>/acl/logs/latest.log`

### TODO
 * Allow to use without SD card
 * Log cleanup over time


## SDMMC
Impl of sd card filesystem operations for ESP-IDF

## A7670
An implementation to talk to A7670X (possibly others as well) modems for esphome that is heavily based on sim800l component, but solves some problems

* Allows it to work on lilygo's t-eth-elite + t-eth-elite-lte shield as these need to wake modem up using a power pin
* Supports setting pin code
* Incoming and outgoing SMS messages are processed in PDU mode which allows full unicode support (courtesy of https://github.com/mgaman/PDUlib)
* Fixes stability issues caused by modem responses coming asynchronously
* Other minor imporovements
