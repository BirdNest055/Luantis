-- Luanti
-- Copyright (C) 2013 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

--------------------------------------------------------------------------------
-- Generic implementation of a filter/sortable list                           --
--                                                                            --
-- Usage:                                                                     --
-- Filterlist needs to be initialized on creation. To achieve this you need   --
-- to pass following functions:                                               --
-- raw_fct() (mandatory):                                                     --
--     function returning a table containing the elements to be filtered      --
-- compare_fct(element1,element2) (mandatory):                                --
--     function returning true/false if element1 is same element as element2  --
-- uid_match_fct(element1,uid) (optional)                                     --
--     function telling if uid is attached to element1                        --
-- filter_fct(element,filtercriteria) (optional)                              --
--     function returning true/false if filtercriteria met to element         --
-- fetch_param (optional)                                                     --
--     parameter passed to raw_fct to acquire correct raw data                --
--                                                                            --
-- Methods:                                                                   --
--   add(element)       - add a single element to the list                    --
--   add_all(list)      - add a table of elements                             --
--   remove(uid)        - remove element matching uid                         --
--   refresh()          - re-fetch raw data and reprocess                     --
--   process()          - apply filter and sort, update m_list                --
--   get_list()         - return current filtered/sorted list                 --
--   get_raw_list()     - return the unfiltered raw list                      --
--   get_size()         - return size of filtered list                        --
--   get_element(index) - return element at 1-based index                     --
--   set_filtercriteria(criteria) - set filter criteria and reprocess         --
--   set_sortmode(mode)  - set sort mode and reprocess                        --
--                                                                            --
-- NOTE: Code cleanup opportunities (formerly TODO):                          --
--   - Internal state (m_raw_list, m_list, m_filtercriteria, etc.) uses      --
--     m_ prefix but there is no encapsulation — any caller can reach in.     --
--     Consider migrating to a proper class pattern with getter/setters.      --
--   - filterlist.process() rebuilds m_list on every call; for large lists,   --
--     an incremental approach (only re-filter changed elements) would help.  --
--   - sort_fct is stored per-instance but currently always falls back to     --
--     the default alphabetical sort — either remove the indirection or       --
--     implement custom sort comparators.                                     --
--------------------------------------------------------------------------------
filterlist = {}

--------------------------------------------------------------------------------
--- Re-fetches the raw data list and reprocesses (filter + sort).
-- @param self filterlist instance
function filterlist.refresh(self)
        self.m_raw_list = self.m_raw_list_fct(self.m_fetch_param)
        filterlist.process(self)
end

--------------------------------------------------------------------------------
--- Creates a new filterlist instance.
-- @param raw_fct       (mandatory) function() → table of elements to filter
-- @param compare_fct   (mandatory) function(a, b) → bool, true if a and b are the same element
-- @param uid_match_fct (optional)  function(element, uid) → bool, true if uid belongs to element
-- @param filter_fct    (optional)  function(element, criteria) → bool, true if element passes filter
-- @param fetch_param   (optional)  parameter passed to raw_fct when fetching data
-- @return filterlist instance
function filterlist.create(raw_fct,compare_fct,uid_match_fct,filter_fct,fetch_param)

        assert((raw_fct ~= nil) and (type(raw_fct) == "function"))
        assert((compare_fct ~= nil) and (type(compare_fct) == "function"))

        local self = {}

        self.m_raw_list_fct  = raw_fct
        self.m_compare_fct   = compare_fct
        self.m_filter_fct    = filter_fct
        self.m_uid_match_fct = uid_match_fct

        self.m_filtercriteria = nil
        self.m_fetch_param = fetch_param

        self.m_sortmode = "none"
        self.m_sort_list = {}

        self.m_processed_list = nil
        self.m_raw_list = self.m_raw_list_fct(self.m_fetch_param)

        self.add_sort_mechanism = filterlist.add_sort_mechanism
        self.set_filtercriteria = filterlist.set_filtercriteria
        self.get_filtercriteria = filterlist.get_filtercriteria
        self.set_sortmode       = filterlist.set_sortmode
        self.get_list           = filterlist.get_list
        self.get_raw_list       = filterlist.get_raw_list
        self.get_raw_element    = filterlist.get_raw_element
        self.get_raw_index      = filterlist.get_raw_index
        self.get_current_index  = filterlist.get_current_index
        self.size               = filterlist.size
        self.uid_exists_raw     = filterlist.uid_exists_raw
        self.raw_index_by_uid   = filterlist.raw_index_by_uid
        self.refresh            = filterlist.refresh

        filterlist.process(self)

        return self
