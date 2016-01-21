// papu.c
// pseuedo-Audio Processing Unit:
// 2015.12.11

#include "papu.h"
#include "cpu.h"
#include <stdio.h>

static int papu_sequence_generator;
static int papu_master_sequence_mode;
static int papu_irq_disabled;
static int papu_irq_pending;
static int papu_buffer_pos = 0;
static double *papu_sound_buffer;
static int papu_sample_delay = 0;
static int papu_sample_delay_reload = 0;
static int papu_sound_buffer_size = 0;
static papu_buffer_callback papu_callback;

static u8 papu_length_counter_reload_table[][16] =
{
	0x0a, 0xfe,
	0x14, 0x02,
	0x28, 0x04,
	0x50, 0x06,
	0xa0, 0x08,
	0x3c, 0x0a,
	0x0e, 0x0c,
	0x1a, 0x0e,
	0x0c, 0x10,
	0x18, 0x12,
	0x30, 0x14,
	0x60, 0x16,
	0xc0, 0x18,
	0x48, 0x1a,
	0x10, 0x1c,
	0x20, 0x1e
};

static u16 papu_noise_channel_timer_table[] =
{
	0x004,
	0x008,
	0x010,
	0x020,
	0x040,
	0x060,
	0x080,
	0x0a0,
	0x0ca,
	0x0fe,
	0x17c,
	0x1fc,
	0x2fa,
	0x3f8,
	0x7f2,
	0xfe4
};

static u16 papu_dmc_channel_timer_table[] =
{
	0x1ac,
	0x17c,
	0x154,
	0x140,
	0x11e,
	0x0fe,
	0x0e2,
	0x0d6,
	0x0be,
	0x0a0,
	0x08e,
	0x080,
	0x06a,
	0x054,
	0x048,
	0x036
};


enum
{
	PULSE1 = 0,
	PULSE2,
	TRIANGLE,
	NOISE,
	DMC
};

static struct papu_channel
{
	u8 enabled;
	u8 output;
	union
	{
		u8 length_counter_halt;
		u8 envelope_looping;
	};
	u16 length_counter;

	struct
	{
		u16 timer;
		u16 timer_counter;
	};
	u8 duty;
	struct
	{
		u8 enabled;
		u8 period;
		u8 negate;
		u8 shift;
		u8 divider;
	}sweep_unit;

	union
	{
		struct
		{
			u8 counter;
			u8 linear_counter_control;
			u8 linear_counter_load;
		}linear_counter;

		struct
		{
			u8 constant_volume;
			u8 volume_d_envelope;
			u8 divider;
			u8 counter;
		}envelope_loop;
	};

	struct
	{
		u8 mode;
		u8 period;
	}noise;

	struct
	{
		u8 irq_enabled;
		u8 loop;
		u8 rate_index;
		u8 direct_load;
		u8 sample_addr;
		u8 sample_len;

		// output unit
		u8 shifter;
		u16 byte_remain_couter;
		u16 address_counter;
		u8 level;
		u8 silence;
	}dmc;

	u8 length_counter_load;
	u8 channel_sequecer;
	u8 pulse_yield_result;
	u16 noise_channel_shifter;

	u8 pulse_divide_by_2;
}papu_channels[PAPU_CHANNEL_COUNT];

static u8 papu_pulse_duty_sequence[4][8] =
{
	0, 1, 0, 0, 0, 0, 0, 0,
	0, 1, 1, 0, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 0, 0, 0,
	1, 0, 0, 1, 1, 1, 1, 1,
};

void papu_init(int samplerate, int buffer_duration)
{
	papu_channels[NOISE].noise_channel_shifter = 1;
	papu_sound_buffer_size = samplerate * buffer_duration / 1000;
	if (!papu_sound_buffer)
	{
		free(papu_sound_buffer);
	}
	papu_sound_buffer = (double *)malloc(papu_sound_buffer_size * sizeof(double));
	papu_sample_delay_reload = papu_sample_delay = PAPU_SAMPLE_RATE / samplerate;
}

void papu_uninit()
{
	if (papu_sound_buffer)
	{
		free(papu_sound_buffer);
		papu_sound_buffer = NULL;
	}
}

void papu_set_buffer_callback(papu_buffer_callback callback)
{
	papu_callback = callback;
}

