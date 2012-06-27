/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_uku.h
 * Types and definitions that are common across OSs for the user side of the
 * User-Kernel interface.
 */

#ifndef _UKU_H_
#define _UKU_H_

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <malisw/mali_stdtypes.h>
#include <uk/mali_uk.h>
#include <plat/mali_uk_os.h>

/**
 * @addtogroup uk_api User-Kernel Interface API
 * @{
 */

/**
 * @defgroup uk_api_user UKU (User side)
 *
 * The UKU is an OS independent API for user-side code which provides functions to
 * - open and close a UKK client driver, a kernel-side device driver implementing the UK interface
 * - call functions inside a UKK client driver
 *
 * The code snippets below show an example using the UKU API:
 *
 * Start with opening the imaginary Midgard Base UKK client driver
 *
@code
    mali_error ret;
    uku_context uku_ctx;
    uku_client_version client_version;
    uku_open_status open_status;

    // open a user-kernel context
    client_version.major = TESTDRV_UK_MAJOR;
    client_version.minor = TESTDRV_UK_MINOR;
    open_status = uku_open(UK_CLIENT_MALI_T600_BASE, &client_version, &uku_ctx);
    if (UKU_OPEN_OK != open_status)
    {
        mali_tpi_printf("failed to open a user-kernel context\n");
        goto cleanup;
    }
@endcode
 *
 * We are going to call a function foo in the Midgard Base UKK client driver. For sample purposes this
 * function foo will simply double the provided input value.
 *
 * First we setup the header of the argument structure to identify that we are calling the
 * a function foo in the Midgard Base UKK client driver identified with the id BASE_UK_FOO_FUNC.
 *
@code
    base_uk_foo_args foo_args;
    foo_args.header.id = BASE_UK_FOO_FUNC;
@endcode
 *
 * Followed by the setup of the arguments for the function foo.
 *
@code
    foo_args.input_value = 48;
@endcode
 *
 * Then we use UKU to actually call the function in the Midgard Base UKK client driver.
 *
@code
    // call kernel-side foo function
    ret = uku_call(uku_ctx, &foo_args, sizeof(foo_args));
    if (MALI_ERROR_NONE == ret && MALI_ERROR_NONE == foo_args.header.ret)
    {
@endcode
 *
 * If the uku_call() function succeeded we can check the return code of the foo function. The return
 * value is returned in the ret field of the header. If it succeeded, we verify here that the
 * foo function indeed doubled the input value.
 *
@code
        // retrieve data returned by kernel-side foo function
        mali_tpi_printf("foo returned value %d\n", foo_args.output_value);

        // output_value should match input_value * 2
        if (foo_args.output_value != foo_args.input_value * 2)
        {
            // data didn't match: test fails
            ret = MALI_ERROR_FUNCTION_FAILED;
        }
    }
@endcode
 *
 * When we are done, we close the Midgard Base UKK client.
 *
@code
cleanup:
    // close an opened user-kernel context
    if (UKU_OPEN_OK == open_status)
    {
        uku_close(uku_ctx);
    }
@endcode
 * @{
 */

/**
 * User-side representation of a connection with a UKK client.
 * See uku_open for opening a connection, and uku_close for closing
 * a connection.
 */
/* Some compilers require a forward declaration of the structure */
struct uku_context;
typedef struct uku_context uku_context;

/**
 * Status returned from uku_open() as a result of trying to open a connection to a UKK client
 */
typedef enum uku_open_status
{
	UKU_OPEN_OK,           /**< UKK client opened succesfully and versions are compatible */
	UKU_OPEN_INCOMPATIBLE, /**< UKK client opened succesfully but versions are not compatible and the UKK client
	                            connection was closed. */
	UKU_OPEN_FAILED        /**< Could not open UKK client or UKK client failed to perform version check */
} uku_open_status;

/**
 * This structure carries a 16-bit major and minor number and is provided to a uku_open call
 * to identify the versions of the UK client of the user-side on input, and on output the
 * version of the UK client on the kernel-side. See uku_open.
 */
