bank_selector = 0
chr_a12_inversion = 0
prg_rom_bank_mode = 0

irq_reload_value = 0
irq_reload_request = 0
irq_counter = 0
irq_pending = 0
irq_enabled = false

function write(addr, data)
	-- back select
	if addr >= 0x8000 and addr <= 0x9ffe and addr % 2 == 0 then
		bank_selector = data & 0x07
		prg_rom_bank_mode = (data >> 6) & 0x01
		chr_a12_inversion = (data >> 7) & 0x01
		
		local offset = 0xc000
		if prg_rom_bank_mode == 1 then offset = 0x8000 end
		mapper_switch_prg(offset, 8, mapper_get_prg_cnt() * 2 - 2)
	elseif addr >= 0x8001 and addr <= 0x9fff and addr % 2 == 1 then
		local bank_no = 0
		if bank_selector >= 6 then
			bank_no = data & 0x3f
		elseif bank_selector <= 1 then
			bank_no = data >> 1
		else
			bank_no = data 
		end
		
		if  bank_selector <= 1 then
			mapper_switch_chr(chr_a12_inversion * 0x1000 + bank_selector * 0x800, 2, bank_no)
		elseif bank_selector > 1 and bank_selector <= 5 then
			mapper_switch_chr((1 - chr_a12_inversion) * 0x1000 + (bank_selector - 2) * 0x400, 1, bank_no)
		elseif bank_selector == 6 then
			if prg_rom_bank_mode == 0 then
				mapper_switch_prg(0x8000, 8, bank_no)
			else
				mapper_switch_prg(0xc000, 8, bank_no)
			end
		elseif bank_selector == 7 then
			mapper_switch_prg(0xa000, 8, bank_no)
		end
	elseif addr >= 0xa000 and addr <= 0xbffe and addr % 2 == 0 then
		mapper_set_mirror_mode(1 - data & 0x01)
	elseif addr >= 0xa001 and addr <= 0xbfff and addr % 2 == 1 then
		-- ram protect
	elseif addr >= 0xc000 and addr <= 0xdffe and addr % 2 == 0 then
		irq_reload_value = data
	elseif addr >= 0xc001 and addr <= 0xdfff and addr % 2 == 1 then
		irq_reload_request = 0
		irq_counter = 0
	elseif addr >= 0xe000 and addr <= 0xfffe and addr % 2 == 0 then
		if irq_pending == 1 then
			irq_pending = 0
			mapper_set_irq_pending(irq_pending)
		end		
		irq_enabled = false
	elseif addr >= 0xe001 and addr <= 0xffff and addr % 2 == 1 then
		irq_enabled = true
	end
end

function a12()
	if irq_reload_request == 1 or irq_counter == 0 then
		irq_counter = irq_reload_value
	elseif irq_counter > 0 then
		irq_counter = irq_counter - 1
	end
	
	if irq_counter == 0 and irq_enabled then
		irq_pending = 1
		mapper_set_irq_pending(irq_pending)
	end
	
	irq_reload_request = 0
end
