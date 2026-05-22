# TR-181 ChannelSelectionRequest Changes

## Overview

This document describes the recent changes to `cmd_channelselect` in `unified-wifi-mesh/src/ctrl/dm_easy_mesh_ctrl.cpp`.

The handler now strictly enforces the nested TR-181 DataElements form for `ChannelSelectionRequest()` and produces a `SetAnticipatedChannelPreference` JSON payload with:

- `Network.ID`
- `DeviceList`
- `RadioList`
- `AnticipatedChannelPreference` inside each radio
- `Class`, `ChannelList`, and `ChannelPrefList` inside each anticipated channel preference entry

Flat fallback parameters such as `OpClass`, `ChannelList`, and `ChannelPrefList` are no longer accepted.

## Expected TR-181 Input Format

The input must use nested `Class.N.*` parameters. Example requests for two radios:

### Radio 1
- `Device.WiFi.Radio.1.ChannelSelectionRequest()`
- Input parameters:
  - `Class.0.OpClass = 115`
  - `Class.0.Channel.0.Channel = 36`
  - `Class.0.Channel.0.Preference = 1`
  - `Class.0.Channel.1.Channel = 40`
  - `Class.0.Channel.1.Preference = 2`
  - `Class.1.OpClass = 118`
  - `Class.1.Channel.0.Channel = 149`
  - `Class.1.Channel.0.Preference = 1`

### Radio 2
- `Device.WiFi.Radio.2.ChannelSelectionRequest()`
- Input parameters:
  - `Class.0.OpClass = 115`
  - `Class.0.Channel.0.Channel = 6`
  - `Class.0.Channel.0.Preference = 1`
  - `Class.0.Channel.1.Channel = 11`
  - `Class.0.Channel.1.Preference = 2`
  - `Class.1.OpClass = 124`
  - `Class.1.Channel.0.Channel = 149`
  - `Class.1.Channel.0.Preference = 1`

## Output JSON Format

The generated JSON payload is built as `wfa-dataelements:SetAnticipatedChannelPreference` and includes a `RadioList` entry for each radio.

### Example output for one radio

```json
{
  "wfa-dataelements:SetAnticipatedChannelPreference": {
    "Network": {
      "ID": "GLOBAL_NET_ID",
      "DeviceList": [
        {
          "ID": "00:11:22:33:44:55",
          "RadioList": [
            {
              "ID": "00:11:22:33:44:66",
              "AnticipatedChannelPreference": [
                {
                  "Class": 115,
                  "ChannelList": [36, 40],
                  "ChannelPrefList": [1, 2]
                },
                {
                  "Class": 118,
                  "ChannelList": [149],
                  "ChannelPrefList": [1]
                }
              ]
            }
          ]
        }
      ]
    }
  }
}
```

### Conceptual multi-radio JSON

If multiple radio entries were combined under the same device, the `RadioList` would contain multiple objects:

```json
{
  "wfa-dataelements:SetAnticipatedChannelPreference": {
    "Network": {
      "ID": "GLOBAL_NET_ID",
      "DeviceList": [
        {
          "ID": "00:11:22:33:44:55",
          "RadioList": [
            {
              "ID": "00:11:22:33:44:66",
              "AnticipatedChannelPreference": [
                {
                  "Class": 115,
                  "ChannelList": [36, 40],
                  "ChannelPrefList": [1, 2]
                }
              ]
            },
            {
              "ID": "00:11:22:33:44:77",
              "AnticipatedChannelPreference": [
                {
                  "Class": 115,
                  "ChannelList": [6, 11],
                  "ChannelPrefList": [1, 2]
                },
                {
                  "Class": 124,
                  "ChannelList": [149],
                  "ChannelPrefList": [1]
                }
              ]
            }
          ]
        }
      ]
    }
  }
}
```

## Notes
- The handler is invoked per-radio, so each call to `ChannelSelectionRequest()` maps to a single radio entry.
