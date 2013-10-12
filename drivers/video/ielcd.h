/* linux/drivers/video/ielcd.h
 *
 * Header file for Samsung (IELCD) driver
 *
*/

#ifndef _S3CFB_IELCD_H
#define _S3CFB_IELCD_H

int s3c_fimd1_display_on(void);
int s3c_fimd1_display_off(void);
int s3c_ielcd_display_on(void);
int s3c_ielcd_display_off(void);
int s3c_ielcd_setup(void);
int s3c_ielcd_hw_init(void);
#ifdef CONFIG_FB_I80IF
void s3c_ielcd_fimd_instance_off(void);
void s3c_ielcd_hw_trigger_set(void);
void s3c_ielcd_hw_trigger_set_of_start(void);
void ielcd_cmd_clear(void);
void ielcd_cmd_set(u32 idx, u32 rs, u32 value);
int ielcd_cmd_start_status(void);
void ielcd_cmd_trigger(void);
void ielcd_cmd_dump(void);
#endif

#endif