static void papu_reload_length_counter(u8 channel, u8 reload_value, u8 reset)
{
	papu_channels[channel].length_counter = papu_length_counter_reload_table[reload_value & 0x01][reload_value >> 1];

	if (reset)
	{
		if (channel <= PULSE2)
		{
			papu_channels[channel].envelope_loop.counter = 15;
			papu_channels[channel].envelope_loop.divider = papu_channels[channel].envelope_loop.volume_d_envelope + 1;
			papu_channels[channel].channel_sequecer = 0;
			papu_channels[channel].pulse_divide_by_2 = 2;
		}
		if (channel == TRIANGLE)
			papu_channels[channel].length_counter_halt = 1;
	}
}

//static u16 papu_reload_noise_channel_timer(u8 period)
//{
//	return papu_channels[NOISE].timer_counter = papu_channels[NOISE].timer = papu_noise_channel_timer_table[period];
//}

#define papu_reload_noise_channel_timer(period) \
do											    \
{												\
	papu_channels[NOISE].timer_counter = papu_channels[NOISE].timer = papu_noise_channel_timer_table[period]; \
} while (0)

#define papu_reload_dmc_channel_timer(period)   \
do											    \
{												\
	papu_channels[DMC].timer_counter = papu_channels[DMC].timer = papu_dmc_channel_timer_table[period]; \
} while (0)

void papu_register_write(u16 addr, u8 data)
{
	u8 i;

	if (addr == 0x4015)
	{
		for (i = 0; i < PAPU_CHANNEL_COUNT; ++i)
		{
			papu_channels[i].enabled = (data & (1 << i)) > 0 ? 1 : 0;
			if (!papu_channels[i].enabled)
			{
				papu_channels[i].length_counter = 0;
			}
		}
	}
	else if (addr == 0x4017)
	{
		papu_master_sequence_mode = (data >> 7) & 0x01;
		papu_irq_disabled = (data >> 6) & 0x01;
		if (papu_irq_disabled)
		{
			papu_irq_pending = 0;
		}
	}
	else
	{
		int channel = (addr - 0x4000) / 4;
		int control_reg = (addr - 0x4000) % 4;
		if (channel <= PULSE2)
		{
			switch (control_reg)
			{
			case 0:
				papu_channels[channel].duty = (data & 0xc0) >> 6;
				papu_channels[channel].length_counter_halt = (data >> 5) & 0x01;
				papu_channels[channel].envelope_loop.constant_volume = (data >> 4) & 0x01;
				papu_channels[channel].envelope_loop.volume_d_envelope = data & 0x0f;
				break;
			case 1:
				papu_channels[channel].sweep_unit.enabled = (data >> 6) & 0x01;
				papu_channels[channel].sweep_unit.period = (data >> 4) & 0x07;
				papu_channels[channel].sweep_unit.divider = papu_channels[channel].sweep_unit.period + 1;
				papu_channels[channel].sweep_unit.negate = (data >> 3) & 0x01;
				papu_channels[channel].sweep_unit.shift = data & 0x07;
				break;
			case 2:
				papu_channels[channel].timer &= 0x700;
				papu_channels[channel].timer |= data;
				papu_channels[channel].timer_counter = papu_channels[channel].timer + 1;
				break;
			case 3:
				papu_channels[channel].length_counter_load = (data & 0xf8) >> 3;
				papu_channels[channel].timer &= 0x0ff;
				papu_channels[channel].timer |= (data & 0x07) << 8;
				papu_channels[channel].timer_counter = papu_channels[channel].timer + 1;
				papu_reload_length_counter(channel, papu_channels[channel].length_counter_load, 1);
				break;
			default:
				break;
			}
		}
		else if (channel == TRIANGLE)
		{
			switch (control_reg)
			{
			case 0:
				papu_channels[channel].length_counter_halt = (data >> 7) & 0x01;
				papu_channels[channel].linear_counter.linear_counter_control = (data >> 6) & 0x01;
				papu_channels[channel].linear_counter.linear_counter_load = data & 0x7f;
				break;
			case 1: // unused
				break;
			case 2:
				papu_channels[channel].timer &= 0x700;
				papu_channels[channel].timer |= data;
				papu_channels[channel].timer_counter = papu_channels[channel].timer + 1;
				break;
			case 3:
				papu_channels[channel].length_counter_load = (data & 0xf8) >> 3;
				papu_channels[channel].timer &= 0x0ff;
				papu_channels[channel].timer |= (data & 0x07) << 8;
				papu_channels[channel].timer_counter = papu_channels[channel].timer + 1;
				papu_reload_length_counter(channel, papu_channels[channel].length_counter_load, 1);
				break;
			default:
				break;
			}
		}
		else if (channel == NOISE)
		{
			switch (control_reg)
			{
			case 0:
				papu_channels[channel].length_counter_halt = (data >> 5) & 0x01;
				papu_channels[channel].envelope_loop.constant_volume = (data >> 4) & 0x01;
				papu_channels[channel].envelope_loop.volume_d_envelope = data & 0x0f;
				break;
			case 1: // unused
				break;
			case 2:
				papu_channels[channel].noise.mode = data >> 6;
				papu_channels[channel].noise.period = data & 0x0f;
				papu_reload_noise_channel_timer(papu_channels[channel].noise.period);
				break;
			case 3:
				papu_channels[channel].length_counter_load = (data & 0xf8) >> 3;
				papu_reload_length_counter(channel, papu_channels[channel].length_counter_load, 1);
				break;
			default:
				break;
			}
		}
		else
		{
			switch (control_reg)
			{
			case 0:
				papu_channels[channel].dmc.irq_enabled = (data >> 7) & 0x01;
				papu_channels[channel].dmc.loop = (data >> 6) & 0x01;
				papu_channels[channel].dmc.rate_index = data & 0x0f;
				papu_reload_dmc_channel_timer(papu_channels[channel].dmc.rate_index);
				break;
			case 1: // 
				papu_channels[channel].output = data & 0x7f;
				break;
			case 2:
				papu_channels[channel].dmc.sample_addr = data;
				papu_channels[channel].dmc.address_counter = papu_channels[channel].dmc.sample_addr * 0x40 + 0xc000;
				break;
			case 3:
				papu_channels[channel].dmc.sample_len = data;
				papu_channels[channel].dmc.address_counter = papu_channels[channel].dmc.sample_len * 0x10 + 1;
				break;
			default:
				break;
			}
		}
	}
}

