--- Module for accessing and updating data about the user's library of tracks.
-- @module database

local database = {}

--- Returns a list of all indexes in the database.
-- @treturn Array(Index)
function database.indexes() end

--- An iterator is a userdata type that behaves like an ordinary Lua iterator.
-- @type Iterator
local Iterator = {}

--- A TrackId is a unique identifier, representing a playable track in the
--- user's library.
-- @type TrackId
local TrackId = {}

--- A record is an item within an Index, representing some value at a specific
--- depth.
-- @type Record
local Record = {}

--- Gets the human-readable text representing this record. The `__tostring`
--- metatable function is an alias of this function.
-- @treturn string
function Record:title() end

--- Returns the value that this record represents. This may be either a track
--- id, for records which uniquely identify a track, or it may be a new
--- Iterator representing the next level of depth for the current index.
---
--- For example, each Record in the "All Albums" index corresponds to an entire
--- album of tracks; the 'contents' of such a Record is an iterator returning
--- each track in the album represented by the Record. The contents of each of
--- the returned 'track' Records would be a full Track, as there is no further
--- disambiguation needed.
-- @treturn TrackId|Iterator(Record)
function Record:contents() end

--- An index is heirarchical, sorted, view of the tracks within the database.
--- For example, the 'All Albums' index contains, first, a sorted list of every
--- album name in the library. Then, at the second level of the index, a sorted
--- list of every track within each album.
-- @type Index
local Index = {}

--- Gets the human-readable name of this index. This is typically something
--- like "All Albums", or "Albums by Artist". The `__tostring` metatable
--- function is an alias of this function.
-- @treturn string
function Index:name() end

--- Returns a new iterator that can be used to access every record within the
--- first level of this index.
-- @treturn Iterator(Record)
function Index:iter() end

return database
