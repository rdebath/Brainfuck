-- bitops selection
-- since the available bitops vary, we need to pick a working one.

-- This library tries it's hardest to get a proper bit32 compatible library going.
-- It tries the real bit32, wraps luajit's bit to be safe, uses Lua 5.3 operators
-- and if all before fail, it'll use pure lua fallbacks. slow, but working.

--[[
	The MIT License (MIT)

	Copyright (c) 2016 Adrian Pistol

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
--]]

local bitlib

-- args and debug
local enable_debug, force_fallback = ...
local debug
if enable_debug then
	debug = function(msg) io.stderr:write(msg.."\n") end
else
	debug = function() end
end

local function get_bit32()
	if bit32 then
		return bit32
	else
		-- try to require bit32
		local success, res = pcall(require, "bit32")
		if success then
			return res
		end
	end
	return {}
end

local loadstring = loadstring or load

local function bit_l53(op, single)
	if not single then
		return loadstring("return function(a, b) return a "..op.." b end end")
	end
	return loadstring("return function(x) return "..op.."x end end")
end

-- pick bit op from a list of candidates
local function bit_pick_fn(name, res, fn_list)
	for fni=1, #fn_list do
		local fn = fn_list[fni]
		if fn then
			local success, ret = pcall(fn, 5, 4)
			if success then
				if ret == res then
					return fn
				end
				debug("bitops: "..name..": Tried candidate no. "..tostring(fni)..", yielded "..tostring(ret).." but wanted "..tostring(res))
			end
		end
	end
end

-- generate a bit library of all the available options
local function bit_select(inst_tests)
	local lib = {}
	for inst, candidates in pairs(inst_tests) do
		local name, test, fixup = inst[1], inst[2], inst[3]
		local tmp = bit_pick_fn(name, test, candidates)
		if not tmp then
			error("bitops: No working candidate for "..name)
		end
		if unifixer then
			tmp = unifixer(lib, name, tmp)
		end
		if fixup then
			tmp = fixup(tmp)
		end
		lib[name] = tmp
	end
	return lib
end

-- fallback bit library
local fallback = {}
local mfloor = math.floor

local function checkint32(x)
	return mfloor(x) % 0x100000000
end

local function checkshift(s)
	if (s < 0) then error("bad shift") end
	return mmin(32, s)
end

local function fallback_bitconv(na, nb, f)
	-- Fixup negative values before beginning, just in case
	while na < 0 do
		na = na + 0x100000000
	end
	while nb < 0 do
		nb = nb + 0x100000000
	end
	na = checkint32(na)
	nb = checkint32(nb)
	local r = 0
	local p = 1
	for i = 1, 32 do
		local pa = na * 0.5
		local pb = nb * 0.5
		na = mfloor(pa)
		nb = mfloor(pb)
		if f(pa ~= na, pb ~= nb) then
			r = r + p
		end
		p = p * 2
	end
	return r
end

-- NOT
function fallback.bnot(x)
	return (-1 - x) % 0x100000000
end

-- AND
local fb_band_r = function(a, b) return a and b end
function fallback.band(a, b)
	-- This is often used where the situation warrants a reconvert back to bit32.
	-- So, don't assume it will receive sane input.

	-- However, there are a lot of cases where the "convert to bit32" behavior is used.
	-- Which are currently rather slow in the "no need to convert" case.
	-- These neatly cover a good portion of these "there was no need to do this" cases,
	-- speeds things up enormously.

	if (b == 0xFFFFFFFF) and (a >= 0) and (a <= 0xFFFFFFFF) then return mfloor(a) end
	if (a == 0xFFFFFFFF) and (b >= 0) and (b <= 0xFFFFFFFF) then return mfloor(b) end
	if (b == 0xFFFFFFFF) and (a < 0) then
		a = mfloor(0x100000000 + a)
		if (a >= 0) and (a <= 0xFFFFFFFF) then return a end
	end
	if (a == 0xFFFFFFFF) and (b < 0) then
		b = mfloor(0x100000000 + b)
		if (b >= 0) and (b <= 0xFFFFFFFF) then return b end
	end

	return fallback_bitconv(a, b, fb_band_r)
end

