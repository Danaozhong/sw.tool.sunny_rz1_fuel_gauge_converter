
#include "fuel_gauge_output.hpp"

// Ugly macros to allow debug logging
#define FUEL_GAUGE_OUTPUT_LOG(...)

// Ugly macros to allow debug logging
#if defined(FUEL_GAUGE_OUTPUT_ENABLE_LOGGING) && defined(USE_TRACE)
/* Compile with debug output */
#include "trace_if.h"
#undef FUEL_GAUGE_OUTPUT_LOG
#define FUEL_GAUGE_OUTPUT_LOG(...)   DEBUG_PRINTF(__VA_ARGS__)
#endif


namespace app
{
	FuelGaugeOutput::FuelGaugeOutput(drivers::GenericDAC* p_dac, \
			app::CharacteristicCurve<int32_t, int32_t>* p_fuel_output_characteristic, \
			int32_t i32_amplifying_factor, int32_t i32_aplifiying_offset)
	: m_p_dac(p_dac), m_p_fuel_output_characteristic(p_fuel_output_characteristic),
	  m_i32_amplifying_factor(i32_amplifying_factor), m_i32_aplifiying_offset(i32_aplifiying_offset)
	{}


	int32_t FuelGaugeOutput::set_fuel_level(int32_t i32_fuel_level)
	{
		//if (i32_fuel_level < 0.0 || d_fuel_level > 100.0)
		//{
		//	return -1;
		//}

		//m_d_set_voltage = d_fuel_level;
		// Calculate the desired final voltage
		m_i32_set_voltage_output = m_p_fuel_output_characteristic->get_y(i32_fuel_level);

		FUEL_GAUGE_OUTPUT_LOG("Setting output to voltage %i\r\n", static_cast<int>(i32_final_voltage));

		// ...take the effects of the OpAmp into account.
		// the m_i32_amplifying_factor is given in x1000, therefore needs to multiply 1000 to balance out
		m_i32_set_voltage_dac = (m_i32_set_voltage_output - m_i32_aplifiying_offset) * 1000 / m_i32_amplifying_factor;


		// and then send the signal to the DAC.
		return m_p_dac->set_output_voltage(m_i32_set_voltage_dac);
	}

    int32_t FuelGaugeOutput::get_voltage_output() const
    {
        return m_i32_set_voltage_output;
    }

    int32_t FuelGaugeOutput::get_voltage_dac() const
    {
        return m_i32_set_voltage_dac;
    }
}
