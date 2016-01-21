function write(addr, data)
	if addr >= 0x8000 and addr <= 0xffff then
		mapper_switch_chr(0x0, 8, data)
	end
end
