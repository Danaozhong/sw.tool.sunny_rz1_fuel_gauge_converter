

#include "fuel_gauge_input.hpp"
#include "excp_handler_if.h"
#include <functional> // for std::bind
#include "os_console.hpp"


#define FUEL_GAUGE_LOG(...)

// Ugly macros to allow debug logging
#if defined(FUEL_GAUGE_INPUT_ENABLE_LOGGING) && defined(USE_TRACE)
/* Compile with debug output */
#include "trace_if.h"
#undef FUEL_GAUGE_LOG
#define FUEL_GAUGE_LOG(...)   DEBUG_PRINTF(__VA_ARGS__)
#endif

namespace app
{

	VoltageDivider::VoltageDivider(uint32_t u32_resistor_1, int32_t i32_supply_voltage)
		: m_u32_resistor_1(u32_resistor_1), m_i32_supply_voltage(i32_supply_voltage)
	{}

	uint32_t VoltageDivider::get_resistor_2_value(int32_t i32_resistor_2_voltage) const
	{
		// R2 = R1/(U - U2) / U - R1
		return (m_u32_resistor_1 * m_i32_supply_voltage) / (m_i32_supply_voltage - i32_resistor_2_voltage ) - m_u32_resistor_1;
	}

	int32_t VoltageDivider::get_resistor_2_voltage(uint32_t u32_resistor_2_value) const
	{
		// currently noone cares
		return 0;
	}

	int32_t VoltageDivider::get_supply_voltage() const
	{
	    return m_i32_supply_voltage;
	}

	ParallelVoltageDivider::ParallelVoltageDivider(uint32_t d_resistor_1, uint32_t d_resistor_2_parallel, int32_t d_supply_voltage)
	: m_u32_resistor_1(d_resistor_1), m_i32_supply_voltage(d_supply_voltage), m_u32_resistor_2_parallel(d_resistor_2_parallel),
			m_voltage_divider(d_resistor_1, d_supply_voltage)
	{}

	uint32_t ParallelVoltageDivider::get_resistor_2_value(int32_t i32_resistor_2_voltage) const
	{
		uint32_t resistor_total = m_voltage_divider.get_resistor_2_value(i32_resistor_2_voltage);
		// r*r2/(r2 - r) = r1
		return resistor_total * m_u32_resistor_2_parallel / (m_u32_resistor_2_parallel - resistor_total);
	}


	FuelGaugeInputFromADC::FuelGaugeInputFromADC(drivers::GenericADC* p_adc,
			const app::CharacteristicCurve<int32_t, int32_t>& o_fuel_input_characteristic)
	: m_p_adc(p_adc), m_o_fuel_input_characteristic(o_fuel_input_characteristic),
	  m_u32_buffer_counter(0u), m_bo_initialized(false), m_bo_terminate_thread(false), m_u32_invalid_read_counter(0u),
      m_o_voltage_divider(100000, 3300), // 100 Ohm (value representation is in mOhm), 3V3 supply
      m_i32_adc_pin_voltage(0),
      m_i32_fuel_sensor_resistor_value(0)
	{
#ifdef FUEL_GAUGE_INPUT_USE_OWN_TASK
		// start the data acquisition thread
		auto main_func = std::bind(&FuelGaugeInputFromADC::thread_main, this);
		m_po_data_acquisition_thread = new std_ex::thread(main_func, "FUEL_SensorInput", 2u, 0x800);
#endif /* FUEL_GAUGE_INPUT_USE_OWN_TASK */
	}

	FuelGaugeInputFromADC::~FuelGaugeInputFromADC()
	{
#ifdef FUEL_GAUGE_INPUT_USE_OWN_TASK
	    if(m_po_data_acquisition_thread != nullptr)
	    {
	        this->m_bo_terminate_thread = true;
	        m_po_data_acquisition_thread->join();
	        delete m_po_data_acquisition_thread;
	    }
#endif

	    // disconnecting the signal handler should not be necessary, will we done automatically during deletion
	}

