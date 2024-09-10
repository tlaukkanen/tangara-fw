--- @meta

--- @class theme
local theme = {}

--- Loads a theme from a filename, this can be either builtin (ie, located in
--- "/lua/") or on the sdcard (in, '/sdcard/.themes/')
--- If successful, the filename will be saved to non-volatile storage.
--- Returns whether the theme was successfully loaded 
--- @param filename string
--- @return boolean
function theme.load_theme(filename) end

--- Sets a theme directly from a table. Does not persist between restarts. 
--- @param theme
function theme.set(theme) end

--- Set the subject (similar in concept to a css class) of an object. 
--- This will set any styles within the theme associated with that subject
--- on the given object
--- @param obj Object The object to set a particular style on
--- @param style string The name of the style to apply to this object
function theme.set_subject(obj, subject_name) end

--- Returns the filename of the saved theme
--- @return string 
function theme.theme_filename() end

return theme
