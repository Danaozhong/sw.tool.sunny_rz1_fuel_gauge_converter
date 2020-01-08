#ifndef _ERROR_STORAGE_IF_H_
#define _ERROR_STORAGE_IF_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        EXCP_MODULE_ADC,
        EXCP_MODULE_DAC,
        EXCP_MODULE_AUTOSAR_BASE = 500,
    } ExceptionModuleID;

    typedef enum
    {
        EXCP_TYPE_NULLPOINTER = 100,
        EXCP_TYPE_PARMETER,
        EXCP_TYPE_AUTOSAR_BASE = 500
    } ExceptionTypeID;

    void ExceptionHandler_handle_exception(
            ExceptionModuleID en_module_id,
            ExceptionTypeID en_exception_id,
            bool bo_critical,
            const char* pci8_file,
            uint32_t u32_line, uint32_t u32_misc);

#ifdef __cplusplus
} // Extern "C"
#endif


#endif
