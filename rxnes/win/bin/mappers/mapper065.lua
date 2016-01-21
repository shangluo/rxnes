irq_reload_value = 0
irq_counter = 0
irq_pending = 0
irq_enabled = false

function write(addr, data)
	local register_dispatcher = 
	{
		[0x8000] = function ()
			mapper_switch_prg(0x8000, 8, data)
		end,
		[0xa000] = function ()
			mapper_switch_prg(0xa000, 8, data)
		end,		
		[0xc000] = function ()
			mapper_switch_prg(0xc000, 8, data)
		end,
		[0x9001] = function ()
			mapper_set_mirror_mode(data & 0x80 == 0x80 and 0 or 1)
		end,
		[0x9003] = function ()
			ack_irq()
			irq_enabled = data & 0x80 == 0x80 and true or false
		end,
		[0x9004] = function ()
			ack_irq()
			irq_counter = irq_reload_value
		end,
		[0x9005] = function ()
			irq_reload_value = irq_reload_value & 0xff00
			irq_reload_value = irq_reload_value | (data << 8)
		end,
		[0x9006] = function ()
			irq_reload_value = irq_reload_value & 0xff
			irq_reload_value = irq_reload_value | data
		end
	}
	
	f = register_dispatcher[addr]
	if f ~= nil then
		f()
	end
	
	if addr >= 0xb000 and addr <= 0xb007 then
		mapper_switch_chr((addr & 0x7) * 0x400, 1, data);
	end
end

function ack_irq()
	if irq_pending == 1 then
		irq_pending = 0
		mapper_set_irq_pending(irq_pending)
	end	
end

function reset()
	mapper_switch_prg(0x8000, 8, 0x00)
	mapper_switch_prg(0xa000, 8, 0x01)
	mapper_switch_prg(0xc000, 8, 0xfe)
end

function runloop(cycle_count)
	if not irq_enabled then
		return 
	end
	
	irq_counter = irq_counter - cycle_count
	if irq_counter <= 0 then
		irq_pending = 1
		mapper_set_irq_pending(irq_pending)
		irq_counter = 0
	end
end
