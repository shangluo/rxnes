function write(addr, data)
	if addr >= 0x8000 and addr <= 0xffff then
		mapper_switch_prg(0x8000, 32, data)
	end
end