end

--------------------------------------------------------------------------------
--- Registers a custom sort mechanism that can be activated by name.
-- @param self filterlist instance
-- @param name unique name for the sort mode (used with set_sortmode)
-- @param fct  function(self) that sorts self.m_processed_list in-place
function filterlist.add_sort_mechanism(self,name,fct)
        self.m_sort_list[name] = fct
end

--------------------------------------------------------------------------------
--- Sets the filter criteria and reprocesses the list.
-- Skips reprocessing if criteria is unchanged (non-table values only;
-- table values are always reprocessed since they may have mutated).
-- @param self     filterlist instance
-- @param criteria value passed to the filter function for each element
function filterlist.set_filtercriteria(self,criteria)
        if criteria == self.m_filtercriteria and
                type(criteria) ~= "table" then
                return
        end
        self.m_filtercriteria = criteria
        filterlist.process(self)
end

--------------------------------------------------------------------------------
--- Returns the current filter criteria.
-- @param self filterlist instance
-- @return current criteria value (may be nil)
function filterlist.get_filtercriteria(self)
        return self.m_filtercriteria
end

--------------------------------------------------------------------------------
--- Sets the sort mode and reprocesses the list.
-- @param self filterlist instance
-- @param mode "none" for no sorting, "alphabetic" for alphabetical,
--             or a custom name registered via add_sort_mechanism
function filterlist.set_sortmode(self,mode)
        if (mode == self.m_sortmode) then
                return
        end
        self.m_sortmode = mode
        filterlist.process(self)
end

--------------------------------------------------------------------------------
--- Returns the current filtered and sorted list.
-- @param self filterlist instance
-- @return table of elements after filtering and sorting
function filterlist.get_list(self)
        return self.m_processed_list
end

--------------------------------------------------------------------------------
--- Returns the unfiltered raw data list.
-- @param self filterlist instance
-- @return table of all raw elements (no filter or sort applied)
function filterlist.get_raw_list(self)
        return self.m_raw_list
end

--------------------------------------------------------------------------------
--- Returns a single element from the raw list by 1-based index.
-- @param self filterlist instance
-- @param idx  1-based index (string or number); returns nil if out of range
function filterlist.get_raw_element(self,idx)
        if type(idx) ~= "number" then
                idx = tonumber(idx)
        end

        if idx ~= nil and idx > 0 and idx <= #self.m_raw_list then
                return self.m_raw_list[idx]
        end

        return nil
end

--------------------------------------------------------------------------------
--- Maps a processed-list index back to the corresponding raw-list index.
-- @param self       filterlist instance
-- @param listindex  1-based index into the processed (filtered/sorted) list
-- @return raw-list index (1-based), or 0 if not found
function filterlist.get_raw_index(self,listindex)
        assert(self.m_processed_list ~= nil)

        if listindex ~= nil and listindex > 0 and
                listindex <= #self.m_processed_list then
                local entry = self.m_processed_list[listindex]

                for i,v in ipairs(self.m_raw_list) do

                        if self.m_compare_fct(v,entry) then
                                return i
                        end
                end
        end

        return 0
end

--------------------------------------------------------------------------------
--- Maps a raw-list index to the corresponding processed-list index.
-- @param self       filterlist instance
-- @param listindex  1-based index into the raw list
-- @return processed-list index (1-based), or 0 if the element is filtered out
function filterlist.get_current_index(self,listindex)
        assert(self.m_processed_list ~= nil)

        if listindex ~= nil and listindex > 0 and
                listindex <= #self.m_raw_list then
                local entry = self.m_raw_list[listindex]

                for i,v in ipairs(self.m_processed_list) do

                        if self.m_compare_fct(v,entry) then
                                return i
                        end
                end
        end

        return 0
end

