function write(addr, data)
	if addr >= 0x8000 and addr <= 0xffff then
		mapper_switch_prg(0x8000, 16, data >> 4 & 0xff);
		mapper_switch_chr(0x0, 8, data & 0xff);
	end
end

function reset()
end