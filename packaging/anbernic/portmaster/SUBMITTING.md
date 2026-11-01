# Submitting to PortMaster

The files here, with `../port.json` and `../Commander Keen 4 Revamped.sh`, make the
`ports/keen4revamped/` folder of a pull request to
[PortMaster-New](https://github.com/PortsMaster/PortMaster-New), where the other Keen source
ports (Omnispeak, Chocolate Keen) are. `make-package.sh --portmaster DIR` puts it together
from an `anbernic` build; CI uploads it as the `keen4-portmaster` artifact.

## Still to add by hand

- `screenshot.png`: gameplay, not the title screen, 4:3 and at least 640x480, captured on a
  device (PortMaster's screenshot tool, or the firmware's). Put it in the folder and in
  `keen4revamped/` (`gameinfo.xml` points there). It shows the game's art, so it is not kept
  in this repository.
- `cover.png` (optional): gameplay with a logo or box art. Without it PortMaster shows the
  screenshot.

## Before the pull request

1. Open a testing thread in #testing-n-dev on the PortMaster Discord with the zip from CI.
2. Get it tested at 640x480 on AmberELEC, dArkOS, muOS, ROCKNIX and Knulli; 720x720 and
   480x320 are optional. Check the Select+Start exit and that the d-pad and buttons act once
   per press (gptokeyb must send no keys).
3. Fork PortMaster-New, add the folder as `ports/keen4revamped/`, and link the testing thread
   in the pull request.