u8 papu_register_read(u16 addr)
{
	int i;
	u8 ret = 0;
	if (addr == 0x4015)
	{
		for (i = 0; i < PAPU_CHANNEL_COUNT; ++i)
		{
			ret |= ((papu_channels[i].length_counter > 0 ? 1 : 0) << i);
		}

		if (papu_irq_pending)
		{
			cpu_set_irq_pending();
			ret |= papu_irq_pending << 6;
			papu_irq_pending = 0;
		}
	}

	return ret;
}

void papu_check_pending_irq()
{
	if (papu_irq_pending)
	{
		cpu_set_irq_pending();
	}
}

static void papu_mixer_channels()
{
	// mix using linear approximation
	double pulse_out;
	double tnd_out;

	pulse_out = 0.00752 * (papu_channels[PULSE1].output + papu_channels[PULSE2].output);
	tnd_out = 0.00851 * papu_channels[TRIANGLE].output + 0.00494 * papu_channels[NOISE].output + 0.00335 * papu_channels[DMC].output;
	papu_sound_buffer[papu_buffer_pos] = pulse_out + tnd_out;

	// advance buffer position
	papu_buffer_pos = (papu_buffer_pos + 1) % papu_sound_buffer_size;
	if (papu_buffer_pos == 0)
	{
		papu_callback(papu_sound_buffer, papu_sound_buffer_size);
	}
}

//static u8 papu_get_channel_output(int channel)
//{
//	u8 output = 0;
//	u8 feedback = 0;
//	static u8 pulse_duty_sequence[4][8] = 
//	{
//		0, 1, 0, 0, 0, 0, 0, 0,
//		0, 1, 1, 0, 0, 0, 0, 0,
//		0, 1, 1, 1, 1, 0, 0, 0,
//		1, 0, 0, 1, 1, 1, 1, 1,
//	};
//
//
//	if (((channel != TRIANGLE && papu_channels[channel].length_counter > 0) 
//		|| (channel == TRIANGLE && papu_channels[channel].linear_counter.counter > 0 && papu_channels[channel].length_counter > 0))
//		&& papu_channels[channel].timer >= 8)
//	{
//		switch (channel)
//		{
//		case PULSE1:
//		case PULSE2:
//			if (pulse_duty_sequence[papu_channels[channel].duty][papu_channels[channel].channel_sequecer])
//			{
//				output = papu_channels[channel].envelope_loop.constant_volume ? papu_channels[channel].envelope_loop.volume_d_envelope : papu_channels[channel].envelope_loop.counter;
//			}
//			else
//			{
//				output = 0;
//			}
//			break;
//		case TRIANGLE:
//			if (papu_channels[channel].channel_sequecer < 16)
//			{
//				output = 0x0f - papu_channels[channel].channel_sequecer;
//			}
//			else
//			{
//				output = papu_channels[channel].channel_sequecer - 16;
//			}
//			break;
//
//		case NOISE:
//			output = (papu_channels[channel].noise_channel_shifter & 0x01 || papu_channels[channel].length_counter == 0) ? 0 : \
//				papu_channels[channel].envelope_loop.constant_volume ? papu_channels[channel].envelope_loop.volume_d_envelope : papu_channels[channel].envelope_loop.counter;
//			break;
//		default:
//			break;
//		}
//	}
//	return output;
//}


