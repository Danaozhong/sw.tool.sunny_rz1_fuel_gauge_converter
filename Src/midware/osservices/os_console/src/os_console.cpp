#include "os_console.hpp"

namespace OSServices
{


int32_t CommandListTasks::execute(const char** params, uint32_t u32_num_of_params, char* p_i8_output_buffer, uint32_t u32_buffer_size)
{
	if (nullptr == p_i8_output_buffer || u32_buffer_size < 2)
	{
		return -1;
	}

	//char ai8_buffer[400] = { 0 };
#ifdef USE_FREERTOS_TASK_LIST
	vTaskList(p_i8_output_buffer);
#else

		UBaseType_t uxArraySize;
		char* pcWriteBuffer = &p_i8_output_buffer[0];

		/* Make sure the write buffer does not contain a string. */
		*pcWriteBuffer = ( char ) 0x00;

		/* Take a snapshot of the number of tasks in case it changes while this
		function is executing. */
		uxArraySize = uxTaskGetNumberOfTasks();;

		/* Allocate an array index for each task.  NOTE!  if
		configSUPPORT_DYNAMIC_ALLOCATION is set to 0 then pvPortMalloc() will
		equate to NULL. */
		TaskStatus_t pxTaskStatusArray[uxArraySize];

		/* Generate the (binary) data. */
		uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

		/* Create a human readable table from the binary data. */

		/* Print the table header */
		snprintf(pcWriteBuffer, u32_buffer_size - strlen(p_i8_output_buffer) - 1,
				"Name        Status      Prio  Stack                    ID\r\n"
				"---------------------------------------------------------\r\n");
		pcWriteBuffer += strlen(pcWriteBuffer);

		for(uint32_t x = 0; x < uxArraySize; x++ )
		{
			char ai8_status_str[100] = "invalid";
			switch( pxTaskStatusArray[ x ].eCurrentState )
			{
				case eRunning:
					strcpy(ai8_status_str, "running");
					break;
				case eReady:
					strcpy(ai8_status_str, "ready");
					break;
				case eBlocked:
					strcpy(ai8_status_str, "blocked");
					break;
				case eSuspended:
					strcpy(ai8_status_str, "SUSPENDED");
					break;
				case eDeleted:
					strcpy(ai8_status_str, "deleted");
					break;
				case eInvalid:
				default:
					strcpy(ai8_status_str, "INVALID");
					break;
			}

			/* Write the task name to the string, padding with spaces so it
			can be printed in tabular form more easily. */
			//pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer,  );
			uint32_t u32_stack_size = 0;
#ifdef configRECORD_STACK_HIGH_ADDRESS
			u32_stack_size = uxTaskGetStackSize(pxTaskStatusArray[x].xHandle);
#endif
			uint32_t u32_stack_usage = 100;
			if (0 != u32_stack_size)
			{
				u32_stack_usage = (pxTaskStatusArray[x].usStackHighWaterMark * 100) / u32_stack_size;
			}


			/* Write the rest of the string. */

			snprintf( pcWriteBuffer, u32_buffer_size - strlen(p_i8_output_buffer) - 1, "%-10s  %-10s  %-4u  %08x/%08x %3u%%  %-2u\r\n",
					pxTaskStatusArray[ x ].pcTaskName, ai8_status_str,
					static_cast<unsigned int>(pxTaskStatusArray[ x ].uxCurrentPriority),
					static_cast<unsigned int>(pxTaskStatusArray[ x ].usStackHighWaterMark),
					static_cast<unsigned int>(u32_stack_size),
					static_cast<unsigned int>(u32_stack_usage),
					( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber ); /*lint !e586 sprintf() allowed as this is compiled with many compilers and this is a utility function only - not part of the core kernel implementation. */
			pcWriteBuffer += strlen( pcWriteBuffer ); /*lint !e9016 Pointer arithmetic ok on char pointers especially as in this case where it best denotes the intent of the code. */
		}
#endif
		return 0;
	}

	int32_t CommandMemory::execute(const char** params, uint32_t u32_num_of_params, char* p_i8_output_buffer, uint32_t u32_buffer_size)
	{
		memset(p_i8_output_buffer, 0, u32_buffer_size);

		int i_free_heap = xPortGetFreeHeapSize();
	    int i_min_heap = xPortGetMinimumEverFreeHeapSize();

	    snprintf(p_i8_output_buffer, u32_buffer_size - 1,
	    		"Current heap size: %u bytes\r\n"
	    		"Minimum ever heap size: %u bytes\r\n",
				i_free_heap, i_min_heap
	    );


#if 0

		typedef struct FREE FREE;

		struct FREE {                          // Heap free block list structure:
		   uint32_t sulSize;                   //    Free block size
		   FREE *spsNext;                      //    Pointer to the next free block
		};

		extern FREE __data_Aldata;             // Heap information structure
		/* sample code to walk the heap */
		   FREE *xpsFree;                      // Free heap block
		   long *xplStack;                     // Stack pointer
		   long xlRomSize;                     // ROM size
		   long xlRamSize;                     // RAM size
		   long xlMemory;
		   long xlHeapTotal;
		   long xlHeapFree;
		   long xlHeapUsed;

		// The heap is walked to find the amount of free memory available.

		   xlHeapFree = 0;
		   xpsFree = &__data_Aldata;
		   while (xpsFree->spsNext) {
		      xpsFree = xpsFree->spsNext;
		      xlHeapFree += xpsFree->sulSize;
		   }
		   xlHeapTotal = (long) __sfe ("HEAP") – (long) __sfb ("HEAP");
		   xlHeapUsed = xlHeapTotal – xlHeapFree;

		// Header

		   printf ("Heap             | Free stack         | Free memoryrn");
		   xlRomSize = (long) __sfb ("ROM0_END") – (long) __sfb ("ROM0_START") + 1;
		   xlRamSize = (long) __sfb ("RAM_END") – (long) __sfb ("RAM_START") + 1;

		// Used heap, FIQ stack, and free flash

		   for (xplStack = (long *) &Stack_FIQ;
		         xplStack < (long *) &StackTop_FIQ && *xplStack == RAM_TEST;
		         ++xplStack);
		   xlMemory = (long) __sfb ("ROM0_END") – (long) __sfb ("ROM0_TOP") + 1;
		   printf ("   Used:  %5d  |    FIQ: %5d      |    Flash:    %6d (%d%%)rn",
		         xlHeapUsed, 4 * (xplStack – (long *) &Stack_FIQ), xlMemory,
		         100 * xlMemory / xlRomSize);

		// Free heap, IRQ stack, and free RAM

		   for (xplStack = (long *) &Stack_IRQ;
		         xplStack < (long *) &StackTop_IRQ && *xplStack == RAM_TEST;
		         ++xplStack);
		   xlMemory = (int) __sfe ("RAM_END") – (int) __sfb ("RAM_TOP") + 1;
		   printf ("   Free:  %5d  |    IRQ: %5d      |    RAM:      %6d (%d%%)rn",
		         xlHeapFree, 4 * (xplStack – (long *) &Stack_IRQ), xlMemory,
		         100 * xlMemory / xlRamSize);

		// Total heap, SVC stack, and free heap + RAM

		   for (xplStack = (long *) &Stack_SVC;
		         xplStack < (long *) &StackTop_SVC && *xplStack == RAM_TEST;
		         ++xplStack);
		   printf ("   TOTAL: %5d  |    SVC: %5d      |    Heap+RAM: %6d (%d%%)rn",
		         xlHeapTotal, 4 * (xplStack – (long *) &Stack_SVC),
		         xlMemory + xlHeapFree, 100 * (xlMemory + xlHeapFree) / xlRamSize);
		}
#endif
		return 0;
	}



