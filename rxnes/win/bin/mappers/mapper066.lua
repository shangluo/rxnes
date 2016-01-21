function write(addr, data)
	if addr >= 0x8000 and addr <= 0xffff then
		mapper_switch_chr(0x0, 8, data & 0x3)
		mapper_switch_prg(0x8000, 32, (data >> 4) & 0x3)
	end
end
