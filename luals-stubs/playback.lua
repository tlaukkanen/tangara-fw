--- @meta

--- Properties for interacting with the audio playback system
--- @class playback
--- @field playing Property Whether or not audio is allowed to be played. if there is a current track, then this indicated whether playback is paused or unpaused. If there is no current track, this determines what will happen when the first track is added to the queue.
--- @field track Property The currently playing track.
--- @field position Property The current playback position within the current track, in seconds.
local playback = {}

return playback
