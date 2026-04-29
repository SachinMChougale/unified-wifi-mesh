# Channel Selection Status Handling

This document describes the channel selection state transition changes introduced by commit `c7081d64e910ec3d5e1dbc77a363d43fcec0e586`.

## Overview

The commit adds handling for OneWiFi channel selection status messages, including a new status subdoc format that reports `Success` or `Error`, and documents the expected state machine path in the channel selection workflow.

### Files changed

- `src/agent/dm_easy_mesh_agent.cpp`
  - Added comments describing expected state transitions for channel selection and status callbacks.
- `src/em/channel/em_channel.cpp`
  - Documented state transitions for `em_state_agent_channel_sel_req_rcvd` and `em_state_agent_channel_sel_resp_sent`.
- `src/em/em.cpp`
  - Documented that `em_cmd_type_op_channel_sel_req` does not perform a generic EM state transition.

## Channel selection state machine

The intended state transition flow is:

1. `em_state_agent_channel_sel_req_rcvd`
   - Set when a Channel Selection Request is received by the agent.
   - The agent is now waiting for the OneWiFi status callback for `SubDocName` "ChannelSelection".

2. `em_state_agent_channel_sel_resp_sent`
   - Set after the agent sends a Channel Selection Response message in reaction to the status callback.
   - The response may be either `accept` or `decline` based on the OneWiFi status.

3. `em_state_agent_channel_report_pending`
   - Set when the corresponding Radio subdoc callback is received for the same band after the response is sent.
   - This signals the agent to generate an Operating Channel Report for the updated radio channel.

4. Timeout/cancel recovery path
   - If the channel selection status callback is not received within the expected window, or if the flow is canceled by orchestration,
     the pending channel selection must be aborted.
   - In that case, the agent should emit an Operating Channel Report with the current/existing channel information
     instead of a new channel assignment.
   - This forces the system back into a stable configured state and avoids leaving the EM object in a pending selection state.

### Expected event->state sequence

- `em_channel_t::handle_channel_sel_req()` receives a channel selection request and transitions the radio EM object to `em_state_agent_channel_sel_req_rcvd`.
- `dm_easy_mesh_agent_t::analyze_onewifi_status_cb()` processes the OneWiFi status callback.
  - OneWiFi now sends a channel selection status subdoc containing `SubDocName == "ChannelSelection"` and `Status == "Success"` or `Status == "Error"`.
  - The status subdoc may also include radio identification fields such as `SubDocRadioName` and, on error, an `ErrorDescription` string.
  - If `Status == "Success"`, the agent sends an accept response.
  - If `Status == "Error"`, the agent sends a decline response.
- Example OneWiFi status subdoc payload:

```json
{
  "SubDocName": "ChannelSelection",
  "Status": "Success",
  "SubDocRadioName": "radio_5G"
}
```

or

```json
{
  "SubDocName": "ChannelSelection",
  "Status": "Error",
  "ErrorDescription": "Radio busy or invalid channel preference",
  "SubDocRadioName": "radio_2.4G"
}
```

- `em_channel_t::send_channel_sel_response_msg()` sets `em_state_agent_channel_sel_resp_sent` after successful transmission.
- `dm_easy_mesh_agent_t::analyze_onewifi_radio_cb()` sees the Radio subdoc callback for the same radio band and moves the EM object to `em_state_agent_channel_report_pending`.
- If no status callback arrives and a timeout is detected, or if orchestration cancels the request, the agent should transition out of the selection flow and send OCR for the existing channel.

## Diagram

```
Channel Selection Request
        |
        v
em_state_agent_channel_sel_req_rcvd
        |
        v
Start orchestration of command em_cmd_type_op_channel_sel_req
   |    |
   |    |  OneWiFi status callback for SubDocName="ChannelSelection"
   |    v
   | em_state_agent_channel_sel_resp_sent
   |    |
   |    |  Radio subdoc callback for the same band
   |    v
   | em_state_agent_channel_report_pending
   |    |
   |    v
   | Send Operating Channel Report
   |    |
   |    v
   | em_state_agent_configured
   |
   +-- Timeout or cancel recovery path
        |
        v
   Operating Channel Report (existing channel)
        |
        v
    em_state_agent_configured
```

## Timeout and cancel scenario

- The active channel selection flow may time out if the OneWiFi status callback is not received in time.
- In this case, the agent should cancel the pending channel selection and restore the radio to a stable state.
- The cancel path may also be triggered by orchestration logic via `pre_process_cancel()` in `em_orch_agent.cpp`.
- When canceling, the agent should send an Operating Channel Report with the current/existing channel information rather than new channel data.
- This allows the controller to recover cleanly and ensures the EM object leaves the selection flow.