typedef struct uku_client_version
{
	/**
	 * 16-bit number identifying the major version. Interfaces with different major version numbers
	 * are incompatible. This field carries the user-side major version on input and the kernel-side
	 * major version on output.
	 */
	u16 major;
	/**
	 * 16-bit number identifying the minor version. A user-side interface minor version that is equal
	 * to or less than the kernel-side interface minor version is compatible. A user-side interface
	 * minor version that is greater than the kernel-side interface minor version is incompatible
	 * (as it is requesting more functionality than exists). This field carries the user-side minor
	 * version on input and the kernel-side minor version on output.
	 */
	u16 minor;
} uku_client_version;

/**
 * @brief Open a connection to a UKK client
 *
 * The User-Kernel interface communicates with a kernel-side driver over
 * an OS specific communication channel. A UKU context object stores the
 * necessary OS specific objects and state information to represent this
 * OS specific communication channel. A UKU context, defined by the
 * uku_context type is passed in as the first argument to nearly all
 * UKU functions. These UKU functions expect the UKU context to be valid,
 * an invalid UKU context will trigger a debug assert (in debug builds).
 *
 * The function uku_open() opens a connection to a kernel-side driver with
 * a User-Kernel interface, aka UKK client, and returns an initialized
 * UKU context. The function uku_close() closes the connection to the UKK
 * client.
 *
 * The kernel-side driver may support multiple instances and the particular
 * instance that needs to be opened is selected by the instance argument.
 * An instance of a kernel-side driver is normally associated with a particular
 * instance of a physical hardware block, e.g. each instance corresponds
 * to one of the ports of a UART controller. See the specifics of the
 * kernel-side driver to find out which instances are supported.
 *
 * As the user and kernel-side of the UK interface are physically two
 * different entities, they might end up using different versions of the
 * UK interface and therefore part of the opening process makes an
 * internal UK call to verify if the versions are compatible. A version
 * has a major and minor part. Interfaces with different major version
 * numbers are incompatible. A user-side interface minor version that is equal
 * to or less than the kernel-side interface minor version is compatible.
 * A user-side interface minor version that is greater than the kernel-side
 * interface minor version is incompatible (as it is requesting more
 * functionality than exists).
 *
 * Each UKK client has a unique id as defined by the uk_client_id
 * enumeration. These IDs are mapped to OS specific device file names
 * that refer to their respective kernel device drivers. Any new UKK client
 * needs to be added to the uk_client_id enumeration.
 *
 * A UKU context must be shareable between threads. It may be shareable
 * between processes. This attribute is mostly dependent on the OS specific
 * communication channel used to communicate with the UKK client. When
 * multiple threads use the same UKU context only one should be responsible
 * for closing it and ensuring the other threads are not using it anymore.
 *
 * Opening a UKU context is considered to be an expensive operation, most
 * likely resulting in loading a kernel device driver when opened for
 * the first time in a system. The specific kernel device driver to be
 * opened is defined by the OS specific implementation of this function and
 * is not configurable.
 *
 * Once a UKU context is opened and in use by the user-kernel interface it
 * is expected to operate without error. Any communication error will not
 * result in an attempt by the user-kernel interface implementation to
 * re-establish the OS specific communication channel and is considered
 * to be a non-recoverable fault.
 *
 * Notes on Base driver context and UKU context
 *
 * A UKU context is opened each time a base driver context is created.
 * A UKU context and base driver context therefore have a 1:1 relationship.
 * A base driver context represents an isolated GPU address space and because
 * of the 1:1 relationship with a UKU context, a UKU context can also be seen
 * to present an isolated GPU address space. Each process is currently
 * expected to create one base driver context (and therefore open one UKU
 * context per process), but this might change, having multiple base driver
 * contexts open per process, in case we need to support WebGL, where each
 * GLES context must use a separate GPU address space inside the web browser
 * to prevent seperate brower tabs from interfering with each other.
 *
 * Debug builds will assert when a NULL pointer is passed for the version or
 * uku_ctx parameters, or when an unknown enumerated value for the id parameter
 * is used.
 *
 * @param[in] id UKK client identifier, see uk_client_id.
 * @param[in] instance instance number (0..) of the UKK client driver
 * @param[in,out] version of the UKU client on input. On output it contains the version
 * of the UKK client, when uku_open returns UKU_OPEN_OK or UKU_OPEN_INCOMPATIBLE,
 * otherwise the output value is undefined.
 * @param[out] uku_ctx Pointer to User-Kernel context to initialize
 * @return UKU_OPEN_OK when the connection to the UKK client was successful.
 * @return UKU_OPEN_FAILED when the connection to the UKK client could not be established,
 * or the UKK client failed to perform version verification.
 * @return UKU_OPEN_INCOMPATIBLE when the version of the UKK and UKU clients are incompatible.
 */
