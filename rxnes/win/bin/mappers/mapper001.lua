reset = 0
load_register = 0
cotrol_register = 0

function write(addr, data)
	if (data & 0x80) == 0x80 then
		reset = 0
		load_register = 0
		cotrol_register = cotrol_register | 0x0c
		mapper_switch_prg(0xc000, 16, mapper_get_prg_cnt() - 1)
		return
	end
	
	if reset == 0 then
		load_register = 0
	end
	
	load_register = load_register | ( ( data & 0x1 ) << reset )
	reset = reset + 1
	
	if reset == 5 then
	    --control
        if  addr >= 0x8000 and addr <= 0x9fff then
            cotrol_register = load_register;
			mapper_set_mirror_mode((cotrol_register & 0x3) == 0x2 and 0 or 1)
        --chr_bank0
        elseif addr >= 0xa000 and addr <= 0xbfff then
            local chr_bank0 = load_register
			if mapper_get_chr_cnt() > 0 then
				if ( cotrol_register & 0x10 ) ~= 0x10 then
					mapper_switch_chr(0, 8, (chr_bank0 & 0x1e))
				else
					mapper_switch_chr(0, 4, (chr_bank0 & 0x1f))
				end
			end
        --chr_bank1
        elseif  addr >= 0xc000 and addr <= 0xdfff then
            local chr_bank1 = load_register;
            if ( cotrol_register & 0x10 ) ~= 0x10 then
                --ignore
                reset = 0;
                return;
			end

            --switch
			mapper_switch_chr(0x1000, 4, chr_bank1);
        --prg_bank
        elseif addr >= 0xe000 and addr <= 0xffff then
            local prg_bank = load_register & 0x0f
            local mode = cotrol_register & 0x0c 
			
			if mode == 0 or mode == 0x04 then
			    --switch 32, igore low bit
				mapper_switch_prg(0x8000, 32, prg_bank & 0x0e)
			elseif mode == 0x08 then
				mapper_switch_prg(0xc000, 16, prg_bank)
			elseif mode == 0x0c then
				mapper_switch_prg(0x8000, 16, prg_bank)
			end
		end
        reset = 0
	end
end