	void FuelGaugeInputFromADC::process_cycle()
	{
	    // read from the ADC
        uint32_t u32_adc_value = m_p_adc->read_adc_value();
        FUEL_GAUGE_LOG("Current ADC value: %u\r\n", u32_adc_value);

        // covert to voltage
        m_i32_adc_pin_voltage = (m_o_voltage_divider.get_supply_voltage() * u32_adc_value) / m_p_adc->get_adc_max_value();
        FUEL_GAUGE_LOG("Current ADC voltage: %i.%iV\r\n", i32_adc_pin_voltage / 1000, i32_adc_pin_voltage % 1000);

        // convert to resistor value of the fuel sensor (in mOhm)
        m_i32_fuel_sensor_resistor_value = m_o_voltage_divider.get_resistor_2_value(m_i32_adc_pin_voltage);

        FUEL_GAUGE_LOG("Current calculated resistor value: %i mOhm\r\n", m_i32_fuel_sensor_resistor_value);

        // find the percentage
        int32_t i32_read_fuel_percentage = m_o_fuel_input_characteristic.get_x(m_i32_fuel_sensor_resistor_value);

        /* On the first read, initialize the buffer with the read values */
        if (false == m_bo_initialized)
        {
            m_bo_initialized = true;
            for (uint32_t u32i = 0; u32i < FUEL_GAUGE_INPUT_AVERAGING_SIZE; ++u32i)
            {
                m_ai32_last_read_fuel_percentages[u32i] = i32_read_fuel_percentage;
            }
        }

        /* Check if the deviation is more than 10% */
        if (std::abs(get_average_fuel_percentage() - i32_read_fuel_percentage) < 1000)
        {
            /* only take the value into account if it differs less than 10% */
            m_ai32_last_read_fuel_percentages[m_u32_buffer_counter] = i32_read_fuel_percentage;
            m_u32_buffer_counter++;
            if (m_u32_buffer_counter >= FUEL_GAUGE_INPUT_AVERAGING_SIZE)
            {
                m_u32_buffer_counter = 0;
            }

            // Send a signal that the fuel level has changed
            const int32_t i32_averaged_fuel_percentage = get_average_fuel_percentage();
            FUEL_GAUGE_LOG("Current calculated fuel input level: %i\r\n", i32_averaged_fuel_percentage);
            this->m_sig_fuel_level_changed(i32_averaged_fuel_percentage);

            // since we have read a valid value, reset the invalid data counter
            m_u32_invalid_read_counter = 0;
        }
        else
        {
            m_u32_invalid_read_counter++;

            /* If we keep reading nonsense, throw everything away and start again */
            if (m_u32_invalid_read_counter > 10)
            {
                FUEL_GAUGE_LOG("Read fuel input data did not make sense, starting again.\r\n");
                ExceptionHandler_handle_exception(EXCP_MODULE_FUEL_SIGNAL_CONVERTER,
                        EXCP_TYPE_FUEL_SIGNAL_CONVERTER_RESTARTED_CONVERSION,
                        false, __FILE__, __LINE__, static_cast<uint32_t>(i32_read_fuel_percentage));

                m_bo_initialized = false;
            }
        }
	}

#ifdef FUEL_GAUGE_INPUT_USE_OWN_TASK
	void FuelGaugeInputFromADC::thread_main(void)
	{
		FUEL_GAUGE_LOG("Fuel Sensor Input Data Acquisition thread started!\r\n");


		// only quit this main loop when the object is destroyed
		while(false == this->m_bo_terminate_thread)
		{
		    process_cycle();
			std_ex::sleep_for(std::chrono::milliseconds(200));
		}
	}
#endif /* FUEL_GAUGE_INPUT_USE_OWN_TASK */

	int32_t FuelGaugeInputFromADC::get_average_fuel_percentage() const
	{
		int64_t i64_avg_value = 0ull;
		for (uint32_t u32i = 0; u32i < FUEL_GAUGE_INPUT_AVERAGING_SIZE; ++u32i)
		{
			i64_avg_value += static_cast<int64_t>(m_ai32_last_read_fuel_percentages[u32i]);
		}
		return static_cast<int32_t>(i64_avg_value / FUEL_GAUGE_INPUT_AVERAGING_SIZE);
	}


    int32_t FuelGaugeInputFromADC::get_adc_voltage() const
    {
        return m_i32_adc_pin_voltage;
    }

    int32_t FuelGaugeInputFromADC::get_fuel_sensor_resistor_value() const
    {
        return m_i32_fuel_sensor_resistor_value;
    }
}