#define papu_get_channel_output(out, channel) \
do										  \
{										  \
	u8 output = 0;						  \
	u8 feedback = 0;					  \
							\
	if (((channel != TRIANGLE && papu_channels[channel].length_counter > 0)															\
		|| (channel == TRIANGLE && papu_channels[channel].linear_counter.counter > 0 && papu_channels[channel].length_counter > 0)) \
		&& papu_channels[channel].timer >= 8)															   \
	{																									   \
		switch (channel)																				   \
		{																								   \
		case PULSE1:																					   \
		case PULSE2:																					   \
			if (papu_pulse_duty_sequence[papu_channels[channel].duty][papu_channels[channel].channel_sequecer]) \
			{																	\
				output = papu_channels[channel].envelope_loop.constant_volume ? \
					papu_channels[channel].envelope_loop.volume_d_envelope : \
					papu_channels[channel].envelope_loop.counter;			 \
			}															 \
			else														 \
			{															 \
				output = 0;												 \
			}															 \
			break;														 \
		case TRIANGLE:													 \
			if (papu_channels[channel].channel_sequecer < 16)			 \
			{														     \
				output = 0x0f - papu_channels[channel].channel_sequecer; \
			}														   \
			else                                                       \
			{														   \
				output = papu_channels[channel].channel_sequecer - 16; \
			}		\
			break;  \
		case NOISE: \
			output = (papu_channels[channel].noise_channel_shifter & 0x01 || papu_channels[channel].length_counter == 0) ? 0 : \
				papu_channels[channel].envelope_loop.constant_volume ? \
				papu_channels[channel].envelope_loop.volume_d_envelope : papu_channels[channel].envelope_loop.counter; \
			break;		\
		case DMC:		\
			output = papu_channels[channel].output; \
			break;		\
		default:		\
			break;		\
		}               \
		out = output;	\
	}					\
} while (0)

void papu_run_frame_counter()
{
	int i;
	if (!papu_master_sequence_mode && papu_sequence_generator == 3)
	{
		// irq
	}

	if ((!papu_master_sequence_mode && (papu_sequence_generator == 1 || papu_sequence_generator == 3))
		|| (papu_master_sequence_mode && (papu_sequence_generator == 0 || papu_sequence_generator == 2)))
	{
		// clock length counters and sweep units
		for (i = 0; i < PAPU_CHANNEL_COUNT; ++i)
		{
			if (papu_channels[i].length_counter && !papu_channels[i].length_counter_halt)
			{
				--papu_channels[i].length_counter;
			}

			if ((i <= PULSE2) && papu_channels[i].sweep_unit.enabled)
			{
				u16 result = papu_channels[i].timer << papu_channels[i].sweep_unit.shift;
				if (papu_channels[i].sweep_unit.negate)
				{
					result = ~result;
					if (i == PULSE2)
					{
						result += 1;
					}
					result += papu_channels[i].timer;
				}

				--papu_channels[i].sweep_unit.divider;
				if (papu_channels[i].timer >= 8 && result < 0x7ff)
				{
					if (!papu_channels[i].sweep_unit.divider)
					{
						papu_channels[i].pulse_yield_result = 1;
						papu_channels[i].timer = result;
					}
					else
					{
						papu_channels[i].pulse_yield_result = 0;
					}
				}
				if (papu_channels[i].sweep_unit.divider == 0)
				{
					papu_channels[i].sweep_unit.divider = papu_channels[i].sweep_unit.period + 1;
				}
			}
		}
	}

	if (papu_sequence_generator < 4)
	{
		// clock envelopes and triangle's linear counter
		for (i = 0; i < PAPU_CHANNEL_COUNT; ++i)
		{
			if (i != TRIANGLE && i != DMC && !papu_channels[i].envelope_loop.constant_volume)
			{
				--papu_channels[i].envelope_loop.divider;
				if (papu_channels[i].envelope_loop.divider == 0)
				{
					// clocked
					if (papu_channels[i].envelope_looping && papu_channels[i].envelope_loop.counter == 0)
					{
						papu_channels[i].envelope_loop.counter = 15;
					}
					else if (papu_channels[i].envelope_loop.counter)
					{
						--papu_channels[i].envelope_loop.counter;
					}

					// reload
					papu_channels[i].envelope_loop.divider = papu_channels[i].envelope_loop.volume_d_envelope + 1;
				}
			}
			else if (i == TRIANGLE)
			{
				if (papu_channels[i].length_counter_halt)
				{
					papu_channels[i].linear_counter.counter = papu_channels[i].linear_counter.linear_counter_load;
				}
				else if (papu_channels[i].linear_counter.counter > 0)
				{
					--papu_channels[i].linear_counter.counter;
				}

				if (!papu_channels[i].length_counter_halt)
				{
					papu_channels[i].length_counter_halt = !papu_channels[i].length_counter_halt;
				}
			}
		}
	}
	papu_sequence_generator = (papu_sequence_generator + 1) % (papu_master_sequence_mode ? 5 : 4);

	// irq
	if (!papu_irq_disabled && papu_sequence_generator == 0 && papu_master_sequence_mode == 0)
	{
		papu_irq_pending = 1;
	}
}