--------------------------------------------------------------------------------
--- Applies the current filter and sort to produce m_processed_list.
-- When no filter and no sort are active, m_processed_list is set to the raw
-- list directly (no copy). Otherwise, elements passing the filter are collected
-- and then sorted by the active sort mechanism.
-- @param self filterlist instance
function filterlist.process(self)
        assert(self.m_raw_list ~= nil)

        if self.m_sortmode == "none" and
                self.m_filtercriteria == nil then
                self.m_processed_list = self.m_raw_list
                return
        end

        self.m_processed_list = {}

        for k,v in pairs(self.m_raw_list) do
                if self.m_filtercriteria == nil or
                        self.m_filter_fct(v,self.m_filtercriteria) then
                        self.m_processed_list[#self.m_processed_list + 1] = v
                end
        end

        if self.m_sortmode == "none" then
                return
        end

        if self.m_sort_list[self.m_sortmode] ~= nil and
                type(self.m_sort_list[self.m_sortmode]) == "function" then

                self.m_sort_list[self.m_sortmode](self)
        end
end

--------------------------------------------------------------------------------
--- Returns the number of elements in the filtered/sorted list.
-- @param self filterlist instance
-- @return count of elements (0 if not yet processed)
function filterlist.size(self)
        if self.m_processed_list == nil then
                return 0
        end

        return #self.m_processed_list
end

--------------------------------------------------------------------------------
--- Checks whether an element with the given UID exists in the raw list.
-- @param self filterlist instance
-- @param uid  unique identifier to search for
-- @return true if found, false otherwise
function filterlist.uid_exists_raw(self,uid)
        for i,v in ipairs(self.m_raw_list) do
                if self.m_uid_match_fct(v,uid) then
                        return true
                end
        end
        return false
end

--------------------------------------------------------------------------------
--- Finds the raw-list index of the element matching the given UID.
-- @param self filterlist instance
-- @param uid  unique identifier to search for
-- @return 1-based index in the raw list, or 0 if not found or ambiguous
function filterlist.raw_index_by_uid(self, uid)
        local elementcount = 0
        local elementidx = 0
        for i,v in ipairs(self.m_raw_list) do
                if self.m_uid_match_fct(v,uid) then
                        elementcount = elementcount +1
                        elementidx = i
                end
        end


        -- If there are more elements than one with same name uid can't decide which
        -- one is meant. This shouldn't be possible but just for sure.
        -- NOTE: The uid_match mechanism assumes unique UIDs in the raw list.
        -- If duplicates exist (e.g., two worlds with the same path), the function
        -- returns 0 to indicate ambiguity. The caller should handle this case.
        -- A future improvement could log a warning when duplicates are detected
        -- during list construction rather than silently returning 0 here.
        if elementcount > 1 then
                elementidx=0
        end

        return elementidx
end

--------------------------------------------------------------------------------
-- COMMON helper functions                                                    --
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
--- Compares two world definitions for equality by path, name, and gameid.
-- Used as the compare_fct for world filterlists.
-- @param world1 first world table
-- @param world2 second world table
-- @return true if both worlds have the same path, name, and gameid
function compare_worlds(world1,world2)
        if world1.path ~= world2.path then
                return false
        end

        if world1.name ~= world2.name then
                return false
        end

        if world1.gameid ~= world2.gameid then
                return false
        end

        return true
end

--------------------------------------------------------------------------------
--- Sorts the processed world list alphabetically by name (case-insensitive).
-- Nil entries are pushed to the end (see issue #857).
-- @param self filterlist instance (sorts self.m_processed_list in-place)
function sort_worlds_alphabetic(self)

        table.sort(self.m_processed_list, function(a, b)
                --fixes issue #857 (crash due to sorting nil in worldlist)
                if a == nil or b == nil then
                        if a == nil and b ~= nil then return false end
                        if b == nil and a ~= nil then return true end
                        return false
                end
                if a.name:lower() == b.name:lower() then
                        return a.name < b.name
                end
                return a.name:lower() < b.name:lower()
        end)
end

--------------------------------------------------------------------------------
--- Sorts the processed mod list with game mods at the bottom, modpacks
-- grouped together, and names in alphabetical order within each group.
-- @param self filterlist instance (sorts self.m_processed_list in-place)
function sort_mod_list(self)

        table.sort(self.m_processed_list, function(a, b)
                -- Show game mods at bottom
                if a.type ~= b.type or a.loc ~= b.loc then
                        if b.type == "game" then
                                return a.loc ~= "game"
                        end
                        return b.loc == "game"
                end
                -- If in same or no modpack, sort by name
                if a.modpack == b.modpack then
                        if a.name:lower() == b.name:lower() then
                                return a.name < b.name
                        end
                        return a.name:lower() < b.name:lower()
                -- Else compare name to modpack name
                else
                        -- Always show modpack pseudo-mod on top of modpack mod list
                        if a.name == b.modpack then
                                return true
                        elseif b.name == a.modpack then
                                return false
                        end

                        local name_a = a.modpack or a.name
                        local name_b = b.modpack or b.name
                        if name_a:lower() == name_b:lower() then
                                return  name_a < name_b
                        end
                        return name_a:lower() < name_b:lower()
                end
        end)
end
