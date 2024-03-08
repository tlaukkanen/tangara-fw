#!/usr/bin/env lua

local json = require "json"

if #arg > 0 then
  print("usage:", arg[0])
  print([[
reads a lua-language-server json doc output from stdin, converts it into
markdown, and writes the result to stdout]])
  return
end

local raw_data = io.read("*all")
local parsed = json.decode(raw_data)

local definitions_per_module = {}
local fields_per_class = {}

for _, class in ipairs(parsed) do
  if not class.defines or not class.defines[1] then goto continue end

  -- Filter out any definitions that didn't come from us.
  local path = class.defines[1].file
  if not string.find(path, "/luals-stubs/", 1, true) then goto continue end

  local module_name = string.gsub(path, ".*/(%a*)%.lua", "%1")
  local module = definitions_per_module[module_name] or {}
  module[class.name] = class
  definitions_per_module[module_name] = module

  local fields = {}
  for _, field in ipairs(class.fields or {}) do
    fields[field.name] = true
  end
  fields_per_class[class.name] = fields

  ::continue::
end

local function sortedPairs(t)
  local keys = {}
  for key in pairs(t) do
    table.insert(keys, key)
  end
  table.sort(keys)
  local generator = coroutine.create(function()
    for _, key in ipairs(keys) do
      coroutine.yield(key, t[key])
    end
  end)
  return function()
    local _, key, val = coroutine.resume(generator)
    return key, val
  end
end

local function printHeading(level, text)
  local hashes = ""
  for _ = 1, level do hashes = hashes .. "#" end
  print(hashes .. " " .. text)
end

local function filterArgs(field)
  if not field.extends.args then return {} end
  local ret = {}
  for _, arg in ipairs(field.extends.args) do
    if arg.name ~= "self" then table.insert(ret, arg) end
  end
  return ret
end

local function filterReturns(field)
  if not field.extends.returns then return {} end
  local ret = {}
  for _, r in ipairs(field.extends.returns) do
    if r.desc then table.insert(ret, r) end
  end
  return ret
end

local function emitField(level, prefix, field)
  if not field.desc then return end

  printHeading(level, "`" .. prefix .. "." .. field.name .. "`")
  print()
  print("`" .. field.extends.view .. "`")
  print()

  if field.rawdesc then
    print(field.rawdesc)
    print()
  end

  local args = filterArgs(field)
  if #args > 0 then
    printHeading(level + 1, "Arguments")
    print()

    for _, arg in ipairs(args) do
      print(string.format(" - *%s*: %s", arg.name, arg.desc))
    end

    print()
  end

  local rets = filterReturns(field)
  if #rets > 0 then
    printHeading(level + 1, "Returns")
    print()

    for _, ret in ipairs(rets) do
      if #rets > 1 then
        print(" - " .. ret.desc)
      else
        print(ret.desc)
      end
    end

    print()
  end
end

local function baseClassName(class)
  for _, define in ipairs(class.defines or {}) do
    for _, extend in ipairs(define.extends or {}) do
      if extend.type == "doc.extends.name" then
        return extend.view
      end
    end
  end
end

local function isEnum(class)
  for _, define in pairs(class.defines) do
    if define.type == "doc.enum" then return true end
  end
  return false
end

local function isAlias(class)
  for _, define in pairs(class.defines) do
    if define.type == "doc.alias" then return true end
  end
  return false
end

local function emitClass(level, prefix, class)
  if not class.name then return end
  if not class.fields then return end
  if isAlias(class) then return end

  for _, define in ipairs(class.defines or {}) do
    if define.type == "tablefield" then
      print(" - " .. class.name)
      return
    end
  end

  printHeading(level, "`" .. prefix .. "." .. class.name .. "`")
  print()

  local base_class = baseClassName(class)
  local base_class_fields = {}
  if base_class then
    base_class_fields = fields_per_class[base_class] or {}
    print("`" .. class.name .. ":" .. base_class .. "`")
    print()
  end

  if class.desc then print(class.desc) end

  for _, field in ipairs(class.fields or {}) do
    if not base_class_fields[field.name] then
      emitField(level + 1, class.name, field)
    end
  end

  if isEnum(class) then
    printHeading(level + 1, "Values")
    print()
  end
end

local initial_level = 3

for name, module in sortedPairs(definitions_per_module) do
  printHeading(initial_level, "`" .. name .. "`")

  local top_level_class = module[name]
  if top_level_class then
    if top_level_class.desc then print(top_level_class.desc) end
    for _, field in ipairs(top_level_class.fields or {}) do
      emitField(initial_level + 1, name, field)
    end
  end

  for _, class in sortedPairs(module) do
    if class.name ~= name then
      emitClass(initial_level + 1, name, class)
    end
  end
end