	OSConsole::OSConsole(drivers::GenericUART* po_io_interface)
	: m_po_io_interface(po_io_interface), m_u32_num_of_registered_commands(0u), m_bo_entering_command(false)
	{
		memset(this->m_ai8_command_buffer, 0x0, sizeof(this->m_ai8_command_buffer));

		// removed to save some memory
		this->register_command(new CommandListTasks());
		this->register_command(new CommandMemory());
		print_bootscreen();
	}

	void OSConsole::process_input(const char* ai8_input_command)
	{
		bool bo_program_executed = false;

		// split the string by spaces, and only check for the first command part
		char ai8_command_copy[COMMAND_MAXIMUM_LENGTH] = { 0 };
		const char delimiter[] = " ";

		// first copy it so that we can run strtok() on it.
		strncpy(ai8_command_copy, ai8_input_command, COMMAND_MAXIMUM_LENGTH);

		// process all backspace commands, if the user has mistyped something
		erase_backspaces(ai8_command_copy);

        const char* api8_delimiters[100]  = { nullptr };
        uint32_t u32_num_of_delimiters = 0u;
		// then search for all spaces and split the lines
		char* p_i8_token = strtok(ai8_command_copy, delimiter);
		while (nullptr != p_i8_token)
		{
		    api8_delimiters[u32_num_of_delimiters] = p_i8_token;
		    ++u32_num_of_delimiters;
		    p_i8_token = strtok(nullptr, delimiter);
		}

		const char* pi8_command = api8_delimiters[0];
		if (nullptr != pi8_command)
		{
            for (uint32_t u32_i = 0; u32_i < m_u32_num_of_registered_commands; ++u32_i)
            {
                auto p_command = this->m_apo_commands[u32_i];

                if (strcmp(pi8_command, p_command->get_command()) == 0)
                {
                    // execute the command and print the buffer
                    const uint32_t cu32_output_buffer_size = 1024;
                    char ai8_output_buffer[cu32_output_buffer_size] = { 0 };
                    /* u32_num_of_delimiters -1 because the first delimiter is the command,
                     * which does not count as a delimiter */
                    int32_t i32_return_code = p_command->execute(api8_delimiters + 1,
                            u32_num_of_delimiters - 1,
                            ai8_output_buffer, cu32_output_buffer_size);
                    m_po_io_interface->write(reinterpret_cast<uint8_t*>(ai8_output_buffer), strlen(ai8_output_buffer));

                    // and print the return code.
                    char ai8_print_str[LINE_LENGTH] = { 0 };
                    snprintf(ai8_print_str, LINE_LENGTH - 1, "\r\nProgram \'%s\' has terminated with return code %i.\r\n", \
                            p_command->get_command(), static_cast<int>(i32_return_code));
                    m_po_io_interface->write(reinterpret_cast<uint8_t*>(ai8_print_str), strlen(ai8_print_str));

                    bo_program_executed = true;
                }
            }
		}
		if (false == bo_program_executed)
		{
			if (strlen(ai8_input_command) > 0)
			{
				char ai8_print_str[LINE_LENGTH] = { 0 };
				snprintf(ai8_print_str, LINE_LENGTH - 1, "No command found with name \'%s\'.\r\n", ai8_input_command);
				m_po_io_interface->write(reinterpret_cast<uint8_t*>(ai8_print_str), strlen(ai8_print_str));
			}
		}
	}

