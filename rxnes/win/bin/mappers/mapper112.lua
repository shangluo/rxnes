local selector = 0

function write(addr, data)
	if addr == 0x8000 then
		selector = data & 0x7
	elseif addr == 0xa000 then
		if selector < 2 then
			mapper_switch_prg(0x8000 + 8 * 1024 * selector, 8, data)
		elseif selector < 4 then
			mapper_switch_chr(0x0 + 0x800 * (selector - 2), 2, data >> 1)		
		else
			mapper_switch_chr(0x1000 + 0x400 * (selector - 4), 1, data)					
		end
	elseif addr == 0xe000 then
		mapper_set_mirror_mode(data & 0x01 == 0x01 and 0 or 1)					
	end
end
