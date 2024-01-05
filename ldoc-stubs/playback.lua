--- Properties for interacting with the audio playback system
-- @module playback

local playback = {}

--- Whether or not any audio is *allowed* to be played. If there is a current track, then this is essentially an indicator of whether playback is paused or unpaused.
-- @see types.Property
playback.playing = types.Property

--- Rich information about the currently playing track.
-- @see types.Property
-- @see types.Track
playback.track = types.Property

--- The current playback position within the current track, in seconds.
-- @see types.Property
playback.position = types.Property

return playback
