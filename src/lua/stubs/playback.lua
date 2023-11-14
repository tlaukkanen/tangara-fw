--- Properties for interacting with the audio playback system
-- @module playback

local playback = {}

--- Whether or not any audio is *allowed* to be played. If there is a current track, then this is essentially an indicator of whether playback is paused or unpaused.
--- This value isn't meaningful if there is no current track.
-- @treturn types.Property a boolean property
function playback.playing() end

return playback
