#ifndef _FUEL_GAUGE_INPUT_HPP_
#define _FUEL_GAUGE_INPUT_HPP_

#include <memory>
#include "generic_adc.hpp"
#include "lookup_table.hpp"
#include "event_handler.h"
#include "ex_thread.hpp"


/* Enable this if you want to have additional log output for the fuel sensor acquisition module */
//#define FUEL_GAUGE_INPUT_ENABLE_LOGGING


#define FUEL_GAUGE_INPUT_AVERAGING_SIZE   (7u)

namespace app
{
	/** Helper class to calculate the voltage / resistor values at a voltage divider */
	class VoltageDivider
	{
	public:
		VoltageDivider(uint32_t u32_resistor_1, int32_t i32_supply_voltage);

		uint32_t get_resistor_2_value(int32_t i32_resistor_2_voltage) const;
		int32_t get_resistor_2_voltage(uint32_t u32_resistor_2_value) const;
	private:
		uint32_t m_u32_resistor_1;
		int32_t m_i32_supply_voltage;
	};

	class ParallelVoltageDivider
	{
	public:
		ParallelVoltageDivider(uint32_t d_resistor_1, uint32_t d_resistor_2_parallel, int32_t d_supply_voltage);

		uint32_t get_resistor_2_value(int32_t d_resistor_2_voltage) const;

	private:
		VoltageDivider m_voltage_divider;
		uint32_t m_u32_resistor_1;
		uint32_t m_u32_resistor_2_parallel;
		int32_t m_i32_supply_voltage;
	};

//#define FUEL_GAUGE_INPUT_USE_OWN_TASK

	class FuelGaugeInputFromADC
	{
	public:
		FuelGaugeInputFromADC(std::shared_ptr<drivers::GenericADC> p_adc,
				std::shared_ptr<app::CharacteristicCurve<int32_t, int32_t>> p_fuel_input_characteristic);

		~FuelGaugeInputFromADC();
		/// Signal triggered when a new value from the fuel sensor was retrieved
		boost::signals2::signal<int32_t> m_sig_fuel_level_changed;

		void process_cycle();

		int32_t get_average_fuel_percentage() const;
    private:
#ifdef FUEL_GAUGE_INPUT_USE_OWN_TASK
        void thread_main(void);
        // data acquisition thread
        std_ex::thread* m_po_data_acquisition_thread;
#endif
		/// The ADC used to retrieve data
		std::shared_ptr<drivers::GenericADC> m_p_adc;

		int32_t m_ai32_last_read_fuel_percentages[FUEL_GAUGE_INPUT_AVERAGING_SIZE]; /* in percent * 100 */
		uint32_t m_u32_buffer_counter;

		bool m_bo_initialized;
		bool m_bo_terminate_thread;
		uint32_t m_u32_invalid_read_counter;

		std::shared_ptr<app::CharacteristicCurve<int32_t, int32_t>> m_p_fuel_input_characteristic;

        // the voltage divider is supplied by 5V, and has a 220Ohm resistor on top,
        // and a 330Ohm resistor in parallel to the fuel gauge.
        VoltageDivider m_o_voltage_divider;



	};


}

#endif /* _FUEL_GAUGE_INPUT_HPP_ */