void papu_run_loop(u32 cycles)
{
	int i;
	static u8 channels_output[PAPU_CHANNEL_COUNT] = { 0, 0, 0, 0, 0 };
	u32 cycles_to_exe = 0;
	int pos = papu_buffer_pos;

	while (cycles_to_exe++ < cycles /*/ 2*//* * 2*/)
	{
		for (i = 0; i < PAPU_CHANNEL_COUNT; ++i)
		{
			//if ((i == TRIANGLE && i % 2 == 0) || i != TRIANGLE)
			{
				--papu_channels[i].timer_counter;
			}
			channels_output[i] = 0;
			if (papu_channels[i].enabled && papu_channels[i].length_counter > 0)
			{
				papu_get_channel_output(channels_output[i], i);
			}
			if (!papu_channels[i].timer_counter && papu_channels[i].enabled)
			{
				if (i <= PULSE2)
				{
					--papu_channels[i].pulse_divide_by_2;
					if (papu_channels[i].pulse_divide_by_2 == 0)
					{
						papu_channels[i].pulse_divide_by_2 = 2;
						// clock
						// papu_channels[i].channel_sequecer = (papu_channels[i].channel_sequecer + 1) % 8;
						if (papu_channels[i].channel_sequecer > 0)
						{
							--papu_channels[i].channel_sequecer;
						}
						else
						{
							papu_channels[i].channel_sequecer = 7;
						}
					}
				}
				else if (i == TRIANGLE && papu_channels[i].length_counter > 0 && papu_channels[i].linear_counter.counter > 0)
				{
					papu_channels[i].channel_sequecer = (papu_channels[i].channel_sequecer + 1) % 32;
				}
				else if (i == NOISE)
				{
					u8 feedback = (papu_channels[i].noise_channel_shifter & 0x01) \
						^ (papu_channels[i].noise.mode ? (papu_channels[i].noise_channel_shifter >> 5 & 0x01)
							: (papu_channels[i].noise_channel_shifter >> 1 & 0x01));
					papu_channels[i].noise_channel_shifter >>= 1;
					papu_channels[i].noise_channel_shifter |= feedback << 14;
				}
				else if (i == DMC)
				{

				}

				// reload timer
				if (i == NOISE)
				{
					papu_reload_noise_channel_timer(papu_channels[i].noise.period);
				}
				else if (i == DMC)
				{
					papu_reload_dmc_channel_timer(papu_channels[i].noise.period);
				}
				else
				{
					papu_channels[i].timer_counter = papu_channels[i].timer;
				}
			}
			papu_channels[i].output = channels_output[i];
		}
		--papu_sample_delay;
		if (!papu_sample_delay)
		{
			papu_mixer_channels();
			papu_sample_delay = papu_sample_delay_reload;
		}
	}
}

u8 papu_get_sound_channel(int channel)
{
	return papu_channels[channel].output;
}