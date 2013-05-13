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

#endif
