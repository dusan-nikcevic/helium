# Zephyr

Zephyr is a musical workspace for recording, composing and mixing with transparent
AI edits. These terms describe the product's musical objects.

## Language

**Project:** A saved workspace containing media, arrangement, launcher, channels,
routing and tempo information.

**Track:** A musical object that records or plays audio or MIDI through a channel.
_Avoid_: Mixer channel when referring to the musical object.

**Channel:** A controllable signal path shown as a mixer strip.

**Bus:** A channel that combines signals from other channels.

**Return channel:** A bus intended to receive auxiliary sends, usually for shared effects.
_Avoid_: Return processor when referring to the channel.

**Clip source:** Reusable musical content and its playback boundaries, independent
of timeline placement.
_Avoid_: Media source when referring to the reusable musical content.

**Arrangement clip:** A placement of musical content at a position in the arrangement.

**Launcher slot:** A track-and-scene location containing a launchable clip or an empty stop slot.

**Scene:** A row of launcher slots launched together.
_Avoid_: Mix snapshot when referring to the launcher row.

**Device:** An instrument, effect or utility in a channel's signal chain.

**Insert:** A device's position and compact representation in the mixer.

**Send:** A branch that copies signal to a destination with its own level.

**Route group:** Channels whose selected controls or editing behavior operate together.

**Folder:** A visual container for organizing channels.
_Avoid_: Bus when referring only to visual organization.

**VCA:** A control channel that adjusts assigned channels without receiving their audio.

**Master:** The channel carrying the main mix output.

**Automation:** Time-varying values assigned to a control.

**Mix snapshot:** A named collection of mixer settings that can be recalled.
_Avoid_: Scene when referring to saved mixer settings.