	void OSConsole::run()
	{
		if (this->m_po_io_interface != nullptr)
		{
			int i_read_char = 0;
			while(this->m_po_io_interface->available() > 0)
			{
				i_read_char = m_po_io_interface->read();

				if (i_read_char >= 0)
				{
					char i8_read_char = static_cast<char>(i_read_char);

					// first character of a new command is entered.
					if (m_bo_entering_command == false)
					{
						m_bo_entering_command = true;
						const char ai8_command_prompt[] = "\r\n\r\nFreeRTOS> ";
						m_po_io_interface->write(reinterpret_cast<const uint8_t*>(ai8_command_prompt), strlen(ai8_command_prompt));
					}

					// print out the character to the serial (like in a terminal)
					m_po_io_interface->write(reinterpret_cast<uint8_t*>(&i8_read_char), 1);

					if ('\r' == i8_read_char)
					{
						// command finished, new line
						const char ai8_command_prompt[] = "\r\n\r\n";
						m_po_io_interface->write(reinterpret_cast<const uint8_t*>(ai8_command_prompt), strlen(ai8_command_prompt));

						// command end, process
						this->process_input(m_ai8_command_buffer);
						// reset the buffer to all 0s
						memset(m_ai8_command_buffer, 0, LINE_LENGTH);
						m_bo_entering_command = false;
					}
					else
					{
						// make sure the entered command fits the buffer
						if (strlen(m_ai8_command_buffer) < LINE_LENGTH - 1)
						{
							m_ai8_command_buffer[strlen(m_ai8_command_buffer)] = i8_read_char;
						}
					}
				}
			}
		}
	}

	int32_t OSConsole::register_command(Command* p_command)
	{
		if (nullptr == p_command || this->m_u32_num_of_registered_commands + 1 >= OS_CONSOLE_MAX_NUM_OF_COMMANDS)
		{
			return -1;
		}

		this->m_apo_commands[this->m_u32_num_of_registered_commands] = p_command;
		this->m_u32_num_of_registered_commands++;
		return 0;
	}


	void OSConsole::print_bootscreen() const
	{
		char ai8_bootscreen[] = "FreeRTOS Platform V0.1\n\r(c) 2019 \n\r";
		m_po_io_interface->write(reinterpret_cast<const uint8_t*>(ai8_bootscreen), strlen(ai8_bootscreen));
	}
}


size_t erase_backspaces(char* pc_string)
{
    const size_t string_len = strlen(pc_string);
    char* write_ptr = pc_string;
    char* read_ptr = pc_string; // how many backspaces have already been erased

    while (read_ptr != pc_string + string_len) // check if end of string reached
    {
        if (*read_ptr == '\b')
        {
            // when reading a backspace, ignore this character and jump to the one after it
            read_ptr++;
            if (write_ptr != pc_string) // only erase a character if this is not start of string
            {
                // remove one character (will be overwritten in the next loop)
                write_ptr--;
            }
            // execute while condition statement, to make sure we don't overrun the buffer
            continue;
        }
        *write_ptr = *read_ptr;
        write_ptr++;
        read_ptr++;
    }
    // append a null terminator
    *write_ptr = '\0';
    return read_ptr - write_ptr;
}
