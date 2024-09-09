--- @meta

--- The `playback` module contains Properties and functions for interacting
--- the device's audio pipeline.
--- @class playback
--- @field playing Property Whether or not audio is allowed to be played. if there is a current track, then this indicated whether playback is paused or unpaused. If there is no current track, this determines what will happen when the first track is added to the queue.
--- @field track Property The currently playing track.
--- @field position Property The current playback position within the current track, in seconds.
local playback = {}

--- Returns whether or not this file can be played (i.e. is this an audio track) 
--- @param filepath string
--- @return boolean
function playback.is_playable(filepath) end

return playback
