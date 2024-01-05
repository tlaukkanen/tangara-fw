--- Properties and functions for inspecting and manipulating the track playback queue
-- @module queue

local queue = {}

--- The index in the queue of the currently playing track. This may be zero if the queue is empty.
-- @see types.Property
queue.position = types.Property

--- The total number of tracks in the queue, including tracks which have already been played.
-- @see types.Property
queue.size = types.Property

--- Determines whether or not the queue will be restarted after the final track is played.
-- @see types.Property
queue.replay = types.Property

--- Determines whether, when progressing to the next track in the queue, the next track will be chosen randomly. The random selection algorithm used is a Miller Shuffle, which guarantees that no repeat selections will be made until every item in the queue has been played.
-- @see types.Property
queue.random = types.Property

return queue
