--- @meta

--- The `database` module contains Properties and functions for working with
--- the device's LevelDB-backed track database.
--- @class database
--- @field updating Property Whether or not a database re-index is currently in progress.
local database = {}

--- Returns a list of all indexes in the database.
--- @return Index[]
function database.indexes() end

--- Returns the track in the database with the id given
--- @param id TrackId
--- @return Track
function database.track_by_id(id) end


--- @class Track
--- @field id TrackId The track id of this track
--- @field filepath string The filepath of this track
--- @field saved_position integer The last saved position of this track
--- @field tags table A mapping of any available tags to that tag's value
local Track = {}

--- An iterator is a userdata type that behaves like an ordinary Lua iterator.
--- @class Iterator
local Iterator = {}

--- A TrackId is a unique identifier, representing a playable track in the
--- user's library.
--- @class TrackId
local TrackId = {}


--- Gets the human-readable text representing this record. The `__tostring`
--- metatable function is an alias of this function.
--- @class Record
local Record = {}

--- @return string
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
--- @return TrackId|Iterator r A track id if this is a leaf record, otherwise a new iterator for the next level of this index.
function Record:contents() end

--- An index is heirarchical, sorted, view of the tracks within the database.
--- For example, the 'All Albums' index contains, first, a sorted list of every
--- album name in the library. Then, at the second level of the index, a sorted
--- list of every track within each album.
--- @class Index
local Index = {}

--- Gets the human-readable name of this index. This is typically something
--- like "All Albums", or "Albums by Artist". The `__tostring` metatable
--- function is an alias of this function.
--- @return string
function Index:name() end

--- Returns a new iterator that can be used to access every record within the
--- first level of this index.
--- @return Iterator it An iterator that yields `Record`s.
function Index:iter() end

return database
