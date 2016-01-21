irq_reload_value = 0
irq_reload_request = 0
irq_counter = 0
irq_pending = 0
irq_enabled = false

function write(addr, data)
	local register_dispatcher = 
	{
		[0x8000] = function ()
			mapper_switch_prg(0x8000, 8, data)
		end,
		[0x8001] = function ()
			mapper_switch_prg(0xa000, 8, data)
		end,
		[0x8002] = function ()
			mapper_switch_chr(0x0, 2, data)
		end,
		[0x8003] = function ()
			mapper_switch_chr(0x0800, 2, data)
		end,
		[0xa000] = function ()
			mapper_switch_chr(0x1000, 1, data)
		end,
		[0xa001] = function ()
			mapper_switch_chr(0x1400, 1, data)
		end,
		[0xa002] = function ()
			mapper_switch_chr(0x1800, 1, data)
		end,
		[0xa003] = function ()
			mapper_switch_chr(0x1c00, 1, data)
		end,
		[0xe000] = function ()
			mapper_set_mirror_mode(data & 0x40 == 0x40)			
		end
	}
	
	addr = addr & 0xe003
	f = register_dispatcher[addr]
	if f ~= nil then
		f()
	elseif addr == 0xc000 then
		irq_reload_value = (data ^ 0xff) & 0xff
	elseif addr == 0xc001 then
		irq_reload_request = true
		irq_counter = 0
	elseif addr == 0xc003 then
		if irq_pending == 1 then
			irq_pending = 0
			mapper_set_irq_pending(irq_pending)
		end		
		irq_enabled = false
	elseif addr == 0xc002 then
		irq_enabled = true
	end
end

function reset()
	mapper_switch_prg(0xc000, 8, mapper_get_prg_cnt() * 2 - 2)
	mapper_switch_prg(0xe000, 8, mapper_get_prg_cnt() * 2 - 1)
end

function a12()
	if irq_reload_request == true or irq_counter == 0 then
		irq_counter = irq_reload_value
	elseif irq_counter > 0 then
		irq_counter = irq_counter - 1
	end
	
	if irq_counter == 0 and irq_enabled then
		irq_pending = 1
		mapper_set_irq_pending(irq_pending)
	end
	
	irq_reload_request = false
end
