--- @meta

--- Properties and functions for inspecting and manipulating the track playback queue
--- @class queue
--- @field position Property The index in the queue of the currently playing track. This may be zero if the queue is empty. Writeable.
--- @field size Property The total number of tracks in the queue, including tracks which have already been played.
--- @field replay Property Whether or not the queue will be restarted after the final track is played. Writeable.
--- @field repeat_track Property Whether or not the current track will repeat indefinitely. Writeable.
--- @field random Property  Determines whether, when progressing to the next track in the queue, the next track will be chosen randomly. The random selection algorithm used is a Miller Shuffle, which guarantees that no repeat selections will be made until every item in the queue has been played. Writeable.
local queue = {}

function queue.next() end
function queue.previous() end

return queue
