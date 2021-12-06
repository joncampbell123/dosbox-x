#ifndef	_mach_eventlink_user_
#define	_mach_eventlink_user_

/* Module mach_eventlink */

#include <string.h>
#include <mach/ndr.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/notify.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/port.h>
	
/* BEGIN MIG_STRNCPY_ZEROFILL CODE */

#if defined(__has_include)
#if __has_include(<mach/mig_strncpy_zerofill_support.h>)
#ifndef USING_MIG_STRNCPY_ZEROFILL
#define USING_MIG_STRNCPY_ZEROFILL
#endif
#ifndef __MIG_STRNCPY_ZEROFILL_FORWARD_TYPE_DECLS__
#define __MIG_STRNCPY_ZEROFILL_FORWARD_TYPE_DECLS__
#ifdef __cplusplus
extern "C" {
#endif
	extern int mig_strncpy_zerofill(char *dest, const char *src, int len) __attribute__((weak_import));
#ifdef __cplusplus
}
#endif
#endif /* __MIG_STRNCPY_ZEROFILL_FORWARD_TYPE_DECLS__ */
#endif /* __has_include(<mach/mig_strncpy_zerofill_support.h>) */
#endif /* __has_include */
	
/* END MIG_STRNCPY_ZEROFILL CODE */


#ifdef AUTOTEST
#ifndef FUNCTION_PTR_T
#define FUNCTION_PTR_T
typedef void (*function_ptr_t)(mach_port_t, char *, mach_msg_type_number_t);
typedef struct {
        char            *name;
        function_ptr_t  function;
} function_table_entry;
typedef function_table_entry   *function_table_t;
#endif /* FUNCTION_PTR_T */
#endif /* AUTOTEST */

#ifndef	mach_eventlink_MSG_COUNT
#define	mach_eventlink_MSG_COUNT	4
#endif	/* mach_eventlink_MSG_COUNT */

#include <mach/mach_eventlink_types.h>
#include <mach/std_types.h>
#include <mach/mig.h>
#include <mach/mig.h>
#include <mach/mach_types.h>
#include <mach/mach_types.h>

#ifdef __BeforeMigUserHeader
__BeforeMigUserHeader
#endif /* __BeforeMigUserHeader */

#include <sys/cdefs.h>
__BEGIN_DECLS


/* Routine mach_eventlink_create */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_eventlink_create
(
	task_t task,
	mach_eventlink_create_option_t option,
	eventlink_port_pair_t eventlink_pair
);

/* Routine mach_eventlink_destroy */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_eventlink_destroy
(
	mach_port_t eventlink
);

/* Routine mach_eventlink_associate */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_eventlink_associate
(
	mach_port_t eventlink,
	thread_t thread,
	mach_vm_address_t copyin_addr_wait,
	uint64_t copyin_mask_wait,
	mach_vm_address_t copyin_addr_signal,
	uint64_t copyin_mask_signal,
	mach_eventlink_associate_option_t option
);

/* Routine mach_eventlink_disassociate */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_eventlink_disassociate
(
	mach_port_t eventlink,
	mach_eventlink_disassociate_option_t option
);

__END_DECLS

/********************** Caution **************************/
/* The following data types should be used to calculate  */
/* maximum message sizes only. The actual message may be */
/* smaller, and the position of the arguments within the */
/* message layout may vary from what is presented here.  */
/* For example, if any of the arguments are variable-    */
/* sized, and less than the maximum is sent, the data    */
/* will be packed tight in the actual message to reduce  */
/* the presence of holes.                                */
/********************** Caution **************************/

/* typedefs for all requests */

#ifndef __Request__mach_eventlink_subsystem__defined
#define __Request__mach_eventlink_subsystem__defined

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		mach_eventlink_create_option_t option;
	} __Request__mach_eventlink_create_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
	} __Request__mach_eventlink_destroy_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t thread;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t copyin_addr_wait;
		uint64_t copyin_mask_wait;
		mach_vm_address_t copyin_addr_signal;
		uint64_t copyin_mask_signal;
		mach_eventlink_associate_option_t option;
	} __Request__mach_eventlink_associate_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		mach_eventlink_disassociate_option_t option;
	} __Request__mach_eventlink_disassociate_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif
#endif /* !__Request__mach_eventlink_subsystem__defined */

/* union of all requests */

#ifndef __RequestUnion__mach_eventlink_subsystem__defined
#define __RequestUnion__mach_eventlink_subsystem__defined
union __RequestUnion__mach_eventlink_subsystem {
	__Request__mach_eventlink_create_t Request_mach_eventlink_create;
	__Request__mach_eventlink_destroy_t Request_mach_eventlink_destroy;
	__Request__mach_eventlink_associate_t Request_mach_eventlink_associate;
	__Request__mach_eventlink_disassociate_t Request_mach_eventlink_disassociate;
};
#endif /* !__RequestUnion__mach_eventlink_subsystem__defined */
/* typedefs for all replies */

#ifndef __Reply__mach_eventlink_subsystem__defined
#define __Reply__mach_eventlink_subsystem__defined

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t eventlink_pair[2];
		/* end of the kernel processed data */
	} __Reply__mach_eventlink_create_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_eventlink_destroy_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_eventlink_associate_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif

#ifdef  __MigPackStructs
#pragma pack(push, 4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_eventlink_disassociate_t __attribute__((unused));
#ifdef  __MigPackStructs
#pragma pack(pop)
#endif
#endif /* !__Reply__mach_eventlink_subsystem__defined */

/* union of all replies */

#ifndef __ReplyUnion__mach_eventlink_subsystem__defined
#define __ReplyUnion__mach_eventlink_subsystem__defined
union __ReplyUnion__mach_eventlink_subsystem {
	__Reply__mach_eventlink_create_t Reply_mach_eventlink_create;
	__Reply__mach_eventlink_destroy_t Reply_mach_eventlink_destroy;
	__Reply__mach_eventlink_associate_t Reply_mach_eventlink_associate;
	__Reply__mach_eventlink_disassociate_t Reply_mach_eventlink_disassociate;
};
#endif /* !__RequestUnion__mach_eventlink_subsystem__defined */

#ifndef subsystem_to_name_map_mach_eventlink
#define subsystem_to_name_map_mach_eventlink \
    { "mach_eventlink_create", 716200 },\
    { "mach_eventlink_destroy", 716201 },\
    { "mach_eventlink_associate", 716202 },\
    { "mach_eventlink_disassociate", 716203 }
#endif

#ifdef __AfterMigUserHeader
__AfterMigUserHeader
#endif /* __AfterMigUserHeader */

#endif	 /* _mach_eventlink_user_ */
