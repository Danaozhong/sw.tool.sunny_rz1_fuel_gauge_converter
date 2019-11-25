#ifndef _OS_CONSOLE_HPP_
#define _OS_CONSOLE_HPP_

/* Standard Library */
#include <memory>
#include <vector>
#include <cstring>

/* FreeRTOS OS calls */
#include "freertos/task.h"

#include "generic_uart.hpp"

#define COMMAND_MAXIMUM_LENGTH (10u)
#define LINE_LENGTH  (75u)
namespace OSServices
{
	class Command
	{
	public:
		Command(const char* ai8_command)
		{
			strncpy(m_ai8_command_str, ai8_command, COMMAND_MAXIMUM_LENGTH - 1);
		}

		virtual ~Command() {}

		const char* get_command() const
		{
			return m_ai8_command_str;
		}

		virtual int32_t execute(char* p_i8_output_buffer, uint32_t u32_buffer_size) const
		{
			return 0;
		}
	protected:
		char m_ai8_command_str[COMMAND_MAXIMUM_LENGTH];

	};

	/** This command is used to list all tasks, their priority, stack size, name, state.
	 */
	class CommandListTasks : public Command
	{
	public:
		CommandListTasks() : Command("lt") {}

		virtual ~CommandListTasks() {}

		virtual int32_t execute(char* p_i8_output_buffer, uint32_t u32_buffer_size) const;
	};

	/** This command prints out the heap memory size */
	class CommandMemory : public Command
	{
	public:
		CommandMemory() : Command("mem") {}

		virtual ~CommandMemory() {}

		virtual int32_t execute(char* p_i8_output_buffer, uint32_t u32_buffer_size) const;
	};

	class OSConsole
	{
	public:
		OSConsole(std::shared_ptr<drivers::GenericUART> po_io_interface);
		~OSConsole() {}

		void run();

	private:
		void print_bootscreen() const;

		void process_input(const char* ai8_input_command);




		std::shared_ptr<drivers::GenericUART> m_po_io_interface;
		std::vector<std::shared_ptr<Command>> m_apo_commands;

		//
		char m_ai8_command_buffer[LINE_LENGTH];

		bool m_bo_entering_command;
	};

}

#endif