uku_open_status uku_open(uk_client_id id, u32 instance, uku_client_version *version, uku_context *uku_ctx) CHECK_RESULT;

/**
 * @brief Returns OS specific driver context from UKU context
 *
 * The UKU context abstracts the connection to a kernel-side driver. If OS specific code
 * needs to communicate with this kernel-side driver directly, it can use this function
 * to retrieve the OS specific object hidden by the UKU context. This object must only
 * be used while the UKU context is open and must only be used by OS specific code.
 *
 * Debug builds will assert when a NULL pointer is passed for uku_ctx.
 *
 * @param[in] uku_ctx Pointer to a valid User-Kernel context. See uku_open.
 * @return OS specific driver context, e.g. for Linux this would be an integer
 * representing a file descriptor.
 *
 */
void *uku_driver_context(uku_context *uku_ctx) CHECK_RESULT;

/**
 * @brief Closes a connection to a UKK client
 *
 * Closes a previously opened connection to a kernel-side driver with a
 * User-Kernel interface, aka UKK client. The UKU context uku_ctx
 * has now become invalid.
 *
 * Before calling this function, any UKU function using the UKU context
 * uku_ctx must have finished. The UKU context uku_ctx must not be in
 * use.
 *
 * Debug builds will assert when a NULL pointer is passed for uku_ctx.
 *
 * @param[in] uku_ctx Pointer to a valid User-Kernel context. See uku_open.
 */
void uku_close(uku_context * const uku_ctx);

/**
 * @brief Calls a function in a UKK client
 *
 * A UKK client defines a structure for each function callable over the UK
 * interface. The structure starts with a header field of type uk_header,
 * followed by the arguments for the function, e.g.
 *
 * @code
 *     typedef struct base_uk_foo_args
 *     {
 *         uk_header header; // first member is the header
 *         int n;            // followed by function arguments
 *         int doubled_n;
 *         ...
 *     } base_uk_foo_args;
 * @endcode
 *
 * The header.id field identifies the function to be called. See the UKK
 * client documentation for a list of available functions and the structure
 * definitions associated with them.
 *
 * The arguments in the structure can be of type input, input/output or
 * output. All input and input/output arguments must be initialized in the
 * structure before calling the uku_call() function. Memory pointed to by
 * pointers in the structure should at least remain allocated until uku_call
 * returns.
 *
 * When uku_call has successfully executed the function in the UKK client,
 * it stores the return code of the function in the header.ret field, and
 * only in this case the output and input/output members are considered to
 * be valid in the structure.
 *
 * For example, to call function 'foo' which simply doubles the supplied
 * argument 'n':
 * @code
 *      base_uk_foo_args args;
 *      mali_error ret;
 *
 *      args.header.id = BASE_UK_FOO_FUNC;
 *      args.n = 10;
 *
 *      ret = uku_call(uku_ctx, &args, sizeof(args));
 *      if (MALI_ERROR_NONE == ret)
 *      {
 *          if (MALI_ERROR_NONE == args.header.ret)
 *          {
 *               printf("%d*%d=%d\n", args.n, args.n, args.doubled_n)
 *          }
 *      }
 * @endcode
 *
 * Debug builds will assert when a NULL pointer is passed for uku_ctx or
 * args, or args_size < sizeof(uku_header).
 *
 * @param[in] uku_ctx   Pointer to a valid User-Kernel context. See uku_open.
 * @param[in,out] args  Pointer to an argument structure associated with
 *                      the function to be called in the UKK client.
 * @param[in] args_size Size of the argument structure in bytes
 * @return MALI_ERROR_NONE on success. Any other value indicates failure,
 * and the structure pointed to by args may contain invalid information.
 */
mali_error uku_call(uku_context *uku_ctx, void *args, u32 args_size) CHECK_RESULT;


/** @} end group uk_api_user */

/** @} end group uk_api */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _UKU_H_ */
