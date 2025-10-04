; master library
; Description:
;	パターンデータを待避したEMSのハンドル
;
; Variables:
;	unsigned super_backup_ems_handle = 0 ;
;	unsigned long super_backup_ems_pos ;
;
; Revision History:
;	93/12/19 Initial: superems.asm/master.lib 0.22
;

	.MODEL SMALL
	.DATA
	public super_backup_ems_handle,_super_backup_ems_handle
_super_backup_ems_handle label word
super_backup_ems_handle dw 0

	.DATA?
	public super_backup_ems_pos,_super_backup_ems_pos
_super_backup_ems_pos label dword
super_backup_ems_pos dd ?

END
