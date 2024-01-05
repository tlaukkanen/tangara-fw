--- @meta

--- @class database
local database = {}

--- @return Index[]
function database.indexes() end

--- @class Iterator
local Iterator = {}

--- @class TrackId
local TrackId = {}

--- @class Record
local Record = {}

--- @return string
function Record:title() end

--- @return TrackId|Iterator(Record)
function Record:contents() end

--- @class Index
local Index = {}

--- @return string
function Index:name() end

--- @return Iterator(Record)
function Index:iter() end

return database