-- OR
local fb_bor_r = function(a, b) return a or b end
function fallback.bor(a, b)
	return fallback_bitconv(a, b, fb_bor_r)
end

-- XOR
local fb_bxor_r = function(a, b) return (a or b) and (not (a and b)) end
function fallback.bxor(a, b)
	return fallback_bitconv(a, b, fb_bxor_r)
end

-- lshift
function fallback.lshift(v, s)
	if (s < 0) then error("bad shift") end
	if s > 31 then return 0 end
	return mfloor(fallback.band(v, 0xFFFFFFFF) * 2^s) % 0x100000000
end

-- rshift
function fallback.rshift(v, s)
	if (s < 0) then error("bad shift") end
	return mfloor(fallback.band(v, 0xFFFFFFFF) / 2^s) % 0x100000000
end

-- arshift
function fallback.arshift(v, s)
	if s > 31 then return 0xFFFFFFFF end -- I /think/ that's correct.
	v = checkint32(v)
	if s <= 0 then return (x * 2^(-s)) % 0x100000000 end
	if v < 0x80000000 then return mfloor(v / 2^s) % 0x100000000 end
	return mfloor(v / 2^s) + (0x100000000 - 2^(32 - s))
end

-- exit early if we are supposed to only use the fallbacks
if force_fallback == true then
	debug("bitops: Forced fallback. Hope you know what you are doing.")
	return fallback
end

-- Fixups!
local function fix_negval(i)
	if not i then return nil end
	while i < 0 do
		i = i + 0x100000000
	end
	return i
end
local function fix_negvalwrap(f)
	return function(a, b)
		return fix_negval(f(fix_negval(a), fix_negval(b)))
	end
end

local function fixupall_signed(bl)
	if bl.band(-1, -2) == -2 then
		debug("bitops: Logic functions are SIGNED (not unsigned) 32-bit. Really. Most likely LuaJIT. Patching.")
		local newbl = {}
		for name, fn in pairs(bl) do
			newbl[name] = fix_negvalwrap(fn)
		end
		return newbl
	end
	return bl
end

local function fixup_lshift(lshift)
	local nlshift = lshift
	if lshift(0x80000000, 1) == 0x100000000 then
		debug("bitops: lshift can return <32 bits, applied workaround.")
		nlshift = function(v, s)
			return checkint32(lshift(v, s))
		end
	end
	if lshift(1, 32) == 1 then
		debug("bitops: lshift wraps at 32, patched.")
		return function(v, s)
			if s > 31 then return 0 end
			return nlshift(v, s)
		end
	end
	return nlshift
end

local function fixup_rshift(rshift)
	local nrshift = rshift
	if rshift(1, 32) == 1 then
		debug("bitops: rshift wraps at 32, patched.")
		nrshift = function(v, s)
			if s > 31 then return 0 end
			return rshift(v, s)
		end
	end
	if rshift(0x80000000, 24) ~= 128 then
		debug("bitops: rshift was arithmetic, this is really bad. Still, this is known now - Speed gains can still be gotten.")
		local band = bitlib.band
		return function(v, s)
			return band(nrshift(v, s), nrshift(0xFFFFFFFF, s))
		end
	end
	return nrshift
end

-- actual bit op assembly
local bit32 = get_bit32()
local ljbit = bit and fixupall_signed(bit) or {}

bitlib = bit_select({
	[{'bnot', 0xFFFFFFFA}] = {bit_l53('~', true), bit32.bnot, ljbit.bnot, fallback.bnot},
	[{'band', 4}] = {bit_l53('&'), bit32.band, ljbit.band, fallback.band},
	[{'bor', 5}] = {bit_l53('|'), bit32.bor, ljbit.bor, fallback.bor},
	[{'bxor', 1}] = {bit_l53('~'), bit32.bxor, ljbit.bxor, fallback.bxor},
	[{'lshift', 80, fixup_lshift}] = {bit_l53('<<'), bit32.lshift, ljbit.lshift, fallback.lshift},
	[{'rshift', 0, fixup_rshift}] = {bit_l53('>>'), bit32.rshift, ljbit.rshift, fallback.rshift},
	[{'arshift', 0}] = {bit32.arshift, ljbit.arshift, fallback.arshift},
})

return bitlib
