local prg_bank_no = 0

function write(addr, data)
	local dispatch_table = {
		[0x5200] = function ()
			prg_bank_no = prg_bank_no & 0xf0
			prg_bank_no = prg_bank_no |( data & 0x0f )
			--mapper_switch_prg(0x8000, 32, prg_bank_no)
		end,
		[0x5000] = function ()
			prg_bank_no = prg_bank_no & 0x0f
			prg_bank_no = prg_bank_no | ( (data & 0xf) << 4 )
		end,
		[0x5300] = function ()
			if data == 0x6 then
				--mapper_switch_prg(0x8000, 32, 3)
				prg_bank_no = 3
			end
			mapper_switch_prg(0x8000, 32, prg_bank_no)
		end
	}
	
	f = dispatch_table[addr]
	if f ~= nil then
		f()
	end
end
