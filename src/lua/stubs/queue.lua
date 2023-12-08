--- Properties and functions for inspecting and manipulating the track playback queue
-- @module queue

local queue = {}

--- queue.position returns the index in the queue of the currently playing track. This may be zero if the queue is empty.
-- @treturn types.Property a positive integer property, which is a 1-based index
function queue.position() end

--- queue.size returns the total number of tracks in the queue, including tracks which have already been played.
-- @treturn types.Property a positive integer property
function queue.size() end

--- queue.replay determines whether or not the queue will be restarted after the final track is played.
-- @treturn types.Property a writeable boolean property
function queue.replay() end

--- queue.random determines whether, when progressing to the next track in the queue, the next track will be chosen randomly. The random selection algorithm used is a Miller Shuffle, which guarantees that no repeat selections will be made until every item in the queue has been played.
-- @treturn types.Property a writeable boolean property
function queue.random() end

return queue