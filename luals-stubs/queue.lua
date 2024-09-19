--- @meta

--- The `queue` module contains Properties and functions that relate to the
--- device's playback queue. This is a persistent, disk-backed list of TrackIds
--- that includes the currently playing track, tracks that have been played,
--- and tracks that are scheduled to be played after the current track has
--- finished.
--- @class queue
--- @field position Property The index in the queue of the currently playing track. This may be zero if the queue is empty. Writeable.
--- @field size Property The total number of tracks in the queue, including tracks which have already been played.
--- @field replay Property Whether or not the queue will be restarted after the final track is played. Writeable.
--- @field repeat_track Property Whether or not the current track will repeat indefinitely. Writeable.
--- @field random Property Determines whether, when progressing to the next track in the queue, the next track will be chosen randomly. The random selection algorithm used is a Miller Shuffle, which guarantees that no repeat selections will be made until every item in the queue has been played. Writeable.
local queue = {}

--- Adds the given track, database iterator, or file to the end of the queue. Database
--- iterators passed to this method will be unnested and expanded into the track
--- ids they contain.
--- @param val TrackId|Iterator|string
function queue.add(val) end

--- Opens a playlist file from a filepath
--- This will replace the existing queue
--- @param filepath string
function queue.open_playlist(filepath) end

--- Removes all tracks from the queue.
function queue.clear() end

--- Moves forward in the play queue, looping back around to the beginning if repeat is on.
function queue.next() end

--- Moves backward in the play queue, looping back around to the end if repeat is on.
function queue.previous() end

--- Play a track starting from a number of seconds in
--- This will replace the existing queue
--- @param filepath string
--- @param seconds_offset integer 
function queue.play_from(filepath, seconds_offset) end

return queue
